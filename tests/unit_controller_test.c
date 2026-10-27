#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "unit_types.h"
#include "cell_events.h"
#include "unit_controller.h"
#include "game_config.h"

#define EVENT_TRACE_CAPACITY 32

typedef struct
{
    CellEventType type;
    CellEventSource source;
    u16 x;
    u16 y;
    u16 otherX;
    u16 otherY;
    s8 cellOccupantId;
    s8 otherOccupantId;
} MovementEvent;

static MovementEvent eventTrace[EVENT_TRACE_CAPACITY];
static u8 eventTraceCount;
static u8 unitLeaveEvents;
static u8 unitEnterEvents;

static void recordMovementEvent(const CellEventContext *context)
{
    MovementEvent *event;

    assert(context != NULL);
    assert(eventTraceCount < EVENT_TRACE_CAPACITY);
    event = &eventTrace[eventTraceCount++];
    event->type = context->type;
    event->source = context->source;
    event->x = context->x;
    event->y = context->y;
    event->otherX = context->otherX;
    event->otherY = context->otherY;
    event->cellOccupantId = context->cell->occupantId;
    if ((context->otherX != CELL_EVENT_NO_COORDINATE) &&
        (context->otherY != CELL_EVENT_NO_COORDINATE))
    {
        const CellData *otherCell = MapData_getCellConst(
            context->map, context->otherX, context->otherY);
        event->otherOccupantId = (otherCell != NULL)
            ? otherCell->occupantId : CELL_NO_OCCUPANT;
    }
    else event->otherOccupantId = CELL_NO_OCCUPANT;
}

static void onUnitLeave(const CellEventContext *context)
{
    assert(context->type == CELL_EVENT_UNIT_LEAVE);
    unitLeaveEvents++;
    recordMovementEvent(context);
}

static void onUnitEnter(const CellEventContext *context)
{
    assert(context->type == CELL_EVENT_UNIT_ENTER);
    unitEnterEvents++;
    recordMovementEvent(context);
}

static void expectMovementEvent(u8 index, CellEventType type,
                                u16 x, u16 y, u16 otherX, u16 otherY)
{
    const MovementEvent *event;

    assert(index < eventTraceCount);
    event = &eventTrace[index];
    assert(event->type == type);
    assert(event->source == CELL_EVENT_SOURCE_UNIT);
    assert(event->x == x && event->y == y);
    assert(event->otherX == otherX && event->otherY == otherY);
}

static const CellEventSet eventSets[] =
{
    { { 0 } },
    { {
        [CELL_EVENT_BEFORE_LEAVE] = recordMovementEvent,
        [CELL_EVENT_LEAVE] = recordMovementEvent,
        [CELL_EVENT_BEFORE_ENTER] = recordMovementEvent,
        [CELL_EVENT_ENTER] = recordMovementEvent,
        [CELL_EVENT_UNIT_LEAVE] = onUnitLeave,
        [CELL_EVENT_UNIT_ENTER] = onUnitEnter
    } }
};

