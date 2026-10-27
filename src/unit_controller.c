/*
 * Estado lógico de las unidades y sus órdenes. Este módulo mantiene sincronizados
 * posiciones, ocupación de celdas, movimiento y eventos; no dibuja sprites.
 */
#include "unit_controller.h"
#include <string.h>
#include "cell_events.h"
#include "game_config.h"

/*
 * Fuente autoritativa de unidades y órdenes: selección, alcance, rutas y animación lógica.
 * La ocupación se actualiza en los límites de celda y el sprite se sincroniza desde UnitView.
 */

#define UNIT_ID_MAX        127
#define NO_SELECTED_UNIT   ((s16)-1)
#define PATH_COST_MAX      254

#if (UNIT_MOVE_SPEED_PIXELS <= 0) || ((CELL_PIXELS % UNIT_MOVE_SPEED_PIXELS) != 0)
#error "UNIT_MOVE_SPEED_PIXELS debe dividir CELL_PIXELS exactamente para animar sin saltos"
#endif

/* Búsqueda de unidades y ocupación segura con niebla. */

static u8 resolveMovementPoints(const UnitStart *start)
{
    const UnitTypeInfo *typeInfo = UnitType_getInfo(start->type);

    if (start->movementPointsPerTurn != 0) return start->movementPointsPerTurn;
    return (typeInfo != NULL) ? typeInfo->defaultMovementPoints : 0;
}

static s16 findUnitIndex(const UnitController *controller, u8 id)
{
    u8 i;

    for (i = 0; i < controller->count; i++)
        if (controller->units[i].id == id) return (s16)i;

    return NO_SELECTED_UNIT;
}

/* Un enemigo fuera de visión existe en el mapa real, pero no en la
   información de navegación que recibe el jugador activo. */
static bool isHiddenEnemyUnit(const UnitController *controller,
                              const Unit *unit)
{
    if ((controller == NULL) || (unit == NULL) || !controller->fogEnabled ||
        !unit->alive || (unit->team == controller->activeTeam))
        return FALSE;

    return MapData_getVisionState(controller->map,
        (u8)controller->activeTeam, unit->x, unit->y) != MAP_VISION_VISIBLE;
}

static bool isHiddenEnemyAt(const UnitController *controller, u16 x, u16 y)
{
    const CellData *cell;
    s16 index;

    if ((controller == NULL) || (controller->map == NULL)) return FALSE;
    cell = MapData_getCellConst(controller->map, x, y);
    if ((cell == NULL) || (cell->occupantId == CELL_NO_OCCUPANT)) return FALSE;
    index = findUnitIndex(controller, (u8)cell->occupantId);
    if (index == NO_SELECTED_UNIT) return FALSE;
    return isHiddenEnemyUnit(controller, &controller->units[index]);
}

/* Pathfinder consulta la ocupación lógica directamente. Para construir el
   alcance conocido, ocultamos temporalmente solo los enemigos no visibles;
   se restaura la ocupación inmediatamente al volver de la búsqueda. No se
   despachan eventos ni se cambia el estado permanente del mapa. */
static void maskHiddenEnemyOccupants(UnitController *controller)
{
    u8 i;

    if ((controller == NULL) || !controller->fogEnabled) return;
    for (i = 0; i < controller->count; i++)
    {
        Unit *unit = &controller->units[i];
        CellData *cell;

        if (!isHiddenEnemyUnit(controller, unit)) continue;
        cell = MapData_getCell(controller->map, unit->x, unit->y);
        if ((cell != NULL) && (cell->occupantId == (s8)unit->id))
            cell->occupantId = CELL_NO_OCCUPANT;
    }
}

static void restoreHiddenEnemyOccupants(UnitController *controller)
{
    u8 i;

    if ((controller == NULL) || !controller->fogEnabled) return;
    for (i = 0; i < controller->count; i++)
    {
        const Unit *unit = &controller->units[i];
        CellData *cell;

        if (!isHiddenEnemyUnit(controller, unit)) continue;
        cell = MapData_getCell(controller->map, unit->x, unit->y);
        if ((cell != NULL) && (cell->occupantId == CELL_NO_OCCUPANT))
            cell->occupantId = (s8)unit->id;
    }
}

/* Estado de planificación y caché del alcance de movimiento. */

static void clearPreview(UnitController *controller)
{
    controller->previewChecked = FALSE;
    controller->previewRelevant = FALSE;
    controller->previewReachable = FALSE;
    controller->previewLength = 0;
    controller->previewCost = 0;
}

static void clearPendingRoute(UnitController *controller)
{
    controller->pendingRoute = FALSE;
    controller->pendingX = 0;
    controller->pendingY = 0;
}

static void clearPlanningState(UnitController *controller)
{
    clearPreview(controller);
    clearPendingRoute(controller);
}

