/* La cabecera SGDK string.h usa tipos definidos por genesis.h. */
#include "fog_of_war.h"
#include <string.h>

/*
 * Visión por equipo y exploración persistente. Guarda snapshots de unidades, recalcula
 * regiones sucias y expone índices de celdas modificadas para que la vista parche solo
 * los tiles afectados. No realiza transferencias al VDP desde este módulo.
 */

#define FOG_STATE_BITS_PER_TEAM 2
#define FOG_STATE_MASK          0x03u
#define FOG_STATE_TEMPORARY     3u
#define FOG_MOVEMENT_RECALCS_PER_UPDATE 1u
#define FOG_NO_PRIORITY_UNIT 0xFFu

/*
 * Al arrancar, el sobre conjunto se construye a partir de los extremos de
 * alcance de cada unidad azul. Son índices lineales (u16) para ahorrar RAM.
 * Hay como máximo cuatro puntos candidatos por unidad y hasta 2N vértices
 * temporales al construir la envolvente convexa.
 */
#define FOG_ENVELOPE_POINTS_MAX (FOG_MAX_UNITS * 4u)
static u16 movementEnvelopePoints[FOG_ENVELOPE_POINTS_MAX];
static u16 movementEnvelopeHull[FOG_ENVELOPE_POINTS_MAX * 2u];

/* Visión por equipo empaquetada y consultas públicas de solo lectura. */

static u8 stateShift(TeamId team)
{
    return (u8)((u8)team * FOG_STATE_BITS_PER_TEAM);
}

static u8 getPackedState(const CellData *cell, TeamId team)
{
    return (u8)((cell->visionState >> stateShift(team)) & FOG_STATE_MASK);
}

static void setPackedState(CellData *cell, TeamId team, u8 state)
{
    const u8 shift = stateShift(team);
    const u8 mask = (u8)(FOG_STATE_MASK << shift);

    cell->visionState = (u8)((cell->visionState & (u8)~mask) |
                             ((state & FOG_STATE_MASK) << shift));
}

FogCellState FogOfWar_getCellState(const MapData *map, TeamId team,
                                   u16 x, u16 y)
{
    const CellData *cell;
    u8 state;

    if ((u8)team >= TEAM_COUNT) return FOG_CELL_UNSEEN;
    cell = MapData_getCellConst(map, x, y);
    if (cell == NULL) return FOG_CELL_UNSEEN;

    state = getPackedState(cell, team);
    /* El estado temporal nunca debe escapar de una actualización. */
    if (state == FOG_STATE_TEMPORARY) return FOG_CELL_EXPLORED;
    return (FogCellState)state;
}

bool FogOfWar_isVisible(const MapData *map, TeamId team, u16 x, u16 y)
{
    return FogOfWar_getCellState(map, team, x, y) == FOG_CELL_VISIBLE;
}

bool FogOfWar_isExplored(const MapData *map, TeamId team, u16 x, u16 y)
{
    return FogOfWar_getCellState(map, team, x, y) != FOG_CELL_UNSEEN;
}

/* Regiones sucias, snapshots de unidades e índices por equipo. */

static void includeVisionRegion(FogDirtyRegion *region, const MapData *map,
                                u16 centerX, u16 centerY, u8 radius)
{
    s16 minX = (s16)centerX - (s16)radius;
    s16 minY = (s16)centerY - (s16)radius;
    s16 maxX = (s16)centerX + (s16)radius;
    s16 maxY = (s16)centerY + (s16)radius;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= (s16)map->width) maxX = (s16)map->width - 1;
    if (maxY >= (s16)map->height) maxY = (s16)map->height - 1;
    if ((minX > maxX) || (minY > maxY)) return;

    if (!region->active)
    {
        region->minX = (u16)minX;
        region->minY = (u16)minY;
        region->maxX = (u16)maxX;
        region->maxY = (u16)maxY;
        region->active = TRUE;
        return;
    }

    if (minX < (s16)region->minX) region->minX = (u16)minX;
    if (minY < (s16)region->minY) region->minY = (u16)minY;
    if (maxX > (s16)region->maxX) region->maxX = (u16)maxX;
    if (maxY > (s16)region->maxY) region->maxY = (u16)maxY;
}

static bool snapshotMatches(const FogUnitSnapshot *snapshot, const Unit *unit)
{
    if (unit == NULL) return !snapshot->valid;
    if (!snapshot->valid) return FALSE;

    return (snapshot->x == unit->x) && (snapshot->y == unit->y) &&
           (snapshot->visionRange == unit->visionRange) &&
           (snapshot->maxMovementPoints == unit->maxMovementPoints) &&
           (snapshot->id == unit->id) &&
           (snapshot->type == unit->type) &&
           (snapshot->team == unit->team) && (snapshot->alive == unit->alive);
}

static void storeSnapshot(FogUnitSnapshot *snapshot, const Unit *unit)
{
    if (unit == NULL)
    {
        snapshot->valid = FALSE;
        snapshot->alive = FALSE;
        return;
    }

    snapshot->x = unit->x;
    snapshot->y = unit->y;
    snapshot->visionRange = unit->visionRange;
    snapshot->maxMovementPoints = unit->maxMovementPoints;
    snapshot->id = unit->id;
    snapshot->type = unit->type;
    snapshot->team = unit->team;
    snapshot->alive = unit->alive;
    snapshot->valid = TRUE;
}