static void testSelectionRouteMovementAndEvents(void)
{
    static const char *const rows[] = { "PPP", "PWP", "PPP" };
    CellData cells[9];
    u8 costs[9];
    u8 parents[9];
    u8 movementRange[9];
    PathfinderWorkspace workspace = { costs, parents, 9, movementRange };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] = { { 0, 0, 0, 4, UNIT_TYPE_INFANTRY, TEAM_BLUE } };
    const UnitStart invalidStarts[] = { { 0, 1, 1, 4, UNIT_TYPE_INFANTRY, TEAM_BLUE } };
    MapData map =
    {
        .width = 3, .height = 3,
        .terrainRows = rows, .terrainRowCount = 3,
        .cells = cells, .cellCapacity = 9,
        .pathWorkspace = &workspace
    };
    const Unit *unit;
    bool reachable;
    u16 steps;
    u8 cost;
    u8 range;
    u16 x;
    u16 y;
    s32 renderX;
    s32 renderY;
    s32 previousRenderX;
    s32 previousRenderY;
    u8 frame;

    unitLeaveEvents = unitEnterEvents = 0;
    eventTraceCount = 0;
    assert(MapData_init(&map));
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    assert(!UnitController_init(&controller, &map, units, 1, invalidStarts, 1));
    assert(MapData_getCell(&map, 0, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(MapData_getCell(&map, 0, 0)->occupantId == 0);
    assert(UnitController_getSelected(&controller) == NULL);

    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_getSelected(&controller) == &units[0]);
    assert(UnitController_refreshMovementRange(&controller));
    assert(movementRange[0] == 0); /* La casilla ocupada no hace glow. */
    assert(movementRange[1] && movementRange[2]);
    assert(!movementRange[4]); /* El agua no se ilumina. */
    assert(movementRange[8]);  /* Coste exacto 4 por el borde inferior. */
    assert(costs[2] == 2); /* El alcance conserva el coste minimo hasta el destino. */
    assert(UnitController_updatePreview(&controller, 1, 0));
    assert(costs[2] == 2); /* Cambiar una celda no lanza otra búsqueda de ruta. */
    assert(UnitController_updatePreview(&controller, 2, 0));
    assert(costs[2] == 2); /* Consultar destinos reutiliza el alcance cacheado. */
    assert(UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                         &cost, &range));
    assert(reachable && steps == 2 && cost == 2 && range == 4);
    assert(UnitController_getPreviewCell(&controller, 0, &x, &y));
    assert(x == 1 && y == 0);
    assert(UnitController_getPreviewCell(&controller, 1, &x, &y));
    assert(x == 2 && y == 0);

    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_ROUTE_READY);
    assert(costs[2] == 2); /* La primera A solo prepara; no calcula otra ruta. */
    assert(UnitController_hasPendingRoute(&controller));
    assert(eventTraceCount == 0);
    assert(!UnitController_isMoving(&controller));
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_MOVING);
    assert(eventTraceCount == 0);
    assert(!UnitController_hasPendingRoute(&controller));
    assert(UnitController_isMoving(&controller));
    assert(UnitController_refreshMovementRange(&controller));
    for (frame = 0; frame < 9; frame++) assert(movementRange[frame] == 0);
    UnitController_getRenderPosition(&controller, 0,
                                     &previousRenderX, &previousRenderY);
    for (frame = 0; frame < (CELL_PIXELS / UNIT_MOVE_SPEED_PIXELS); frame++)
    {
        assert(UnitController_update(&controller) == UNIT_ACTION_MOVING);
        UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
        /* El sprite avanza la misma distancia también al cruzar el centro. */
        assert(renderX == previousRenderX + UNIT_MOVE_SPEED_PIXELS);
        assert(renderY == previousRenderY);
        previousRenderX = renderX;
        previousRenderY = renderY;
    }
    unit = UnitController_getUnit(&controller, 0);
    assert(unit->x == 1 && unit->y == 0);
    assert(MapData_getCell(&map, 0, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(MapData_getCell(&map, 1, 0)->occupantId == 0);

    {
        UnitActionResult lastResult = UNIT_ACTION_MOVING;
        for (frame = 0; UnitController_isMoving(&controller) && (frame < 100); frame++)
        {
            lastResult = UnitController_update(&controller);
            UnitController_getRenderPosition(&controller, 0,
                                             &renderX, &renderY);
            assert(renderX == previousRenderX + UNIT_MOVE_SPEED_PIXELS);
            assert(renderY == previousRenderY);
            previousRenderX = renderX;
            previousRenderY = renderY;
        }
        assert(!UnitController_isMoving(&controller));
        assert(lastResult == UNIT_ACTION_MOVED);
    }
    unit = UnitController_getUnit(&controller, 0);
    assert(unit->x == 2 && unit->y == 0);
    assert(MapData_getCell(&map, 2, 0)->occupantId == 0);
    assert(UnitController_getSelected(&controller) == NULL);
    assert(UnitController_getSelectedIndex(&controller) == -1);
    assert(UnitController_refreshMovementRange(&controller));
    for (frame = 0; frame < 9; frame++)
        assert(movementRange[frame] == 0); /* Alcance apagado tras la llegada. */

    /* Se puede seleccionar de nuevo, pero no iniciar otra orden ese turno. */
    assert(unit->movementPoints == 0);
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_refreshMovementRange(&controller));
    assert(!movementRange[2] && !movementRange[5]);
    assert(UnitController_activate(&controller, 2, 1) ==
           UNIT_ACTION_NO_MOVEMENT_POINTS);
    assert(UnitController_cancelOrder(&controller) == UNIT_ACTION_CANCELLED);
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 2));
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_SELECTED);
    assert(unitLeaveEvents == 2 && unitEnterEvents == 2);
    assert(eventTraceCount == 12);
    expectMovementEvent(0, CELL_EVENT_BEFORE_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(1, CELL_EVENT_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(2, CELL_EVENT_BEFORE_ENTER, 1, 0, 0, 0);
    expectMovementEvent(3, CELL_EVENT_ENTER, 1, 0, 0, 0);
    expectMovementEvent(4, CELL_EVENT_UNIT_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(5, CELL_EVENT_UNIT_ENTER, 1, 0, 0, 0);
    expectMovementEvent(6, CELL_EVENT_BEFORE_LEAVE, 1, 0, 2, 0);
    expectMovementEvent(7, CELL_EVENT_LEAVE, 1, 0, 2, 0);
    expectMovementEvent(8, CELL_EVENT_BEFORE_ENTER, 2, 0, 1, 0);
    expectMovementEvent(9, CELL_EVENT_ENTER, 2, 0, 1, 0);
    expectMovementEvent(10, CELL_EVENT_UNIT_LEAVE, 1, 0, 2, 0);
    expectMovementEvent(11, CELL_EVENT_UNIT_ENTER, 2, 0, 1, 0);
    assert(UnitController_getActionText(UNIT_ACTION_MOVED)[0] != '\0');

    assert(UnitController_updatePreview(&controller, 1, 1));
    assert(UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                         &cost, &range));
    assert(!reachable); /* Destino agua/obstáculo o ruta fuera del presupuesto. */

    /* Volver al origen borra la previsualización anterior y notifica al HUD. */
    assert(UnitController_updatePreview(&controller, 2, 0));
    assert(!UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                          &cost, &range));
    assert(UnitController_activate(&controller, 1, 1) == UNIT_ACTION_NO_ROUTE);
    assert(unitLeaveEvents == 2 && unitEnterEvents == 2);
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_DESELECTED);
    assert(UnitController_getSelected(&controller) == NULL);
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_SELECTED);

    /* A prepara una ruta, B la cancela y limpia sus marcadores/seleccion. */
    assert(UnitController_updatePreview(&controller, 1, 0));
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_hasPendingRoute(&controller));

    /* Una ocupación desconocida invalida la ruta antigua y sus marcadores. */
    assert(MapData_setOccupant(&map, 1, 0, 127));
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_BLOCKED);
    assert(!UnitController_hasPendingRoute(&controller));
    assert(!UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                          &cost, &range));
    assert(MapData_setOccupant(&map, 1, 0, CELL_NO_OCCUPANT));
    assert(UnitController_updatePreview(&controller, 1, 0));
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_hasPendingRoute(&controller));

    /* Cambiar el cursor invalida el destino que estaba preparado. */
    assert(UnitController_updatePreview(&controller, 0, 0));
    assert(!UnitController_hasPendingRoute(&controller));
    assert(UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                         &cost, &range));
    assert(reachable && steps > 0);
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_cancelOrder(&controller) == UNIT_ACTION_CANCELLED);
    assert(!UnitController_hasPendingRoute(&controller));
    assert(UnitController_getSelected(&controller) == NULL);
    assert(!UnitController_getPreviewInfo(&controller, &reachable, &steps,
                                          &cost, &range));
    assert(UnitController_cancelOrder(&controller) == UNIT_ACTION_NO_UNIT);
}