void UnitController_invalidateMovementRange(UnitController *controller)
{
    if (controller == NULL) return;
    controller->movementRangeValid = FALSE;
    controller->movementRangeUnitIndex = NO_SELECTED_UNIT;
}

static bool movementRangeMatchesSelection(const UnitController *controller)
{
    const Unit *unit;

    if ((controller == NULL) || !controller->movementRangeValid ||
        controller->moving || (controller->selectedIndex < 0) ||
        (controller->selectedIndex >= controller->count))
        return FALSE;

    unit = &controller->units[controller->selectedIndex];
    return (unit->team == controller->activeTeam) &&
        (controller->movementRangeUnitIndex == controller->selectedIndex) &&
        (controller->movementRangeUnitX == unit->x) &&
        (controller->movementRangeUnitY == unit->y) &&
        (controller->movementRangePoints == unit->movementPoints);
}

/*
 * Consulta exclusiva de la caché de alcance. Nunca inicia una búsqueda:
 * el alcance se construye explícitamente al seleccionar o refrescar la unidad.
 * El cursor puede llamar esta función sin ejecutar ninguna búsqueda Pathfinder.
 */
static bool getMovementRangeInfo(UnitController *controller,
                                 const Unit *unit, u16 targetX, u16 targetY,
                                 u16 *stepCount, u8 *totalCost)
{
    if ((controller->pathWorkspace->reachable == NULL) ||
        !movementRangeMatchesSelection(controller))
        return FALSE;

    /*
     * getReachableInfo solo consulta reachable/costs ya calculados; no explora
     * ocupantes. Enmascarar/restaurar enemigos aquí hacía dos barridos completos
     * del ejército cada vez que el cursor cambiaba de casilla, sin afectar el
     * resultado de la consulta.
     */
    return Pathfinder_getReachableInfo(
        controller->map, controller->pathWorkspace,
        unit->x, unit->y, targetX, targetY, unit->movementPoints,
        stepCount, totalCost);
}

/* Detiene la animación y conserva la selección para permitir otra ruta. */
/* Movimiento por celdas y validación de destinos. */

static void stopMovement(UnitController *controller)
{
    const s16 stoppedIndex = controller->movingIndex;
    bool canReplanFromOrigin = FALSE;

    /* Si una ruta se interrumpe después de llegar a una o más celdas,
       el movimiento del turno ya se ha consumido. Solo se conserva la acción
       si el fallo ocurrió antes de que la unidad abandonara su origen lógico. */
    if (controller->moving && (stoppedIndex >= 0) &&
        (stoppedIndex < controller->count))
    {
        Unit *unit = &controller->units[stoppedIndex];

        canReplanFromOrigin = unit->alive &&
            (unit->team == controller->activeTeam) &&
            (unit->movementPoints > 0) &&
            (unit->x == controller->movementStartX) &&
            (unit->y == controller->movementStartY) &&
            (controller->selectedIndex == stoppedIndex);

        if (unit->alive &&
            ((unit->x != controller->movementStartX) ||
             (unit->y != controller->movementStartY)))
            unit->movementPoints = 0;
    }

    controller->moving = FALSE;
    controller->movingIndex = NO_SELECTED_UNIT;
    controller->pathLength = 0;
    controller->pathStep = 0;
    controller->segmentPixels = 0;
    clearPlanningState(controller);
    UnitController_invalidateMovementRange(controller);

    /* El bloqueo antes de la primera celda mantiene disponible la acción y
       reconstruye el alcance para poder elegir otra ruta sin reseleccionar. */
    if (canReplanFromOrigin)
        (void)UnitController_refreshMovementRange(controller);
}

/* Una orden completada libera también la selección y su alcance visual. */
static void finishMovement(UnitController *controller)
{
    /* Una orden de movimiento consume la accion de movimiento del turno,
       aunque el camino cueste menos que el presupuesto inicial. */
    if ((controller->movingIndex >= 0) &&
        (controller->movingIndex < controller->count))
    {
        controller->units[controller->movingIndex].movementPoints = 0;
    }

    stopMovement(controller);
    controller->selectedIndex = NO_SELECTED_UNIT;
}

static bool getNeighbour(const MapData *map, u16 x, u16 y,
                         PathDirection direction, u16 *nextX, u16 *nextY)
{
    s32 candidateX = x;
    s32 candidateY = y;

    if ((map == NULL) || (nextX == NULL) || (nextY == NULL)) return FALSE;

    switch (direction)
    {
        case PATH_DIR_UP:    candidateY--; break;
        case PATH_DIR_RIGHT: candidateX++; break;
        case PATH_DIR_DOWN:  candidateY++; break;
        case PATH_DIR_LEFT:  candidateX--; break;
        default: return FALSE;
    }

    if ((candidateX < 0) || (candidateY < 0) ||
        (candidateX >= map->width) || (candidateY >= map->height))
        return FALSE;

    *nextX = (u16)candidateX;
    *nextY = (u16)candidateY;
    return TRUE;
}