static void revealUnitVision(FogOfWar *fog, const FogUnitSnapshot *unit,
                             const FogDirtyRegion *clip)
{
    s16 minX = (s16)unit->x - (s16)unit->visionRange;
    s16 minY = (s16)unit->y - (s16)unit->visionRange;
    s16 maxX = (s16)unit->x + (s16)unit->visionRange;
    s16 maxY = (s16)unit->y + (s16)unit->visionRange;
    s16 x;
    s16 y;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= (s16)fog->map->width) maxX = (s16)fog->map->width - 1;
    if (maxY >= (s16)fog->map->height) maxY = (s16)fog->map->height - 1;

    if (clip != NULL)
    {
        if (minX < (s16)clip->minX) minX = (s16)clip->minX;
        if (minY < (s16)clip->minY) minY = (s16)clip->minY;
        if (maxX > (s16)clip->maxX) maxX = (s16)clip->maxX;
        if (maxY > (s16)clip->maxY) maxY = (s16)clip->maxY;
    }

    if ((minX > maxX) || (minY > maxY)) return;

    for (y = minY; y <= maxY; y++)
    {
        for (x = minX; x <= maxX; x++)
        {
            CellData *cell = MapData_getCell(fog->map, (u16)x, (u16)y);
            if ((cell != NULL) &&
                MapData_hasLineOfSight(fog->map, unit->x, unit->y,
                                       (u16)x, (u16)y))
                setPackedState(cell, unit->team, FOG_CELL_VISIBLE);
        }
    }
}

/* Se define después de recordDirtyCell(), y se usa tanto al iniciar como al actualizar. */
static void revealPlayerMovementAreas(FogOfWar *fog, bool trackDirty,
                                      const bool *unitMask);
/* Recálculo incremental de exploración por alcance de movimiento. */

static void markMovementUnitsNearCell(FogOfWar *fog, u16 x, u16 y);

static void rebuildTeamUnitIndex(FogOfWar *fog)
{
    u8 i;
    u8 team;

    for (team = 0; team < TEAM_COUNT; team++) fog->teamUnitCount[team] = 0;

    for (i = 0; i < fog->cachedUnitCount; i++)
    {
        const FogUnitSnapshot *unit = &fog->unitSnapshots[i];
        const u8 unitTeam = (u8)unit->team;
        u8 slot;

        if (!unit->valid || !unit->alive || (unitTeam >= TEAM_COUNT)) continue;
        slot = fog->teamUnitCount[unitTeam];
        if (slot >= FOG_MAX_UNITS) continue;
        fog->teamUnitIndices[unitTeam][slot] = i;
        fog->teamUnitCount[unitTeam] = (u8)(slot + 1);
    }
}

/* Pruebas de visibilidad con snapshots aliados. */

static bool isVisibleFromTeam(const FogOfWar *fog, TeamId team,
                              u16 x, u16 y, u8 excludedUnitIndex)
{
    u8 i;
    const u8 teamIndex = (u8)team;

    if (teamIndex >= TEAM_COUNT) return FALSE;

    /* Solo recorre unidades vivas del equipo consultado, normalmente 30 y no 60. */
    for (i = 0; i < fog->teamUnitCount[teamIndex]; i++)
    {
        const u8 unitIndex = fog->teamUnitIndices[teamIndex][i];
        const FogUnitSnapshot *unit = &fog->unitSnapshots[unitIndex];
        u16 deltaX;
        u16 deltaY;

        /* Ya se comprobó esta unidad al probar su nueva posición. */
        if (unitIndex == excludedUnitIndex) continue;
        deltaX = (unit->x > x)
            ? (u16)(unit->x - x) : (u16)(x - unit->x);
        deltaY = (unit->y > y)
            ? (u16)(unit->y - y) : (u16)(y - unit->y);

        if ((deltaX <= unit->visionRange) && (deltaY <= unit->visionRange) &&
            MapData_hasLineOfSight(fog->map, unit->x, unit->y, x, y))
            return TRUE;
    }
    return FALSE;
}

static bool snapshotCoversCell(const MapData *map,
                               const FogUnitSnapshot *unit, TeamId team,
                               u16 x, u16 y)
{
    u16 deltaX;
    u16 deltaY;

    if ((unit == NULL) || !unit->valid || !unit->alive ||
        (unit->team != team))
        return FALSE;

    deltaX = (unit->x > x) ? (u16)(unit->x - x) : (u16)(x - unit->x);
    deltaY = (unit->y > y) ? (u16)(unit->y - y) : (u16)(y - unit->y);
    return (deltaX <= unit->visionRange) && (deltaY <= unit->visionRange) &&
           MapData_hasLineOfSight(map, unit->x, unit->y, x, y);
}

static bool liveUnitCoversCell(const MapData *map, const Unit *unit,
                               TeamId team, u16 x, u16 y)
{
    u16 deltaX;
    u16 deltaY;

    if ((unit == NULL) || !unit->alive || (unit->team != team)) return FALSE;
    deltaX = (unit->x > x) ? (u16)(unit->x - x) : (u16)(x - unit->x);
    deltaY = (unit->y > y) ? (u16)(unit->y - y) : (u16)(y - unit->y);
    return (deltaX <= unit->visionRange) && (deltaY <= unit->visionRange) &&
           MapData_hasLineOfSight(map, unit->x, unit->y, x, y);
}

static void recordDirtyCell(FogOfWar *fog, u16 x, u16 y, bool tileChanged)
{
    const u32 index = (u32)y * fog->map->width + x;

    if ((index < fog->map->cellCapacity) &&
        (index < FOG_MAX_MAP_CELLS) &&
        (fog->dirtyCellCount < FOG_MAX_MAP_CELLS))
    {
        fog->dirtyCellIndices[fog->dirtyCellCount++] = (u16)index;
        if (tileChanged) fog->dirtyTileFlags[index] = TRUE;
    }
}

/* Envolvente inicial de exploración a partir del alcance aliado. */

static bool envelopePointLess(u16 a, u16 b, u16 width)
{
    const u16 ax = (u16)(a % width);
    const u16 ay = (u16)(a / width);
    const u16 bx = (u16)(b % width);
    const u16 by = (u16)(b / width);

    if (ax != bx) return ax < bx;
    return ay < by;
}