static void testCursorHoverNeverRunsPathfinder(void)
{
    static const char *const rows[] =
    {
        "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[15];
    u8 costs[15];
    u8 parents[15];
    u8 movementRange[15];
    u8 costsSnapshot[15];
    u8 parentsSnapshot[15];
    u8 rangeSnapshot[15];
    PathfinderWorkspace workspace = { costs, parents, 15, movementRange };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, 0, 1, 8, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 5, .height = 3,
        .terrainRows = rows, .terrainRowCount = 3,
        .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace
    };
    static const u16 hoverX[] = { 1, 2, 3, 3, 2, 4 };
    static const u16 hoverY[] = { 1, 1, 1, 0, 2, 1 };
    u8 i;

    assert(MapData_init(&map));
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 1) == UNIT_ACTION_SELECTED);
    assert(controller.movementRangeValid);

    /* El alcance se calcula al seleccionar la unidad, no durante el hover. */
    memcpy(costsSnapshot, costs, sizeof(costs));
    memcpy(parentsSnapshot, parents, sizeof(parents));
    memcpy(rangeSnapshot, movementRange, sizeof(movementRange));

    /* Incluso con la caché invalidada, el hover no debe iniciar una búsqueda. */
    controller.movementRangeValid = FALSE;
    controller.movementRangeUnitIndex = -1;
    assert(UnitController_updatePreview(&controller, 4, 2));
    assert(!controller.movementRangeValid);
    assert(memcmp(costsSnapshot, costs, sizeof(costs)) == 0);
    assert(memcmp(parentsSnapshot, parents, sizeof(parents)) == 0);
    assert(memcmp(rangeSnapshot, movementRange, sizeof(movementRange)) == 0);

    /* El refresco debe ser explícito; a partir de aquí, mover el cursor solo lee. */
    assert(UnitController_refreshMovementRange(&controller));
    memcpy(costsSnapshot, costs, sizeof(costs));
    memcpy(parentsSnapshot, parents, sizeof(parents));
    memcpy(rangeSnapshot, movementRange, sizeof(movementRange));
    for (i = 0; i < (u8)(sizeof(hoverX) / sizeof(hoverX[0])); i++)
    {
        assert(UnitController_updatePreview(&controller, hoverX[i], hoverY[i]));
        assert(controller.movementRangeValid);
        assert(memcmp(costsSnapshot, costs, sizeof(costs)) == 0);
        assert(memcmp(parentsSnapshot, parents, sizeof(parents)) == 0);
        assert(memcmp(rangeSnapshot, movementRange, sizeof(movementRange)) == 0);
    }

    /* La primera A prepara el destino usando solo la máscara ya calculada. */
    assert(UnitController_activate(&controller, 4, 1) == UNIT_ACTION_ROUTE_READY);
    assert(memcmp(costsSnapshot, costs, sizeof(costs)) == 0);
    assert(memcmp(parentsSnapshot, parents, sizeof(parents)) == 0);

    /* La ruta concreta se calcula al confirmar la misma celda con la segunda A. */
    assert(UnitController_activate(&controller, 4, 1) == UNIT_ACTION_MOVING);
    assert(memcmp(costsSnapshot, costs, sizeof(costs)) != 0 ||
           memcmp(parentsSnapshot, parents, sizeof(parents)) != 0);
}