static bool canMoveTo(const UnitController *controller, const Unit *unit,
                      u16 nextX, u16 nextY)
{
    const CellData *origin = MapData_getCellConst(controller->map,
                                                   unit->x, unit->y);
    const CellData *destination = MapData_getCellConst(controller->map,
                                                        nextX, nextY);
    const u8 moveCost = (destination != NULL)
        ? UnitType_getTerrainMoveCost(unit->type, destination->terrain) : 0;

    return (origin != NULL) && (origin->occupantId == (s8)unit->id) &&
           MapData_canEnter(controller->map, nextX, nextY) &&
           (moveCost != 0) && (moveCost <= unit->movementPoints);
}

/*
 * Confirma la transición en el centro de la casilla. Primero valida ambos
 * extremos; después despacha los eventos en su punto lógico de salida/entrada.
 * Los callbacks son notificadores y no deben alterar terreno u ocupación.
 */
static bool commitStep(UnitController *controller, Unit *unit,
                       u16 nextX, u16 nextY)
{
    const u16 oldX = unit->x;
    const u16 oldY = unit->y;
    CellData *origin;
    CellData *destination;
    u8 moveCost;

    if (!canMoveTo(controller, unit, nextX, nextY)) return FALSE;

    origin = MapData_getCell(controller->map, oldX, oldY);
    destination = MapData_getCell(controller->map, nextX, nextY);
    if ((origin == NULL) || (destination == NULL)) return FALSE;
    moveCost = UnitType_getTerrainMoveCost(unit->type, destination->terrain);
    if ((moveCost == 0) || (moveCost > unit->movementPoints)) return FALSE;

    /* Los callbacks notifican una transición ya validada; no pueden vetarla. */
    CellEvents_dispatchAtForTeam(controller->map, oldX, oldY,
        CELL_EVENT_BEFORE_LEAVE, nextX, nextY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);
    CellEvents_dispatchAtForTeam(controller->map, oldX, oldY,
        CELL_EVENT_LEAVE, nextX, nextY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);

    /* leave ve el origen ocupado; la ocupación cambia entre leave y beforeEnter. */
    origin->occupantId = CELL_NO_OCCUPANT;
    CellEvents_dispatchAtForTeam(controller->map, nextX, nextY,
        CELL_EVENT_BEFORE_ENTER, oldX, oldY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);

    destination->occupantId = (s8)unit->id;
    unit->x = nextX;
    unit->y = nextY;
    unit->movementPoints = (u8)(unit->movementPoints - moveCost);
    CellEvents_dispatchAtForTeam(controller->map, nextX, nextY,
        CELL_EVENT_ENTER, oldX, oldY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);
    CellEvents_dispatchAtForTeam(controller->map, oldX, oldY,
        CELL_EVENT_UNIT_LEAVE, nextX, nextY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);
    CellEvents_dispatchAtForTeam(controller->map, nextX, nextY,
        CELL_EVENT_UNIT_ENTER, oldX, oldY, CELL_EVENT_SOURCE_UNIT,
        (u8)unit->team, controller->turnNumber);
    return TRUE;
}

/* API pública: despliegue, órdenes y transiciones de turno. */