static s32 envelopeCross(u16 a, u16 b, u16 c, u16 width)
{
    const s32 ax = (s32)(a % width);
    const s32 ay = (s32)(a / width);
    const s32 bx = (s32)(b % width);
    const s32 by = (s32)(b / width);
    const s32 cx = (s32)(c % width);
    const s32 cy = (s32)(c / width);

    return ((bx - ax) * (cy - ay)) - ((by - ay) * (cx - ax));
}

static u16 buildMovementEnvelopeHull(u16 pointCount, u16 width)
{
    u16 i;
    u16 uniqueCount = 0;
    u16 hullCount = 0;
    u16 upperThreshold;

    if ((pointCount == 0) || (width == 0)) return 0;

    /* Inserción: como máximo 256 puntos, y esto solo se usa al inicializar. */
    for (i = 1; i < pointCount; i++)
    {
        const u16 key = movementEnvelopePoints[i];
        u16 j = i;
        while ((j > 0) && envelopePointLess(
                key, movementEnvelopePoints[j - 1], width))
        {
            movementEnvelopePoints[j] = movementEnvelopePoints[j - 1];
            j--;
        }
        movementEnvelopePoints[j] = key;
    }

    /* Quita los extremos repetidos compartidos por unidades próximas. */
    for (i = 0; i < pointCount; i++)
    {
        if ((uniqueCount == 0) ||
            (movementEnvelopePoints[i] != movementEnvelopePoints[uniqueCount - 1]))
            movementEnvelopePoints[uniqueCount++] = movementEnvelopePoints[i];
    }
    if (uniqueCount < 3) return uniqueCount;

    for (i = 0; i < uniqueCount; i++)
    {
        while ((hullCount >= 2) &&
            (envelopeCross(movementEnvelopeHull[hullCount - 2],
                           movementEnvelopeHull[hullCount - 1],
                           movementEnvelopePoints[i], width) <= 0))
            hullCount--;
        movementEnvelopeHull[hullCount++] = movementEnvelopePoints[i];
    }

    upperThreshold = (u16)(hullCount + 1);
    i = (u16)(uniqueCount - 1);
    while (i > 0)
    {
        const u16 point = movementEnvelopePoints[i - 1];
        while ((hullCount >= upperThreshold) &&
            (envelopeCross(movementEnvelopeHull[hullCount - 2],
                           movementEnvelopeHull[hullCount - 1],
                           point, width) <= 0))
            hullCount--;
        movementEnvelopeHull[hullCount++] = point;
        i--;
    }

    /* El último vértice repite el primero para cerrar el polígono. */
    if (hullCount > 1) hullCount--;
    return hullCount;
}

static bool cellInsideMovementEnvelope(u16 cellIndex, u16 hullCount,
                                       u16 width)
{
    u16 i;

    if (hullCount < 3) return FALSE;
    for (i = 0; i < hullCount; i++)
    {
        const u16 a = movementEnvelopeHull[i];
        const u16 b = movementEnvelopeHull[(u16)((i + 1) % hullCount)];
        /* La envolvente es antihoraria; su interior queda a la izquierda. */
        if (envelopeCross(a, b, cellIndex, width) < 0) return FALSE;
    }
    return TRUE;
}

static void revealInitialMovementEnvelope(FogOfWar *fog, u16 pointCount)
{
    const u32 cellCount = (u32)fog->map->width * fog->map->height;
    const u16 hullCount = buildMovementEnvelopeHull(pointCount,
                                                     fog->map->width);
    u32 index;

    if (hullCount < 3) return;

    /*
     * Cierra los huecos entre las áreas de movimiento aliadas. El área añadida
     * es solo EXPLORED (nunca VISIBLE): oculta enemigos y mantiene independiente
     * la visión táctica. Todo lo que queda fuera de esta envolvente conserva
     * su niebla. La pasada se hace una vez al iniciar, no durante cada frame.
     */
    for (index = 0; index < cellCount; index++)
    {
        CellData *cell;
        if (!cellInsideMovementEnvelope((u16)index, hullCount,
                                        fog->map->width))
            continue;
        cell = &fog->map->cells[index];
        if (getPackedState(cell, TEAM_BLUE) == FOG_CELL_UNSEEN)
            setPackedState(cell, TEAM_BLUE, FOG_CELL_EXPLORED);
    }
}

/*
 * Descubre casillas dentro del rango completo de movimiento del equipo azul
 * aunque ya haya gastado puntos este turno y aunque la línea de visión esté
 * bloqueada. Al inicio también se cierra el interior de la envolvente conjunta
 * de los alcances para que no queden bolsas de niebla
 * entre tropas aliadas. Las casillas añadidas se marcan EXPLORED, nunca VISIBLE.
 *
 * Para calcular esa envolvente, las aliadas se tratan temporalmente como huecos
 * de ocupación: un aliado no debe crear una bolsa de niebla al dividir el área
 * del grupo. Los enemigos ocultos también se enmascaran; los visibles siguen
 * bloqueando rutas. Se restaura toda ocupación al terminar. El movimiento real
 * sigue usando el pathfinder normal y continúa respetando unidades ocupantes.
 */
static void markMovementUnitsNearCell(FogOfWar *fog, u16 x, u16 y)
{
    u8 i;

    if (fog == NULL) return;
    for (i = 0; i < fog->cachedUnitCount; i++)
    {
        const FogUnitSnapshot *unit = &fog->unitSnapshots[i];
        const u16 dx = (unit->x > x) ? (u16)(unit->x - x) : (u16)(x - unit->x);
        const u16 dy = (unit->y > y) ? (u16)(unit->y - y) : (u16)(y - unit->y);

        /*
         * Quitar un ocupante solo puede ampliar el área explorada de unidades
         * que podrían llegar a esa casilla con su rango máximo. Manhattan es
         * una cota inferior conservadora porque cada paso cuesta >= 1.
         */
        if (unit->valid && unit->alive && (unit->team == TEAM_BLUE) &&
            (unit->maxMovementPoints > 0) &&
            ((u16)(dx + dy) <= unit->maxMovementPoints))
            fog->movementRecalcMask[i] = TRUE;
    }
}