static void testMovementKeepsMapAndSpriteSynchronized(void)
{
    static const char *const rows[] = { "PPPP" };
    CellData cells[4];
    u8 costs[4];
    u8 parents[4];
    u8 movementRange[4];
    PathfinderWorkspace workspace = { costs, parents, 4, movementRange };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] = { { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE } };
    MapData map =
    {
        .width = 4, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 4,
        .pathWorkspace = &workspace
    };
    s32 renderX;
    s32 renderY;
    u8 frame;

    unitLeaveEvents = unitEnterEvents = 0;
    eventTraceCount = 0;
    assert(MapData_init(&map));
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_MOVING);

    /* El primer fotograma interpola 4 px sin adelantar la ocupacion logica. */
    assert(UnitController_update(&controller) == UNIT_ACTION_MOVING);
    assert(eventTraceCount == 0); /* No se anuncian pasos incompletos. */
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == UNIT_MOVE_SPEED_PIXELS && renderY == 0);
    assert(units[0].x == 0 && cells[0].occupantId == 0);
    assert(cells[1].occupantId == CELL_NO_OCCUPANT);

    /* No se permite cambiar de unidad ni cancelar una animacion en curso. */
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_MOVING);
    assert(UnitController_cancelOrder(&controller) == UNIT_ACTION_MOVING);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == UNIT_MOVE_SPEED_PIXELS && renderY == 0);

    for (frame = 1; frame < (CELL_PIXELS / UNIT_MOVE_SPEED_PIXELS); frame++)
        (void)UnitController_update(&controller);

    /* La llegada actualiza a la vez posicion logica, ocupacion y centro visual. */
    assert(units[0].x == 1 && units[0].y == 0);
    assert(cells[0].occupantId == CELL_NO_OCCUPANT);
    assert(cells[1].occupantId == 0);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == CELL_PIXELS && renderY == 0);
    assert(unitLeaveEvents == 1 && unitEnterEvents == 1);
    assert(eventTraceCount == 6);
    expectMovementEvent(0, CELL_EVENT_BEFORE_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(1, CELL_EVENT_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(2, CELL_EVENT_BEFORE_ENTER, 1, 0, 0, 0);
    expectMovementEvent(3, CELL_EVENT_ENTER, 1, 0, 0, 0);
    expectMovementEvent(4, CELL_EVENT_UNIT_LEAVE, 0, 0, 1, 0);
    expectMovementEvent(5, CELL_EVENT_UNIT_ENTER, 1, 0, 0, 0);

    /* Los callbacks ven estados coherentes en cada frontera del ciclo. */
    assert(eventTrace[0].cellOccupantId == 0);
    assert(eventTrace[0].otherOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[1].cellOccupantId == 0); /* leave antes de liberar origen */
    assert(eventTrace[1].otherOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[2].cellOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[2].otherOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[3].cellOccupantId == 0); /* enter tras ocupar destino */
    assert(eventTrace[3].otherOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[4].cellOccupantId == CELL_NO_OCCUPANT);
    assert(eventTrace[4].otherOccupantId == 0);
    assert(eventTrace[5].cellOccupantId == 0);
    assert(eventTrace[5].otherOccupantId == CELL_NO_OCCUPANT);

    /* Si el siguiente paso se bloquea entre tramos, la unidad no se desplaza
       visualmente hacia el obstaculo ni pierde la casilla que ocupa. */
    assert(MapData_setOccupant(&map, 2, 0, 77));
    assert(UnitController_update(&controller) == UNIT_ACTION_BLOCKED);
    assert(!UnitController_isMoving(&controller));
    assert(units[0].x == 1 && units[0].y == 0);
    assert(cells[1].occupantId == 0 && cells[2].occupantId == 77);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == CELL_PIXELS && renderY == 0);
    assert(unitLeaveEvents == 1 && unitEnterEvents == 1);
    assert(eventTraceCount == 6);

    /* El tramo rechazado no genera eventos de salida ni de entrada. */
    assert(unitLeaveEvents == 1 && unitEnterEvents == 1);
    assert(eventTraceCount == 6);

    /* Queda seleccionada para que el jugador pueda preparar otra ruta. */
    assert(UnitController_getSelected(&controller) == &units[0]);
    assert(UnitController_updatePreview(&controller, 2, 0));
}