bool UnitController_init(UnitController *controller, MapData *map,
                         Unit *storage, u8 capacity,
                         const UnitStart *starts, u8 count)
{
    u8 i;
    u8 j;
    u32 requiredCells;

    if ((controller == NULL) || (map == NULL) ||
        ((count > 0) && ((storage == NULL) || (starts == NULL))) ||
        (capacity < count) || (map->pathWorkspace == NULL) ||
        (map->pathWorkspace->costs == NULL) ||
        (map->pathWorkspace->parentDirections == NULL) ||
        (map->pathWorkspace->reachable == NULL))
        return FALSE;

    requiredCells = (u32)map->width * map->height;
    if ((requiredCells == 0) || (map->cellCapacity < requiredCells) ||
        (map->pathWorkspace->capacity < requiredCells))
        return FALSE;

    /* Valida todo el despliegue antes de ocupar ninguna casilla. */
    for (i = 0; i < count; i++)
    {
        const CellData *cell;

        const UnitTypeInfo *typeInfo = UnitType_getInfo(starts[i].type);
        const u8 movementPoints = resolveMovementPoints(&starts[i]);

        if ((starts[i].id > UNIT_ID_MAX) || (typeInfo == NULL) ||
            (typeInfo->maxHealth == 0) || (typeInfo->attack == 0) ||
            (typeInfo->attackRange == 0) || (typeInfo->visionRange == 0) ||
            ((typeInfo->repairRange == 0) != (typeInfo->repairAmount == 0)) ||
            (movementPoints == 0) || (movementPoints > PATH_COST_MAX) ||
            ((u8)starts[i].team >= TEAM_COUNT) ||
            !MapData_isPassable(map, starts[i].x, starts[i].y))
            return FALSE;

        cell = MapData_getCellConst(map, starts[i].x, starts[i].y);
        if ((cell == NULL) || (cell->occupantId != CELL_NO_OCCUPANT))
            return FALSE;

        for (j = 0; j < i; j++)
            if ((starts[i].id == starts[j].id) ||
                ((starts[i].x == starts[j].x) &&
                 (starts[i].y == starts[j].y)))
                return FALSE;
    }

    controller->map = map;
    controller->pathWorkspace = map->pathWorkspace;
    controller->units = storage;
    controller->count = count;
    controller->selectedIndex = NO_SELECTED_UNIT;
    controller->movingIndex = NO_SELECTED_UNIT;
    controller->moving = FALSE;
    controller->activeTeam = TEAM_BLUE;
    controller->fogEnabled = FALSE;
    controller->turnNumber = 1;
    memset(controller->lastStartedTurnByTeam, 0,
           sizeof(controller->lastStartedTurnByTeam));
    controller->movementStartX = 0;
    controller->movementStartY = 0;
    controller->pathLength = 0;
    controller->pathStep = 0;
    controller->segmentPixels = 0;
    controller->movementRangeValid = FALSE;
    controller->movementRangeUnitIndex = NO_SELECTED_UNIT;
    controller->movementRangeUnitX = 0;
    controller->movementRangeUnitY = 0;
    controller->movementRangePoints = 0;
    clearPlanningState(controller);

    for (i = 0; i < count; i++)
    {
        storage[i].id = starts[i].id;
        storage[i].x = starts[i].x;
        storage[i].y = starts[i].y;
        storage[i].maxMovementPoints = resolveMovementPoints(&starts[i]);
        storage[i].movementPoints = (starts[i].team == TEAM_BLUE)
            ? storage[i].maxMovementPoints : 0;
        storage[i].type = starts[i].type;
        storage[i].team = starts[i].team;
        {
            const UnitTypeInfo *info = UnitType_getInfo(starts[i].type);
            storage[i].maxHealth = info->maxHealth;
            storage[i].health = info->maxHealth;
            storage[i].attack = info->attack;
            storage[i].defense = info->defense;
            storage[i].attackRange = info->attackRange;
            storage[i].visionRange = info->visionRange;
            storage[i].repairRange = info->repairRange;
            storage[i].repairAmount = info->repairAmount;
        }
        storage[i].hasAttackedThisTurn = FALSE;
        storage[i].hasRepairedThisTurn = FALSE;
        storage[i].alive = TRUE;
        (void)MapData_setOccupant(map, starts[i].x, starts[i].y,
                                  (s8)starts[i].id);
    }

    return TRUE;
}