/*
 * Las celdas liberadas por una unidad azul no requieren invalidar el alcance
 * de las otras azules: revealPlayerMovementAreas() las enmascara como huecos al
 * calcular cualquier rango. Encolar una búsqueda por cada celda del recorrido
 * provocaba búsquedas redundantes y bajadas de FPS durante varios fotogramas.
 * Solo se recalcula el área de la unidad azul que cambió de posición.
 */

static void revealPlayerMovementAreas(FogOfWar *fog, bool trackDirty,
                                      const bool *unitMask)
{
    PathfinderWorkspace *workspace;
    u32 cellCount;
    bool maskedOccupants[FOG_MAX_UNITS] = { FALSE };
    const bool buildInitialEnvelope = !trackDirty && (unitMask == NULL);
    u16 envelopePointCount = 0;
    u8 i;

    if ((fog == NULL) || (fog->map == NULL) || (fog->units == NULL) ||
        (fog->map->pathWorkspace == NULL) ||
        (UnitController_getCount(fog->units) > FOG_MAX_UNITS))
        return;

    cellCount = (u32)fog->map->width * fog->map->height;
    if ((cellCount == 0) || (cellCount > FOG_MAX_MAP_CELLS) ||
        (cellCount > fog->map->cellCapacity))
        return;

    workspace = fog->map->pathWorkspace;
    if ((workspace->reachable == NULL) || (workspace->capacity < cellCount))
        return;

    /*
     * Las aliadas no deben crear una bolsa de niebla entre áreas de movimiento.
     * Los enemigos solo se enmascaran si no son visibles; un enemigo visible
     * sigue siendo un obstáculo real para la envolvente de movimiento.
     */
    for (i = 0; i < fog->cachedUnitCount; i++)
    {
        const FogUnitSnapshot *occupant = &fog->unitSnapshots[i];
        CellData *cell;
        bool mayMask = FALSE;

        if (!occupant->valid || !occupant->alive) continue;
        if (occupant->team == TEAM_BLUE) mayMask = TRUE;
        else if ((occupant->team == TEAM_RED) &&
                 !FogOfWar_isVisible(fog->map, TEAM_BLUE,
                                     occupant->x, occupant->y))
            mayMask = TRUE;
        if (!mayMask) continue;

        cell = MapData_getCell(fog->map, occupant->x, occupant->y);
        if ((cell != NULL) && (cell->occupantId == (s8)occupant->id))
        {
            cell->occupantId = CELL_NO_OCCUPANT;
            maskedOccupants[i] = TRUE;
        }
    }

    for (i = 0; i < fog->cachedUnitCount; i++)
    {
        const FogUnitSnapshot *unit = &fog->unitSnapshots[i];
        const u8 *reachable;
        u16 minX;
        u16 maxX;
        u16 minY;
        u16 maxY;
        u16 y;

        if (!unit->valid || !unit->alive || (unit->team != TEAM_BLUE) ||
            (unit->maxMovementPoints == 0) ||
            ((unitMask != NULL) && !unitMask[i]))
            continue;

        u16 leftX = fog->map->width;
        u16 rightX = 0;
        u16 topY = fog->map->height;
        u16 bottomY = 0;
        u16 leftPoint = 0;
        u16 rightPoint = 0;
        u16 topPoint = 0;
        u16 bottomPoint = 0;
        bool haveReachable = FALSE;

        if (!Pathfinder_buildReachabilityForType(fog->map, workspace,
                unit->x, unit->y, (s8)unit->id, unit->type,
                unit->maxMovementPoints))
            continue;

        reachable = workspace->reachable;
        /* El rango máximo acota la distancia Manhattan: no hace falta barrer
           las 1.600 casillas por cada unidad que se recalcula. */
        minX = (unit->x > unit->maxMovementPoints)
            ? (u16)(unit->x - unit->maxMovementPoints) : 0;
        minY = (unit->y > unit->maxMovementPoints)
            ? (u16)(unit->y - unit->maxMovementPoints) : 0;
        {
            const u32 candidateMaxX = (u32)unit->x + unit->maxMovementPoints;
            const u32 candidateMaxY = (u32)unit->y + unit->maxMovementPoints;
            maxX = (candidateMaxX < fog->map->width)
                ? (u16)candidateMaxX : (u16)(fog->map->width - 1u);
            maxY = (candidateMaxY < fog->map->height)
                ? (u16)candidateMaxY : (u16)(fog->map->height - 1u);
        }

        for (y = minY; y <= maxY; y++)
        {
            u16 x;
            for (x = minX; x <= maxX; x++)
            {
                const u32 index = (u32)y * fog->map->width + x;
                CellData *cell;
                u8 oldState;

                if (reachable[index] == 0) continue;
                if (buildInitialEnvelope)
                {
                    const u16 point = (u16)index;
                    if (!haveReachable)
                    {
                        leftX = rightX = x;
                        topY = bottomY = y;
                        leftPoint = rightPoint = topPoint = bottomPoint = point;
                        haveReachable = TRUE;
                    }
                    else
                    {
                        if (x < leftX) { leftX = x; leftPoint = point; }
                        if (x > rightX) { rightX = x; rightPoint = point; }
                        if (y < topY) { topY = y; topPoint = point; }
                        if (y > bottomY) { bottomY = y; bottomPoint = point; }
                    }
                }
                cell = &fog->map->cells[index];
                oldState = getPackedState(cell, TEAM_BLUE);
                /* La exploración es persistente y nunca rebaja la visión actual. */
                if (oldState != FOG_CELL_UNSEEN) continue;

                setPackedState(cell, TEAM_BLUE, FOG_CELL_EXPLORED);
                if (trackDirty)
                    recordDirtyCell(fog, x, y, TRUE);
            }
        }

        if (buildInitialEnvelope && haveReachable)
        {
            movementEnvelopePoints[envelopePointCount++] = leftPoint;
            movementEnvelopePoints[envelopePointCount++] = rightPoint;
            movementEnvelopePoints[envelopePointCount++] = topPoint;
            movementEnvelopePoints[envelopePointCount++] = bottomPoint;
        }
    }

    /* No dejar nunca la ocupación lógica temporalmente alterada. */
    for (i = 0; i < fog->cachedUnitCount; i++)
    {
        const FogUnitSnapshot *occupant = &fog->unitSnapshots[i];
        CellData *cell;

        if (!maskedOccupants[i] || !occupant->valid || !occupant->alive)
            continue;
        cell = MapData_getCell(fog->map, occupant->x, occupant->y);
        if ((cell != NULL) && (cell->occupantId == CELL_NO_OCCUPANT))
            cell->occupantId = (s8)occupant->id;
    }

    if (buildInitialEnvelope)
        revealInitialMovementEnvelope(fog, envelopePointCount);
}