static void assertMovementCompletesExactly(u16 startX, u16 startY,
                                           u16 targetX, u16 targetY)
{
    static const char *const rows[] =
    {
        "PPPPP", "PPPPP", "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[25];
    u8 costs[25];
    u8 parents[25];
    u8 reachable[25];
    PathfinderWorkspace workspace = { costs, parents, 25, reachable };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, startX, startY, 10, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 5, .height = 5,
        .terrainRows = rows, .terrainRowCount = 5,
        .cells = cells, .cellCapacity = 25,
        .pathWorkspace = &workspace
    };
    UnitActionResult result;
    const Unit *unit;
    s32 renderX;
    s32 renderY;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, startX, startY) ==
           UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, targetX, targetY) ==
           UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, targetX, targetY) ==
           UNIT_ACTION_MOVING);

    /* Al comprobar cada fotograma, la posición renderizada debe corresponder
       exactamente a la casilla lógica más el único tramo interpolado activo. */
    for (frame = 0; frame < 200 && UnitController_isMoving(&controller); frame++)
    {
        u16 x;
        u16 y;
        result = UnitController_update(&controller);
        unit = UnitController_getUnit(&controller, 0);
        assert(unit != NULL);
        UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);

        if (controller.moving && (controller.segmentPixels > 0))
        {
            const PathDirection direction = Pathfinder_getStep(
                &workspace, controller.pathLength, controller.pathStep);
            s32 expectedX = (s32)unit->x * CELL_PIXELS;
            s32 expectedY = (s32)unit->y * CELL_PIXELS;
            switch (direction)
            {
                case PATH_DIR_UP:    expectedY -= controller.segmentPixels; break;
                case PATH_DIR_RIGHT: expectedX += controller.segmentPixels; break;
                case PATH_DIR_DOWN:  expectedY += controller.segmentPixels; break;
                case PATH_DIR_LEFT:  expectedX -= controller.segmentPixels; break;
                default: assert(0 && "dirección inválida durante movimiento");
            }
            assert(renderX == expectedX && renderY == expectedY);
        }
        else
        {
            assert(renderX == (s32)unit->x * CELL_PIXELS);
            assert(renderY == (s32)unit->y * CELL_PIXELS);
        }

        /* En todo instante solo la casilla lógica contiene a la unidad. */
        for (y = 0; y < map.height; y++)
        {
            for (x = 0; x < map.width; x++)
            {
                const CellData *cell = MapData_getCellConst(&map, x, y);
                if ((x == unit->x) && (y == unit->y))
                    assert(cell->occupantId == (s8)unit->id);
                else
                    assert(cell->occupantId == CELL_NO_OCCUPANT);
            }
        }
        if (!UnitController_isMoving(&controller))
            assert(result == UNIT_ACTION_MOVED);
        else
            assert(result == UNIT_ACTION_MOVING);
    }

    assert(!UnitController_isMoving(&controller));
    unit = UnitController_getUnit(&controller, 0);
    assert(unit->x == targetX && unit->y == targetY);
    assert(controller.segmentPixels == 0);
    assert(controller.pathLength == 0 && controller.pathStep == 0);
    assert(controller.movingIndex == -1 && controller.selectedIndex == -1);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == (s32)targetX * CELL_PIXELS);
    assert(renderY == (s32)targetY * CELL_PIXELS);
    assert(MapData_getCellConst(&map, targetX, targetY)->occupantId == 0);
}