UnitActionResult UnitController_activate(UnitController *controller,
                                         u16 x, u16 y)
{
    CellData *target;
    Unit *unit;
    s16 targetIndex;
    u16 pathLength;
    u8 routeCost;

    if ((controller == NULL) || (controller->map == NULL))
        return UNIT_ACTION_INVALID;
    if (controller->moving) return UNIT_ACTION_MOVING;

    target = MapData_getCell(controller->map, x, y);
    if (target == NULL)
    {
        clearPlanningState(controller);
        return UNIT_ACTION_INVALID;
    }

    if ((target->occupantId != CELL_NO_OCCUPANT) &&
        !isHiddenEnemyAt(controller, x, y))
    {
        targetIndex = findUnitIndex(controller, (u8)target->occupantId);
        if (targetIndex == NO_SELECTED_UNIT)
        {
            clearPlanningState(controller);
            return UNIT_ACTION_BLOCKED;
        }
        if (controller->units[targetIndex].team != controller->activeTeam)
        {
            clearPlanningState(controller);
            return UNIT_ACTION_ENEMY_UNIT;
        }
        if (targetIndex == controller->selectedIndex)
        {
            controller->selectedIndex = NO_SELECTED_UNIT;
            clearPlanningState(controller);
            UnitController_invalidateMovementRange(controller);
            return UNIT_ACTION_DESELECTED;
        }

        controller->selectedIndex = targetIndex;
        clearPlanningState(controller);
        UnitController_invalidateMovementRange(controller);

        /* Calcula el alcance una vez al seleccionar, nunca durante el hover. */
        if (!UnitController_refreshMovementRange(controller))
        {
            controller->selectedIndex = NO_SELECTED_UNIT;
            clearPlanningState(controller);
            UnitController_invalidateMovementRange(controller);
            return UNIT_ACTION_INVALID;
        }
        return UNIT_ACTION_SELECTED;
    }

    if (controller->selectedIndex == NO_SELECTED_UNIT)
        return UNIT_ACTION_NO_UNIT;

    unit = &controller->units[controller->selectedIndex];
    if (unit->movementPoints == 0)
    {
        clearPlanningState(controller);
        return UNIT_ACTION_NO_MOVEMENT_POINTS;
    }
    /* Confirmar ejecuta una sola busqueda sobre el mapa actual. La primera A
       consulta el alcance ya calculado y no vuelve a ejecutar la búsqueda ponderada de ruta. */
    if (controller->pendingRoute && (controller->pendingX == x) &&
        (controller->pendingY == y))
    {
        UnitController_invalidateMovementRange(controller);
        bool routeFound;

        maskHiddenEnemyOccupants(controller);
        routeFound = Pathfinder_findRouteForType(
            controller->map, controller->pathWorkspace,
            unit->x, unit->y, x, y, (s8)unit->id, unit->type,
            unit->movementPoints, &pathLength, &routeCost);
        restoreHiddenEnemyOccupants(controller);
        if (!routeFound || (pathLength == 0))
        {
            clearPlanningState(controller);
            /* La búsqueda de ruta ha usado costs/parents; restaura el alcance
               aquí, en el fallo de confirmación, no al mover el cursor. */
            (void)UnitController_refreshMovementRange(controller);
            return UNIT_ACTION_NO_ROUTE;
        }

        clearPendingRoute(controller);
        controller->movingIndex = controller->selectedIndex;
        controller->movementStartX = unit->x;
        controller->movementStartY = unit->y;
        controller->pathLength = pathLength;
        controller->pathStep = 0;
        controller->segmentPixels = 0;
        controller->moving = TRUE;
        clearPreview(controller);
        return UNIT_ACTION_MOVING;
    }

    if (!getMovementRangeInfo(controller, unit, x, y,
                              &pathLength, &routeCost) || (pathLength == 0))
    {
        clearPlanningState(controller);
        return UNIT_ACTION_NO_ROUTE;
    }

    controller->pendingRoute = TRUE;
    controller->pendingX = x;
    controller->pendingY = y;
    controller->previewChecked = TRUE;
    controller->previewRelevant = TRUE;
    controller->previewReachable = TRUE;
    controller->previewX = x;
    controller->previewY = y;
    controller->previewLength = pathLength;
    controller->previewCost = routeCost;
    return UNIT_ACTION_ROUTE_READY;
}

UnitActionResult UnitController_update(UnitController *controller)
{
    Unit *unit;
    PathDirection direction;
    u16 nextX;
    u16 nextY;
    u16 progress;

    if ((controller == NULL) || !controller->moving ||
        (controller->movingIndex < 0) ||
        (controller->movingIndex >= controller->count))
        return UNIT_ACTION_INVALID;

    unit = &controller->units[controller->movingIndex];
    direction = Pathfinder_getStep(controller->pathWorkspace,
                                   controller->pathLength,
                                   controller->pathStep);
    if (!getNeighbour(controller->map, unit->x, unit->y, direction,
                      &nextX, &nextY))
    {
        stopMovement(controller);
        return UNIT_ACTION_BLOCKED;
    }

    /* Nunca empieza un tramo si la ocupación ya no coincide con el mapa. */
    if ((controller->segmentPixels == 0) &&
        !canMoveTo(controller, unit, nextX, nextY))
    {
        const UnitActionResult result = (unit->movementPoints == 0)
            ? UNIT_ACTION_NO_MOVEMENT_POINTS : UNIT_ACTION_BLOCKED;
        stopMovement(controller);
        return result;
    }

    progress = (u16)controller->segmentPixels + UNIT_MOVE_SPEED_PIXELS;
    if (progress < CELL_PIXELS)
    {
        controller->segmentPixels = (u8)progress;
        return UNIT_ACTION_MOVING;
    }

    /* El sprite llega al centro antes de confirmar el cambio lógico. */
    controller->segmentPixels = CELL_PIXELS;
    if (!commitStep(controller, unit, nextX, nextY))
    {
        const UnitActionResult result = (unit->movementPoints == 0)
            ? UNIT_ACTION_NO_MOVEMENT_POINTS : UNIT_ACTION_BLOCKED;
        stopMovement(controller);
        return result;
    }

    controller->pathStep++;
    controller->segmentPixels = 0;
    if (controller->pathStep >= controller->pathLength)
    {
        finishMovement(controller);
        return UNIT_ACTION_MOVED;
    }

    return UNIT_ACTION_MOVING;
}