/*
 * El alcance de movimiento se consolida por tandas pequeñas: un pathfinding
 * de unidad por llamada, normalmente un fotograma. Al terminar una ruta esto
 * evita ejecutar de golpe 10-30 búsquedas que provocaban un tirón visible.
 * La unidad recién movida tiene prioridad para quitar su propia niebla enseguida;
 * las aliadas cuyo alcance pudo ampliarse al liberarse casillas se procesan
 * durante los fotogramas siguientes.
 */
static bool hasMovementRecalcMask(const FogOfWar *fog)
{
    u8 i;
    if (fog == NULL) return FALSE;
    for (i = 0; i < fog->cachedUnitCount; i++)
        if (fog->movementRecalcMask[i]) return TRUE;
    return FALSE;
}

static void revealPendingMovementAreaChunk(FogOfWar *fog)
{
    bool oneUnitMask[FOG_MAX_UNITS] = { FALSE };
    u8 processed;
    u8 i;

    if (fog == NULL) return;

    /* Un cambio de contexto o una alteración simultánea puede pedir todo el
       ejército. Expande la petición a máscaras, pero sigue ejecutándola por
       tandas para no congelar el fotograma que termina una animación. */
    if (fog->movementRecalcAllPending)
    {
        for (i = 0; i < FOG_MAX_UNITS; i++)
        {
            const FogUnitSnapshot *unit = (i < fog->cachedUnitCount)
                ? &fog->unitSnapshots[i] : NULL;
            fog->movementRecalcMask[i] = (unit != NULL) && unit->valid &&
                unit->alive && (unit->team == TEAM_BLUE) &&
                (unit->maxMovementPoints > 0);
        }
        fog->movementRecalcAllPending = FALSE;
        fog->movementExplorationPending = TRUE;
    }

    for (processed = 0; processed < FOG_MOVEMENT_RECALCS_PER_UPDATE; processed++)
    {
        u8 selected = FOG_NO_PRIORITY_UNIT;
        if ((fog->movementPriorityUnitIndex < fog->cachedUnitCount) &&
            fog->movementRecalcMask[fog->movementPriorityUnitIndex])
        {
            selected = fog->movementPriorityUnitIndex;
        }
        else
        {
            for (i = 0; i < fog->cachedUnitCount; i++)
            {
                if (fog->movementRecalcMask[i])
                {
                    selected = i;
                    break;
                }
            }
        }

        if (selected == FOG_NO_PRIORITY_UNIT) break;

        memset(oneUnitMask, 0, sizeof(oneUnitMask));
        oneUnitMask[selected] = TRUE;
        /* Consume antes de procesar para mantener la petición idempotente. */
        fog->movementRecalcMask[selected] = FALSE;
        if (fog->movementPriorityUnitIndex == selected)
            fog->movementPriorityUnitIndex = FOG_NO_PRIORITY_UNIT;
        revealPlayerMovementAreas(fog, TRUE, oneUnitMask);
    }

    fog->movementExplorationPending = hasMovementRecalcMask(fog) ||
        fog->movementRecalcAllPending;
    if (!fog->movementExplorationPending)
        fog->movementPriorityUnitIndex = FOG_NO_PRIORITY_UNIT;
}

/* Recalcular visión únicamente dentro de la región sucia. */