static void testMovementAllDirectionsAndTurnsSnapExactly(void)
{
    /* Los cuatro sentidos y una ruta que obliga a cambiar de dirección. */
    assertMovementCompletesExactly(0, 2, 4, 2);
    assertMovementCompletesExactly(4, 2, 0, 2);
    assertMovementCompletesExactly(2, 4, 2, 0);
    assertMovementCompletesExactly(2, 0, 2, 4);
    assertMovementCompletesExactly(0, 0, 3, 3);
    puts("PASS: cuatro direcciones y ruta con giros terminan centradas, con ocupación coherente");
}

static void testBlockAppearingMidSegmentSnapsBack(void)
{
    static const char *const rows[] = { "PPPP", "PPPP" };
    CellData cells[8];
    u8 costs[8];
    u8 parents[8];
    u8 reachable[8];
    PathfinderWorkspace workspace = { costs, parents, 8, reachable };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 4, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 8,
        .pathWorkspace = &workspace
    };
    s32 renderX;
    s32 renderY;
    u8 i;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_MOVING);

    /* Completa la primera celda y avanza 20/24 px en el siguiente tramo. */
    for (i = 0; i < (2 * CELL_PIXELS / UNIT_MOVE_SPEED_PIXELS) - 1; i++)
        assert(UnitController_update(&controller) == UNIT_ACTION_MOVING);
    assert(UnitController_isMoving(&controller));
    assert(units[0].x == 1 && controller.segmentPixels ==
           CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS);

    /* Un ocupante aparece antes de la confirmación del paso interpolado. */
    assert(MapData_setOccupant(&map, 2, 0, 77));
    assert(UnitController_update(&controller) == UNIT_ACTION_BLOCKED);
    assert(!UnitController_isMoving(&controller));
    assert(controller.segmentPixels == 0 && controller.pathLength == 0);
    assert(units[0].x == 1 && units[0].y == 0);
    assert(MapData_getCellConst(&map, 1, 0)->occupantId == 0);
    assert(MapData_getCellConst(&map, 2, 0)->occupantId == 77);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == CELL_PIXELS && renderY == 0);

    /* Tras avanzar una celda, abortar la ruta no permite emitir otra orden
       en el mismo turno, aunque exista una ruta alternativa libre. */
    assert(units[0].movementPoints == 0);
    assert(UnitController_activate(&controller, 1, 1) ==
           UNIT_ACTION_NO_MOVEMENT_POINTS);
    assert(units[0].x == 1 && units[0].y == 0);
    puts("PASS: una ruta interrumpida tras avanzar consume la acción y vuelve al centro lógico");
}