UnitActionResult UnitController_cancelOrder(UnitController *controller)
{
    if ((controller == NULL) || (controller->map == NULL))
        return UNIT_ACTION_INVALID;
    if (controller->moving) return UNIT_ACTION_MOVING;
    if (controller->selectedIndex == NO_SELECTED_UNIT)
        return UNIT_ACTION_NO_UNIT;

    controller->selectedIndex = NO_SELECTED_UNIT;
    clearPlanningState(controller);
    UnitController_invalidateMovementRange(controller);
    return UNIT_ACTION_CANCELLED;
}

bool UnitController_beginTeamTurn(UnitController *controller, TeamId team,
                                  u16 turnNumber)
{
    u8 i;

    if ((controller == NULL) || controller->moving ||
        ((u8)team >= TEAM_COUNT) || (turnNumber == 0))
        return FALSE;

    /* No se puede volver a abrir un turno ya iniciado para ese equipo,
       aunque entre ambas llamadas se haya activado el turno del rival. */
    if (turnNumber <= controller->lastStartedTurnByTeam[team])
        return FALSE;

    controller->activeTeam = team;
    controller->turnNumber = turnNumber;
    controller->lastStartedTurnByTeam[team] = turnNumber;
    controller->selectedIndex = NO_SELECTED_UNIT;
    clearPlanningState(controller);
    UnitController_invalidateMovementRange(controller);
    for (i = 0; i < controller->count; i++)
    {
        Unit *unit = &controller->units[i];
        if (!unit->alive) continue;
        unit->movementPoints = (unit->team == team)
            ? unit->maxMovementPoints : 0;
        if (unit->team == team)
        {
            unit->hasAttackedThisTurn = FALSE;
            unit->hasRepairedThisTurn = FALSE;
        }
    }
    return TRUE;
}

void UnitController_setFogEnabled(UnitController *controller, bool enabled)
{
    if (controller == NULL) return;
    if (controller->fogEnabled == enabled) return;

    controller->fogEnabled = enabled;
    UnitController_invalidateMovementRange(controller);
    clearPlanningState(controller);
}

/* Consultas de alcance y vista previa. El hover debe ser de solo lectura. */

bool UnitController_refreshMovementRange(UnitController *controller)
{
    const Unit *unit;
    u32 cellCount;

    if ((controller == NULL) || (controller->map == NULL) ||
        (controller->pathWorkspace == NULL) ||
        (controller->pathWorkspace->reachable == NULL))
        return FALSE;

    cellCount = (u32)controller->map->width * controller->map->height;
    if (cellCount > controller->pathWorkspace->capacity) return FALSE;

    if (controller->moving ||
        (controller->selectedIndex == NO_SELECTED_UNIT))
    {
        memset(controller->pathWorkspace->reachable, 0, cellCount);
        UnitController_invalidateMovementRange(controller);
        return TRUE;
    }

    unit = &controller->units[controller->selectedIndex];
    if (unit->team != controller->activeTeam)
    {
        memset(controller->pathWorkspace->reachable, 0, cellCount);
        UnitController_invalidateMovementRange(controller);
        return TRUE;
    }

    if (movementRangeMatchesSelection(controller)) return TRUE;

    UnitController_invalidateMovementRange(controller);
    {
        bool built;

        maskHiddenEnemyOccupants(controller);
        built = Pathfinder_buildReachabilityForType(
            controller->map, controller->pathWorkspace, unit->x, unit->y,
            (s8)unit->id, unit->type, unit->movementPoints);
        restoreHiddenEnemyOccupants(controller);
        if (!built) return FALSE;
    }

    controller->movementRangeValid = TRUE;
    controller->movementRangeUnitIndex = controller->selectedIndex;
    controller->movementRangeUnitX = unit->x;
    controller->movementRangeUnitY = unit->y;
    controller->movementRangePoints = unit->movementPoints;
    return TRUE;
}

const u8 *UnitController_getMovementRange(const UnitController *controller)
{
    if ((controller == NULL) || (controller->pathWorkspace == NULL))
        return NULL;
    return controller->pathWorkspace->reachable;
}

void UnitController_haltMovement(UnitController *controller)
{
    if (controller == NULL) return;

    /* El fin de misión no debe reconstruir el alcance de una unidad cancelada. */
    controller->selectedIndex = NO_SELECTED_UNIT;
    clearPlanningState(controller);
    if (controller->moving) stopMovement(controller);
    UnitController_invalidateMovementRange(controller);
}

bool UnitController_hasPendingRoute(const UnitController *controller)
{
    return (controller != NULL) && controller->pendingRoute;
}