static void updateTeamRegion(FogOfWar *fog, TeamId team,
                             const FogDirtyRegion *region,
                             const FogUnitSnapshot *singleOldUnit,
                             const Unit *singleNewUnit,
                             bool singleUnitChange,
                             u8 singleChangedIndex)
{
    u16 x;
    u16 y;

    if (!region->active) return;

    for (y = region->minY; y <= region->maxY; y++)
    {
        for (x = region->minX; x <= region->maxX; x++)
        {
            CellData *cell;
            u8 oldState;
            u8 newState;

            /*
             * Para un único movimiento, las celdas fuera de los dos cuadrados
             * de visión no pueden cambiar. Las de intersección siguen visibles
             * por la misma unidad antes y después. No hagas ahí el scan de equipo.
             */
            bool oldCovers = FALSE;
            bool newCovers = FALSE;

            cell = MapData_getCell(fog->map, x, y);
            if (cell == NULL) continue;
            oldState = getPackedState(cell, team);
            newState = oldState;

            if (singleUnitChange)
            {
                /*
                 * Prueba primero la nueva posición. Si ya estaba visible y
                 * la unidad móvil sigue viéndola, no es posible que cambie el
                 * estado: ahorra la prueba LOS redundante desde la posición
                 * anterior, que se ejecutaba para cada celda de intersección.
                 */
                newCovers = liveUnitCoversCell(fog->map,
                    singleNewUnit, team, x, y);
                if (newCovers && (oldState == FOG_CELL_VISIBLE)) continue;

                oldCovers = snapshotCoversCell(fog->map,
                    singleOldUnit, team, x, y);
                if (!oldCovers && !newCovers) continue;
            }

            fog->lastProcessedCellCount++;
            /* La fuente móvil ya se comprobó arriba; no repitas su LOS al
               buscar cobertura de las demás unidades para una celda que deja. */
            if (singleUnitChange && newCovers)
                newState = FOG_CELL_VISIBLE;
            else if (isVisibleFromTeam(fog, team, x, y,
                    singleUnitChange ? singleChangedIndex : 0xFFu))
                newState = FOG_CELL_VISIBLE;
            else if ((oldState == FOG_CELL_VISIBLE) ||
                     (oldState == FOG_STATE_TEMPORARY))
                newState = FOG_CELL_EXPLORED;

            if (newState == oldState) continue;
            setPackedState(cell, team, newState);
            if (team == fog->viewerTeam)
            {
                const bool tileChanged =
                    (oldState == FOG_CELL_UNSEEN) !=
                    (newState == FOG_CELL_UNSEEN);
                recordDirtyCell(fog, x, y, tileChanged);
            }
        }
    }
}

/* Construcción inicial y API pública de actualización incremental. */

static bool rebuildInitialVision(FogOfWar *fog, TeamId viewerTeam)
{
    const u32 cellCount = (u32)fog->map->width * fog->map->height;
    u32 i;
    u8 unitIndex;
    const u8 count = UnitController_getCount(fog->units);

    if ((cellCount > FOG_MAX_MAP_CELLS) ||
        (fog->map->cellCapacity < cellCount) || (count > FOG_MAX_UNITS))
        return FALSE;

    /* Fija el equipo observador ANTES de revelar la zona de movimiento inicial.
     * revealInitialMovementArea() consulta fog->viewerTeam; asignarlo después
     * hacía que el primer cálculo dependiera del valor cero de la estructura. */
    fog->viewerTeam = viewerTeam;

    for (i = 0; i < cellCount; i++) fog->map->cells[i].visionState = 0;
    for (unitIndex = 0; unitIndex < count; unitIndex++)
    {
        const Unit *unit = UnitController_getUnit(fog->units, unitIndex);
        storeSnapshot(&fog->unitSnapshots[unitIndex], unit);
    }
    for (unitIndex = count; unitIndex < FOG_MAX_UNITS; unitIndex++)
        fog->unitSnapshots[unitIndex].valid = FALSE;

    fog->cachedUnitCount = count;
    rebuildTeamUnitIndex(fog);
    /* Primero se consolida la visión real de todas las unidades del mapa. */
    for (unitIndex = 0; unitIndex < count; unitIndex++)
    {
        const FogUnitSnapshot *unit = &fog->unitSnapshots[unitIndex];
        if (unit->valid && unit->alive && ((u8)unit->team < TEAM_COUNT))
            revealUnitVision(fog, unit, NULL);
    }

    /* Después se explora la unión del alcance de movimiento del jugador. */
    if (viewerTeam == TEAM_BLUE) revealPlayerMovementAreas(fog, FALSE, NULL);

    fog->movementExplorationPending = FALSE;
    fog->movementRecalcAllPending = FALSE;
    memset(fog->movementRecalcMask, 0, sizeof(fog->movementRecalcMask));
    fog->movementPriorityUnitIndex = FOG_NO_PRIORITY_UNIT;
    fog->initialized = TRUE;
    fog->dirtyCellCount = 0;
    fog->lastProcessedCellCount = 0;
    return TRUE;
}

bool FogOfWar_init(FogOfWar *fog, MapData *map,
                   const UnitController *units, TeamId viewerTeam)
{
    if ((fog == NULL) || (map == NULL) || (units == NULL) ||
        ((u8)viewerTeam >= TEAM_COUNT) || (map->cells == NULL) ||
        (map->width == 0) || (map->height == 0) ||
        (((u32)map->width * map->height) > FOG_MAX_MAP_CELLS) ||
        (map->cellCapacity < ((u32)map->width * map->height)) ||
        (UnitController_getCount(units) > FOG_MAX_UNITS))
        return FALSE;

    memset(fog, 0, sizeof(*fog));
    fog->map = map;
    fog->units = units;
    return rebuildInitialVision(fog, viewerTeam);
}