static void testBlockBeforeFirstCellKeepsActionAndAllowsReplan(void)
{
    static const char *const rows[] = { "PPPP", "PPPP" };
    CellData cells[8];
    u8 costs[8], parents[8], reachable[8];
    PathfinderWorkspace workspace = { costs, parents, 8, reachable };
    MapData map = { .width = 4, .height = 2, .terrainRows = rows,
        .terrainRowCount = 2, .cells = cells, .cellCapacity = 8,
        .pathWorkspace = &workspace };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    s32 renderX;
    s32 renderY;
    u8 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_MOVING);

    for (frame = 0; frame < (CELL_PIXELS / UNIT_MOVE_SPEED_PIXELS) - 1; frame++)
        assert(UnitController_update(&controller) == UNIT_ACTION_MOVING);
    assert(controller.segmentPixels == CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS);
    assert(units[0].x == 0 && units[0].y == 0);

    /* El primer paso queda bloqueado antes de confirmar una nueva celda. */
    assert(MapData_setOccupant(&map, 1, 0, 77));
    assert(UnitController_update(&controller) == UNIT_ACTION_BLOCKED);
    assert(!UnitController_isMoving(&controller));
    assert(units[0].x == 0 && units[0].y == 0);
    assert(units[0].movementPoints == units[0].maxMovementPoints);
    assert(controller.movementRangeValid);
    UnitController_getRenderPosition(&controller, 0, &renderX, &renderY);
    assert(renderX == 0 && renderY == 0);

    /* La ruta alternativa se puede preparar con la selección ya conservada. */
    assert(UnitController_activate(&controller, 0, 1) == UNIT_ACTION_ROUTE_READY);
    puts("PASS: el bloqueo antes de salir del origen conserva la acción y permite replantear la ruta");
}

static void testRepeatedTeamTurnCannotRestoreMovement(void)
{
    static const char *const rows[] = { "PPP", "PPP" };
    CellData cells[6];
    u8 costs[6], parents[6], reachable[6];
    PathfinderWorkspace workspace = { costs, parents, 6, reachable };
    MapData map = { .width = 3, .height = 2, .terrainRows = rows,
        .terrainRowCount = 2, .cells = cells, .cellCapacity = 6,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 0, 0, 3, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 1, 3, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    u8 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 1));

    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_MOVING);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100 && units[0].x == 1 && units[0].movementPoints == 0);

    /* Reabrir exactamente el mismo turno no puede regenerar el movimiento. */
    assert(!UnitController_beginTeamTurn(&controller, TEAM_BLUE, 1));
    assert(units[0].movementPoints == 0);

    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    assert(units[0].movementPoints == 0 && units[1].movementPoints == 3);
    /* Tampoco se puede repetir un turno azul obsoleto tras cambiar a rojo. */
    assert(!UnitController_beginTeamTurn(&controller, TEAM_BLUE, 1));
    assert(controller.activeTeam == TEAM_RED && units[1].movementPoints == 3);
    assert(UnitController_activate(&controller, 2, 1) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 1) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 1) == UNIT_ACTION_MOVING);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100 && units[1].x == 1 && units[1].movementPoints == 0);
    assert(!UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    assert(units[1].movementPoints == 0);

    /* El nuevo turno del equipo vuelve a restaurar el presupuesto una sola vez. */
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 3));
    assert(units[0].movementPoints == units[0].maxMovementPoints);
    assert(units[1].movementPoints == 0);
    puts("PASS: ningún turno duplicado repone el movimiento; el siguiente turno válido sí lo restaura");
}

static void testRejectsInsufficientCapacityAndDuplicateUnits(void)
{
    static const char *const rows[] = { "PP", "PP" };
    CellData cells[4];
    u8 costs[4];
    u8 parents[4];
    u8 movementRange[4];
    PathfinderWorkspace workspace = { costs, parents, 4, movementRange };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 3, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 1, 0, 3, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    const UnitStart duplicateIds[] =
    {
        { 0, 0, 0, 3, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 0, 1, 0, 3, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 2, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 4,
        .pathWorkspace = &workspace
    };

    assert(MapData_init(&map));
    assert(!UnitController_init(&controller, &map, units, 1, starts, 2));
    assert(!UnitController_init(&controller, &map, units, 2, duplicateIds, 2));
    assert(MapData_getCell(&map, 0, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(MapData_getCell(&map, 0, 0)->occupantId == 0);
    assert(MapData_getCell(&map, 1, 0)->occupantId == 1);
}


static void testExhaustedMovementPointsBlockFurtherOrders(void)
{
    static const char *const rows[] = { "PP" };
    CellData cells[2];
    u8 costs[2];
    u8 parents[2];
    u8 movementRange[2];
    PathfinderWorkspace workspace = { costs, parents, 2, movementRange };
    Unit units[1];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 1, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 2, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 2,
        .pathWorkspace = &workspace
    };
    u8 frame;

    assert(MapData_init(&map));
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_MOVING);

    for (frame = 0; UnitController_isMoving(&controller) && (frame < 8); frame++)
        (void)UnitController_update(&controller);

    assert(frame < 8);
    assert(units[0].x == 1 && units[0].movementPoints == 0);
    assert(cells[0].occupantId == CELL_NO_OCCUPANT && cells[1].occupantId == 0);
    assert(UnitController_getSelected(&controller) == NULL);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 0, 0) ==
           UNIT_ACTION_NO_MOVEMENT_POINTS);
    assert(units[0].x == 1 && units[0].movementPoints == 0);
}