bool UnitController_updatePreview(UnitController *controller, u16 x, u16 y)
{
    const Unit *unit;
    const CellData *target;
    const bool hadPreview = (controller != NULL) &&
        (controller->previewRelevant || controller->previewReachable);
    u16 length = 0;
    u8 cost = 0;

    if ((controller == NULL) || (controller->map == NULL) || controller->moving)
        return FALSE;

    if (controller->pendingRoute &&
        ((controller->pendingX != x) || (controller->pendingY != y)))
        clearPendingRoute(controller);

    if (controller->selectedIndex == NO_SELECTED_UNIT)
    {
        const bool hadState = controller->previewChecked || hadPreview;
        clearPreview(controller);
        return hadState;
    }

    if (controller->previewChecked && (controller->previewX == x) &&
        (controller->previewY == y))
        return FALSE;

    unit = &controller->units[controller->selectedIndex];
    controller->previewChecked = TRUE;
    controller->previewX = x;
    controller->previewY = y;
    controller->previewRelevant = FALSE;
    controller->previewReachable = FALSE;
    controller->previewLength = 0;
    controller->previewCost = 0;

    if ((x == unit->x) && (y == unit->y)) return hadPreview;

    target = MapData_getCellConst(controller->map, x, y);
    if ((target == NULL) ||
        ((target->occupantId != CELL_NO_OCCUPANT) &&
         !isHiddenEnemyAt(controller, x, y)))
        return TRUE;

    controller->previewRelevant = TRUE;
    if (getMovementRangeInfo(controller, unit, x, y, &length, &cost) &&
        (length > 0))
    {
        controller->previewReachable = TRUE;
        controller->previewLength = length;
        controller->previewCost = cost;
    }
    return TRUE;
}

bool UnitController_getPreviewInfo(const UnitController *controller,
                                   bool *reachable, u16 *stepCount,
                                   u8 *totalCost, u8 *availablePoints)
{
    const Unit *unit;

    if ((controller == NULL) || (controller->moving) ||
        !controller->previewRelevant || !controller->previewChecked ||
        (controller->selectedIndex == NO_SELECTED_UNIT))
        return FALSE;

    unit = &controller->units[controller->selectedIndex];
    if (reachable != NULL) *reachable = controller->previewReachable;
    if (stepCount != NULL) *stepCount = controller->previewLength;
    if (totalCost != NULL) *totalCost = controller->previewCost;
    if (availablePoints != NULL)
        *availablePoints = unit->movementPoints;
    return TRUE;
}

bool UnitController_getPreviewCell(const UnitController *controller,
                                   u16 stepIndex, u16 *x, u16 *y)
{
    const Unit *unit;

    if ((controller == NULL) || controller->moving ||
        !controller->previewReachable || (x == NULL) || (y == NULL) ||
        (stepIndex >= controller->previewLength) ||
        (controller->selectedIndex == NO_SELECTED_UNIT))
        return FALSE;

    unit = &controller->units[controller->selectedIndex];
    if (movementRangeMatchesSelection(controller))
        return Pathfinder_getReachableStep(
            controller->map, controller->pathWorkspace,
            unit->x, unit->y, controller->previewX, controller->previewY,
            unit->movementPoints, stepIndex, x, y);

    /* Compatibilidad con workspace sin buffer de alcance en pruebas host. */
    {
        u16 i;
        u16 cellX = unit->x;
        u16 cellY = unit->y;

        for (i = 0; i <= stepIndex; i++)
        {
            u16 nextX;
            u16 nextY;
            const PathDirection direction = Pathfinder_getStep(
                controller->pathWorkspace, controller->previewLength, i);

            if (!getNeighbour(controller->map, cellX, cellY, direction,
                              &nextX, &nextY))
                return FALSE;
            cellX = nextX;
            cellY = nextY;
        }
        *x = cellX;
        *y = cellY;
        return TRUE;
    }
}

bool UnitController_isMoving(const UnitController *controller)
{
    return (controller != NULL) && controller->moving;
}

/* Consultas de solo lectura para el render y el HUD. */

void UnitController_getRenderPosition(const UnitController *controller,
                                      u8 unitIndex, s32 *pixelX, s32 *pixelY)
{
    const Unit *unit = UnitController_getUnit(controller, unitIndex);
    s32 x;
    s32 y;

    if ((pixelX == NULL) || (pixelY == NULL)) return;
    if (unit == NULL)
    {
        *pixelX = 0;
        *pixelY = 0;
        return;
    }

    x = (s32)unit->x * CELL_PIXELS;
    y = (s32)unit->y * CELL_PIXELS;

    if (controller->moving && (controller->movingIndex == unitIndex) &&
        (controller->pathStep < controller->pathLength) &&
        (controller->segmentPixels > 0))
    {
        switch (Pathfinder_getStep(controller->pathWorkspace,
                                   controller->pathLength,
                                   controller->pathStep))
        {
            case PATH_DIR_UP:    y -= controller->segmentPixels; break;
            case PATH_DIR_RIGHT: x += controller->segmentPixels; break;
            case PATH_DIR_DOWN:  y += controller->segmentPixels; break;
            case PATH_DIR_LEFT:  x -= controller->segmentPixels; break;
            default: break;
        }
    }

    *pixelX = x;
    *pixelY = y;
}