bool FogOfWar_update(FogOfWar *fog, TeamId viewerTeam)
{
    FogDirtyRegion dirtyRegions[TEAM_COUNT] = { { 0, 0, 0, 0, FALSE },
                                                 { 0, 0, 0, 0, FALSE } };
    const u8 currentCount = (fog != NULL && fog->units != NULL)
        ? UnitController_getCount(fog->units) : 0;
    const bool viewerChanged = (fog != NULL) &&
        (!fog->initialized || (fog->viewerTeam != viewerTeam));
    u8 compareCount;
    u8 i;
    u8 changedUnitCount = 0;
    u8 singleChangedIndex = 0xFFu;
    bool movementRelevantChanged = FALSE;
    bool movementTopologyChanged = FALSE;
    FogUnitSnapshot singleOldUnit;
    const Unit *singleNewUnit = NULL;
    bool singleUnitChange = FALSE;
    bool singleOldRedWasVisibleToBlue = FALSE;
    bool teamIndexChanged;
    u8 changedUnitMask[(FOG_MAX_UNITS + 7u) / 8u] = { 0 };
    u16 dirtyIndex;

    if ((fog == NULL) || (fog->map == NULL) || (fog->units == NULL) ||
        ((u8)viewerTeam >= TEAM_COUNT) ||
        (((u32)fog->map->width * fog->map->height) > FOG_MAX_MAP_CELLS) ||
        (currentCount > FOG_MAX_UNITS))
        return FALSE;

    if (!fog->initialized) return rebuildInitialVision(fog, viewerTeam);

    /* Limpia solo las marcas usadas en la actualización anterior. */
    for (dirtyIndex = 0; dirtyIndex < fog->dirtyCellCount; dirtyIndex++)
        fog->dirtyTileFlags[fog->dirtyCellIndices[dirtyIndex]] = FALSE;
    fog->dirtyCellCount = 0;
    fog->lastProcessedCellCount = 0;
    compareCount = (currentCount > fog->cachedUnitCount)
        ? currentCount : fog->cachedUnitCount;
    teamIndexChanged = currentCount != fog->cachedUnitCount;

    /* Conserva el área anterior y nueva de cada unidad que haya cambiado. */
    for (i = 0; i < compareCount; i++)
    {
        const Unit *unit = (i < currentCount)
            ? UnitController_getUnit(fog->units, i) : NULL;
        FogUnitSnapshot *oldSnapshot = &fog->unitSnapshots[i];
        const bool oldHasVision = oldSnapshot->valid && oldSnapshot->alive &&
            ((u8)oldSnapshot->team < TEAM_COUNT);
        const bool newHasVision = (unit != NULL) && unit->alive &&
            ((u8)unit->team < TEAM_COUNT);
        /* Los puntos de movimiento cambian al consumir acciones y al iniciar
           turnos, pero no alteran la visión táctica. Evita marcar regiones
           sucias y recorrer LOS cuando solo cambió el presupuesto de movimiento. */
        const bool visionRegionChanged = (oldHasVision != newHasVision) ||
            (oldHasVision && newHasVision &&
             ((oldSnapshot->x != unit->x) ||
              (oldSnapshot->y != unit->y) ||
              (oldSnapshot->team != unit->team) ||
              (oldSnapshot->visionRange != unit->visionRange)));

        if (snapshotMatches(oldSnapshot, unit)) continue;

        changedUnitMask[i >> 3] |= (u8)(1u << (i & 7u));

        /* La visión por sí sola no requiere volver a buscar alcances móviles. */
        if (oldSnapshot->valid != (unit != NULL))
        {
            teamIndexChanged = TRUE;
            movementTopologyChanged = TRUE;
            movementRelevantChanged = TRUE;
        }
        else if ((unit != NULL) && oldSnapshot->valid)
        {
            const bool topologyChangedForUnit =
                (oldSnapshot->x != unit->x) || (oldSnapshot->y != unit->y) ||
                (oldSnapshot->id != unit->id) ||
                (oldSnapshot->team != unit->team) ||
                (oldSnapshot->alive != unit->alive);
            if ((oldSnapshot->team != unit->team) ||
                (oldSnapshot->alive != unit->alive))
                teamIndexChanged = TRUE;
            if (topologyChangedForUnit)
            {
                movementTopologyChanged = TRUE;
                movementRelevantChanged = TRUE;
            }
            if (oldSnapshot->maxMovementPoints != unit->maxMovementPoints)
            {
                movementRelevantChanged = TRUE;
                if (unit->team == TEAM_BLUE && unit->alive)
                {
                    fog->movementRecalcMask[i] = TRUE;
                    fog->movementPriorityUnitIndex = i;
                }
            }
        }
        else if (unit != NULL)
        {
            teamIndexChanged = TRUE;
            movementTopologyChanged = TRUE;
            movementRelevantChanged = TRUE;
        }

        changedUnitCount++;
        if (changedUnitCount == 1)
        {
            const CellData *oldCell = oldSnapshot->valid
                ? MapData_getCellConst(fog->map, oldSnapshot->x,
                                       oldSnapshot->y) : NULL;
            singleOldUnit = *oldSnapshot;
            singleNewUnit = unit;
            singleChangedIndex = i;
            /* Captura la visibilidad ANTES de recalcular la región antigua.
               Si un rojo visible abandona una casilla durante su turno, solo
               las azules cercanas pueden necesitar explorar rutas nuevas. */
            singleOldRedWasVisibleToBlue = oldSnapshot->valid &&
                oldSnapshot->alive && (oldSnapshot->team == TEAM_RED) &&
                (oldCell != NULL) &&
                (getPackedState(oldCell, TEAM_BLUE) == FOG_CELL_VISIBLE);
        }
        else singleNewUnit = NULL;

        if (visionRegionChanged && oldHasVision)
            includeVisionRegion(&dirtyRegions[oldSnapshot->team], fog->map,
                oldSnapshot->x, oldSnapshot->y, oldSnapshot->visionRange);

        if (visionRegionChanged && newHasVision)
            includeVisionRegion(&dirtyRegions[unit->team], fog->map,
                unit->x, unit->y, unit->visionRange);
    }

    /* Actualiza solo los snapshots que han cambiado. Al mover una unidad, su
       entrada en teamUnitIndices sigue siendo la misma: reconstruir las listas
       de ambos bandos en cada llegada costaba otro recorrido completo del
       ejército sin aportar información nueva. Las altas, bajas y cambios de
       equipo/vida sí fuerzan una reconstrucción del índice. */
    if (changedUnitCount != 0)
    {
        if (currentCount != fog->cachedUnitCount)
        {
            for (i = 0; i < currentCount; i++)
                storeSnapshot(&fog->unitSnapshots[i],
                              UnitController_getUnit(fog->units, i));
            for (i = currentCount; i < FOG_MAX_UNITS; i++)
                fog->unitSnapshots[i].valid = FALSE;
            fog->cachedUnitCount = currentCount;
            teamIndexChanged = TRUE;
        }
        else if (changedUnitCount == 1)
        {
            storeSnapshot(&fog->unitSnapshots[singleChangedIndex],
                          singleNewUnit);
        }
        else
        {
            for (i = 0; i < currentCount; i++)
            {
                if (changedUnitMask[i >> 3] & (u8)(1u << (i & 7u)))
                    storeSnapshot(&fog->unitSnapshots[i],
                                  UnitController_getUnit(fog->units, i));
            }
        }
        if (teamIndexChanged) rebuildTeamUnitIndex(fog);
    }
    fog->viewerTeam = viewerTeam;
    singleUnitChange = (changedUnitCount == 1);

    for (i = 0; i < TEAM_COUNT; i++)
    {
        const FogDirtyRegion *region = &dirtyRegions[i];
        if (!region->active) continue;
        updateTeamRegion(fog, (TeamId)i, region,
                         singleUnitChange ? &singleOldUnit : NULL,
                         singleUnitChange ? singleNewUnit : NULL,
                         singleUnitChange,
                         singleUnitChange ? singleChangedIndex : 0xFFu);
    }

    /*
     * La exploración de movimiento es persistente. No reconstruyas por sistema
     * el alcance de las 60 azules al cambiar de equipo: durante el turno rojo
     * acumulamos únicamente las azules que podrían beneficiarse de una casilla
     * que un rojo VISIBLE acaba de abandonar. Así se evita ejecutar 60 búsquedas
     * de pathfinding al comienzo de cada turno azul, incluso si no hacen falta.
     *
     * Las azules se enmascaran como huecos durante el cálculo, de modo que mover
     * una azul solo exige recalcular su propio rango. Cambios simultáneos o altas
     * y bajas múltiples conservan el camino conservador de recálculo global.
     */
    if (movementTopologyChanged)
    {
        if (changedUnitCount != 1)
        {
            /* Varios cambios juntos: no arriesgar áreas de movimiento omitidas. */
            fog->movementRecalcAllPending = TRUE;
            fog->movementExplorationPending = TRUE;
        }
        else
        {
            const bool oldCellVacated = singleOldUnit.valid &&
                singleOldUnit.alive &&
                ((singleNewUnit == NULL) || !singleNewUnit->alive ||
                 (singleOldUnit.x != singleNewUnit->x) ||
                 (singleOldUnit.y != singleNewUnit->y) ||
                 (singleOldUnit.team != singleNewUnit->team) ||
                 (singleOldUnit.id != singleNewUnit->id));

            if (oldCellVacated && (singleOldUnit.team == TEAM_RED) &&
                singleOldRedWasVisibleToBlue)
            {
                markMovementUnitsNearCell(fog, singleOldUnit.x,
                                          singleOldUnit.y);
                fog->movementExplorationPending = TRUE;
            }

            if ((viewerTeam == TEAM_BLUE) &&
                (singleChangedIndex < fog->cachedUnitCount) &&
                fog->unitSnapshots[singleChangedIndex].valid &&
                fog->unitSnapshots[singleChangedIndex].alive &&
                (fog->unitSnapshots[singleChangedIndex].team == TEAM_BLUE))
            {
                fog->movementRecalcMask[singleChangedIndex] = TRUE;
                fog->movementPriorityUnitIndex = singleChangedIndex;
                fog->movementExplorationPending = TRUE;
            }
        }
    }

    if (viewerTeam == TEAM_BLUE)
    {
        /* Un cambio de observador no implica que cambie el terreno alcanzable.
           Se procesan las candidatas acumuladas; solo eventos estructurales
           múltiples han solicitado expresamente un recálculo completo. */
        if (movementRelevantChanged || viewerChanged ||
            hasMovementRecalcMask(fog) || fog->movementRecalcAllPending)
            fog->movementExplorationPending = TRUE;

        if (!UnitController_isMoving(fog->units) &&
            (fog->movementExplorationPending || fog->movementRecalcAllPending))
            revealPendingMovementAreaChunk(fog);
    }
    else
    {
        /* Durante el turno rojo no proceses pathfinding azul. Conserva las
           candidatas hasta volver al azul, en lugar de descartarlas y forzar
           allí un barrido completo por un mero cambio de observador. */
        if (hasMovementRecalcMask(fog) || fog->movementRecalcAllPending)
            fog->movementExplorationPending = TRUE;
    }

    return viewerChanged || (fog->dirtyCellCount > 0);
}