static void testUnitTypesAndDefaultMovementPoints(void)
{
    static const char *const rows[] = { "PPPP", "PPPP" };
    CellData cells[8];
    u8 costs[8];
    u8 parents[8];
    u8 movementRange[8];
    PathfinderWorkspace workspace = { costs, parents, 8, movementRange };
    Unit units[3];
    UnitController controller;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
        { 1, 1, 0, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
        { 2, 2, 0, 0, UNIT_TYPE_TANK, TEAM_BLUE }
    };
    MapData map =
    {
        .width = 4, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 8,
        .pathWorkspace = &workspace
    };

    assert(MapData_init(&map));
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(units[0].type == UNIT_TYPE_SCOUT && units[0].maxMovementPoints == 8);
    assert(units[1].type == UNIT_TYPE_ENGINEER && units[1].maxMovementPoints == 5);
    assert(units[2].type == UNIT_TYPE_TANK && units[2].maxMovementPoints == 7);
    assert(strcmp(UnitType_getInfo(UNIT_TYPE_SCOUT)->name, "EXPLORADOR") == 0);
    assert(strcmp(UnitType_getInfo(UNIT_TYPE_TANK)->name, "BLINDADO") == 0);
    assert(UnitType_getInfo((UnitType)99) == NULL);
}


static u8 runSingleCellTerrainMove(UnitType type)
{
    static const char *const rows[] = { "PF" };
    CellData cells[2];
    u8 costs[2];
    u8 parents[2];
    u8 reachable[2];
    PathfinderWorkspace workspace = { costs, parents, 2, reachable };
    MapData map =
    {
        .width = 2, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 2,
        .pathWorkspace = &workspace
    };
    Unit unitStorage[1];
    UnitController controller;
    const UnitStart starts[] = { { 0, 0, 0, 0, type, TEAM_BLUE } };
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, unitStorage, 1, starts, 1));
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_MOVING);
    for (frame = 0; frame < 16 && UnitController_isMoving(&controller); frame++)
        (void)UnitController_update(&controller);
    assert(!UnitController_isMoving(&controller));
    assert(unitStorage[0].x == 1 && unitStorage[0].y == 0);
    return unitStorage[0].movementPoints;
}

static void testTerrainMovementCostsAreChargedAtArrival(void)
{
    /* La llegada consume la acción de movimiento del turno, sin importar
       cuántos puntos sobren tras pagar el coste real del terreno. */
    assert(runSingleCellTerrainMove(UNIT_TYPE_INFANTRY) == 0);
    assert(runSingleCellTerrainMove(UNIT_TYPE_SCOUT) == 0);
    assert(runSingleCellTerrainMove(UNIT_TYPE_ENGINEER) == 0);
    assert(runSingleCellTerrainMove(UNIT_TYPE_TANK) == 0);
}

int main(void)
{
    testTerrainMovementCostsAreChargedAtArrival();
    testSelectionRouteMovementAndEvents();
    testCursorHoverNeverRunsPathfinder();
    testMovementKeepsMapAndSpriteSynchronized();
    testMovementAllDirectionsAndTurnsSnapExactly();
    testBlockAppearingMidSegmentSnapsBack();
    testBlockBeforeFirstCellKeepsActionAndAllowsReplan();
    testRepeatedTeamTurnCannotRestoreMovement();
    testRejectsInsufficientCapacityAndDuplicateUnits();
    testExhaustedMovementPointsBlockFurtherOrders();
    testUnitTypesAndDefaultMovementPoints();
    puts("PASS: tipos de unidad, confirmación/cancelación, sincronización, rutas y eventos");
    return 0;
}