u8 UnitController_getCount(const UnitController *controller)
{
    return (controller != NULL) ? controller->count : 0;
}

const Unit *UnitController_getUnit(const UnitController *controller, u8 index)
{
    if ((controller == NULL) || (index >= controller->count)) return NULL;
    return &controller->units[index];
}

Unit *UnitController_getMutableUnitById(UnitController *controller, u8 id)
{
    s16 index;

    if (controller == NULL) return NULL;
    index = findUnitIndex(controller, id);
    if (index == NO_SELECTED_UNIT) return NULL;
    if (!controller->units[index].alive) return NULL;
    return &controller->units[index];
}

const Unit *UnitController_getUnitAt(const UnitController *controller,
                                     u16 x, u16 y)
{
    const CellData *cell;
    s16 index;

    if ((controller == NULL) || (controller->map == NULL)) return NULL;
    cell = MapData_getCellConst(controller->map, x, y);
    if ((cell == NULL) || (cell->occupantId == CELL_NO_OCCUPANT)) return NULL;
    index = findUnitIndex(controller, (u8)cell->occupantId);
    if (index == NO_SELECTED_UNIT) return NULL;
    if (!controller->units[index].alive) return NULL;
    return &controller->units[index];
}

bool UnitController_removeUnit(UnitController *controller, u8 id)
{
    s16 index;
    Unit *unit;
    CellData *cell;

    if ((controller == NULL) || (controller->map == NULL)) return FALSE;
    index = findUnitIndex(controller, id);
    if (index == NO_SELECTED_UNIT) return FALSE;
    unit = &controller->units[index];
    if (!unit->alive) return FALSE;

    cell = MapData_getCell(controller->map, unit->x, unit->y);
    if ((cell == NULL) || (cell->occupantId != (s8)unit->id)) return FALSE;

    cell->occupantId = CELL_NO_OCCUPANT;
    unit->health = 0;
    unit->alive = FALSE;
    if (controller->selectedIndex == index)
    {
        controller->selectedIndex = NO_SELECTED_UNIT;
        clearPlanningState(controller);
    }
    if (controller->movingIndex == index) stopMovement(controller);
    UnitController_invalidateMovementRange(controller);
    return TRUE;
}

const Unit *UnitController_getSelected(const UnitController *controller)
{
    if ((controller == NULL) || (controller->selectedIndex < 0) ||
        (controller->selectedIndex >= controller->count))
        return NULL;

    if (!controller->units[controller->selectedIndex].alive) return NULL;
    return &controller->units[controller->selectedIndex];
}

s16 UnitController_getSelectedIndex(const UnitController *controller)
{
    if ((controller == NULL) || (controller->selectedIndex < 0) ||
        (controller->selectedIndex >= controller->count))
        return NO_SELECTED_UNIT;
    return controller->selectedIndex;
}

TeamId UnitController_getActiveTeam(const UnitController *controller)
{
    return (controller != NULL) ? controller->activeTeam : TEAM_BLUE;
}

u16 UnitController_getTurnNumber(const UnitController *controller)
{
    return (controller != NULL) ? controller->turnNumber : 0;
}

const char *UnitController_getActionText(UnitActionResult result)
{
    switch (result)
    {
        case UNIT_ACTION_SELECTED:   return "UNIDAD SELECCIONADA: MUEVE CURSOR";
        case UNIT_ACTION_DESELECTED: return "UNIDAD DESELECCIONADA";
        case UNIT_ACTION_ROUTE_READY: return "RUTA PREPARADA: A CONFIRMA, B CANCELA";
        case UNIT_ACTION_CANCELLED: return "ORDEN CANCELADA";
        case UNIT_ACTION_MOVING:     return "MOVIENDO UNIDAD...";
        case UNIT_ACTION_MOVED:      return "MOVIMIENTO COMPLETADO";
        case UNIT_ACTION_NO_UNIT:    return "NO HAY UNIDAD U ORDEN QUE CANCELAR";
        case UNIT_ACTION_NO_ROUTE:   return "SIN RUTA: BLOQUEADA / FUERA DE RANGO";
        case UNIT_ACTION_BLOCKED:    return "DESTINO BLOQUEADO";
        case UNIT_ACTION_ENEMY_UNIT: return "TURNO DEL OTRO EQUIPO";
        case UNIT_ACTION_NO_MOVEMENT_POINTS: return "SIN PUNTOS DE MOVIMIENTO";
        default:                     return "ACCION NO DISPONIBLE";
    }
}