u16 FogOfWar_getDirtyCellCount(const FogOfWar *fog)
{
    return (fog != NULL) ? fog->dirtyCellCount : 0;
}

bool FogOfWar_getDirtyCell(const FogOfWar *fog, u16 dirtyIndex,
                           u16 *x, u16 *y)
{
    u16 mapWidth;
    u16 index;

    if ((fog == NULL) || (fog->map == NULL) || (x == NULL) || (y == NULL) ||
        (dirtyIndex >= fog->dirtyCellCount))
        return FALSE;

    mapWidth = fog->map->width;
    if (mapWidth == 0) return FALSE;
    index = fog->dirtyCellIndices[dirtyIndex];
    *x = (u16)(index % mapWidth);
    *y = (u16)(index / mapWidth);
    return TRUE;
}

bool FogOfWar_didTileChange(const FogOfWar *fog, u16 dirtyIndex)
{
    u16 cellIndex;

    if ((fog == NULL) || (dirtyIndex >= fog->dirtyCellCount)) return FALSE;
    cellIndex = fog->dirtyCellIndices[dirtyIndex];
    if (cellIndex >= FOG_MAX_MAP_CELLS) return FALSE;
    return fog->dirtyTileFlags[cellIndex] != 0;
}

u16 FogOfWar_getLastProcessedCellCount(const FogOfWar *fog)
{
    return (fog != NULL) ? fog->lastProcessedCellCount : 0;
}


bool FogOfWar_hasPendingMovementExploration(const FogOfWar *fog)
{
    return (fog != NULL) && (fog->movementExplorationPending ||
        fog->movementRecalcAllPending);
}
