#include <assert.h>
#include <stdio.h>
#include "fog_of_war.h"
#include "combat.h"
#include "map_data.h"
#include "pathfinder.h"
#include "unit_controller.h"
#include "cell_events.h"


static void testTerrainBlocksVisionBehindIt(void)
{
    static const char *const rows[] = {
        "PPPPPPP", "PPPPPPP", "PPPRPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 2, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 6, 4, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    units[0].visionRange = 4;
    units[1].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 2, 2));
    /* La propia roca es visible, pero la línea recta tras ella queda oculta. */
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 3, 2));
    /* Las casillas alcanzables tras la roca quedan exploradas para moverse,
       pero siguen sin visión táctica mientras la LOS esté bloqueada. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 4, 2));
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 5, 2));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 4, 2));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 5, 2));
    /* La oclusión no filtra el conocimiento ni altera el estado del rival. */
    assert(FogOfWar_isVisible(&map, TEAM_RED, 6, 4));
    assert(!FogOfWar_isExplored(&map, TEAM_RED, 1, 2));

    /* Al desplazarse al norte, la unidad descubre la zona tras la roca por
       una línea nueva; al volver, la casilla queda explorada, no visible. */
    units[0].x = 2;
    units[0].y = 0;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 5, 2));
    units[0].x = 1;
    units[0].y = 2;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_getCellState(&map, TEAM_BLUE, 5, 2) == FOG_CELL_EXPLORED);
}

static void testTeamVisionAndExploration(void)
{
    static const char *const rows[] = {
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP"
    };
    CellData cells[81];
    u8 costs[81], parents[81], reachable[81];
    PathfinderWorkspace workspace = { costs, parents, 81, reachable };
    MapData map = { .width = 9, .height = 9, .terrainRows = rows,
        .terrainRowCount = 9, .cells = cells, .cellCapacity = 81,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 8, 8, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 1, 1));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 4, 4));
    assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 8, 8));
    assert(FogOfWar_isVisible(&map, TEAM_RED, 8, 8));
    assert(!FogOfWar_isExplored(&map, TEAM_RED, 1, 1));

    /* El territorio ya explorado se conserva aunque deje de estar visible. */
    units[0].x = 5;
    units[0].y = 1;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_getLastProcessedCellCount(&fog) < map.width * map.height);
    assert(FogOfWar_getDirtyCellCount(&fog) > 0);
    assert(FogOfWar_getDirtyCellCount(&fog) < map.width * map.height);
    {
        u16 i;
        u16 tileChanges = 0;
        for (i = 0; i < FogOfWar_getDirtyCellCount(&fog); i++)
            if (FogOfWar_didTileChange(&fog, i)) tileChanges++;
        assert(tileChanges > 0);
        assert(tileChanges < FogOfWar_getDirtyCellCount(&fog));
    }
    assert(FogOfWar_getCellState(&map, TEAM_BLUE, 1, 1) == FOG_CELL_EXPLORED);
    assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 8, 8));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 1, 1));

    /* Cambiar de jugador restaura la visión independiente de su equipo. */
    assert(FogOfWar_update(&fog, TEAM_RED));
    assert(FogOfWar_isVisible(&map, TEAM_RED, 8, 8));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 1, 1) == FALSE);

    /* Sin cambios en unidades, una actualización no recorre el mapa lógico. */
    assert(!FogOfWar_update(&fog, TEAM_RED));
    assert(FogOfWar_getLastProcessedCellCount(&fog) == 0);
    assert(FogOfWar_getDirtyCellCount(&fog) == 0);

    /* Consumir/restaurar puntos de movimiento no cambia la visión táctica.
       Debe actualizarse el snapshot sin recorrer regiones ni hacer pruebas LOS. */
    units[0].maxMovementPoints--;
    assert(!FogOfWar_update(&fog, TEAM_RED));
    assert(FogOfWar_getLastProcessedCellCount(&fog) == 0);
    assert(FogOfWar_getDirtyCellCount(&fog) == 0);
}


static void testOverlappingVisionSurvivesOneUnitMovingAway(void)
{
    static const char *const rows[] = {
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[40];
    u8 costs[40], parents[40], reachable[40];
    PathfinderWorkspace workspace = { costs, parents, 40, reachable };
    MapData map = { .width = 8, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 40,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 2, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 2, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 7, 4, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    units[0].visionRange = 1;
    units[1].visionRange = 1;
    units[2].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 2, 2));

    /* Al mover la primera unidad, la segunda aún revela la zona solapada. */
    units[0].x = 0;
    units[0].y = 0;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 2, 2));
    assert(FogOfWar_getCellState(&map, TEAM_BLUE, 1, 2) == FOG_CELL_EXPLORED);
}



static void testSingleCellMoveOnlyScansLocalRegion(void)
{
    static char terrainRow[41];
    static const char *terrainRows[40];
    static CellData cells[1600];
    static u8 costs[1600], parents[1600], reachable[1600];
    PathfinderWorkspace workspace = { costs, parents, 1600, reachable };
    MapData map = { .width = 40, .height = 40, .terrainRows = terrainRows,
        .terrainRowCount = 40, .cells = cells, .cellCapacity = 1600,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 20, 20, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 39, 39, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    u16 i;

    for (i = 0; i < 40; i++) terrainRow[i] = 'P';
    terrainRow[40] = '\0';
    for (i = 0; i < 40; i++) terrainRows[i] = terrainRow;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    units[0].visionRange = 4;
    units[1].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    units[0].x++;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    /*
     * Un paso horizontal solo necesita reevaluar la diferencia simétrica:
     * dos bandas de 9 celdas. La intersección sigue visible por esa unidad.
     */
    assert(FogOfWar_getLastProcessedCellCount(&fog) == 18);
    assert(FogOfWar_getLastProcessedCellCount(&fog) < 1600);
}

static void testLargeDirtyRegionUsesWideCounter(void)
{
    static char terrainRow[41];
    static const char *terrainRows[40];
    static CellData cells[1600];
    static u8 costs[1600], parents[1600], reachable[1600];
    PathfinderWorkspace workspace = { costs, parents, 1600, reachable };
    MapData map = { .width = 40, .height = 40, .terrainRows = terrainRows,
        .terrainRowCount = 40, .cells = cells, .cellCapacity = 1600,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 5, 5, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 39, 39, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    u16 i;

    for (i = 0; i < 40; i++) terrainRow[i] = 'P';
    terrainRow[40] = '\0';
    for (i = 0; i < 40; i++) terrainRows[i] = terrainRow;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    units[0].visionRange = 10;
    units[1].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    /* El movimiento de una unidad descubre/retira más de 255 estados. */
    units[0].x = 25;
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_getDirtyCellCount(&fog) > 255);
    assert(FogOfWar_getLastProcessedCellCount(&fog) < 1600);

    /* El siguiente update limpia todas las marcas previas sin desbordarse. */
    assert(!FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_getDirtyCellCount(&fog) == 0);
    assert(FogOfWar_getLastProcessedCellCount(&fog) == 0);
}

static void testHiddenEnemyDoesNotLeakIntoMovementPreview(void)
{
    static const char *const rows[] = {
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP"
    };
    CellData cells[45];
    u8 costs[45], parents[45], reachable[45];
    PathfinderWorkspace workspace = { costs, parents, 45, reachable };
    MapData map = { .width = 9, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 45,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 2, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 5, 2, 0, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const u8 *range;
    UnitActionResult action;
    bool previewReachable;
    u16 previewSteps;
    u8 previewCost;
    u8 movementPoints;
    s16 i;
    UnitActionResult lastAction = UNIT_ACTION_INVALID;
    CombatResult combatResult;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    UnitController_setFogEnabled(&controller, TRUE);
    /* El alcance de movimiento quita el bloqueo de niebla de la casilla,
       pero no revela la unidad enemiga que continúa oculta allí. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 5, 2));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 5, 2));
    assert(UnitController_activate(&controller, 5, 2) == UNIT_ACTION_NO_UNIT);
    combatResult = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(combatResult.code == COMBAT_RESULT_NOT_VISIBLE);
    assert(units[1].health == units[1].maxHealth);

    assert(UnitController_activate(&controller, 1, 2) == UNIT_ACTION_SELECTED);
    range = UnitController_getMovementRange(&controller);
    assert(range != NULL && range[2 * map.width + 5] != 0);
    assert(MapData_getCellConst(&map, 5, 2)->occupantId == 1);

    assert(UnitController_updatePreview(&controller, 5, 2));
    assert(UnitController_getPreviewInfo(&controller, &previewReachable,
        &previewSteps, &previewCost, &movementPoints));
    assert(previewReachable && previewCost == 4 && movementPoints == 6);

    action = UnitController_activate(&controller, 5, 2);
    assert(action == UNIT_ACTION_ROUTE_READY);
    action = UnitController_activate(&controller, 5, 2);
    assert(action == UNIT_ACTION_MOVING);

    for (i = 0; i < 80 && UnitController_isMoving(&controller); i++)
        lastAction = UnitController_update(&controller);

    assert(!UnitController_isMoving(&controller));
    assert(units[0].x == 4 && units[0].y == 2);
    assert(units[1].alive);
    assert(MapData_getCellConst(&map, 5, 2)->occupantId == 1);
    assert(lastAction == UNIT_ACTION_BLOCKED);

    /* Una vez descubierta la unidad, ya no se trata como terreno vacío. */
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 5, 2));
    UnitController_invalidateMovementRange(&controller);
    assert(UnitController_refreshMovementRange(&controller));
    range = UnitController_getMovementRange(&controller);
    assert(range[2 * map.width + 5] == 0);
    assert(UnitController_activate(&controller, 5, 2) == UNIT_ACTION_ENEMY_UNIT);
}

static void testMovementExplorationUnitesAllFriendlyUnits(void)
{
    static const char *const rows[] = {
        "PPPPPPPPPPPPPP", "PPPPPPPPPPPPPP", "PPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPP", "PPPPPPPPPPPPPP"
    };
    CellData cells[70];
    u8 costs[70], parents[70], reachable[70];
    PathfinderWorkspace workspace = { costs, parents, 70, reachable };
    MapData map = { .width = 14, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 70,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 2, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 12, 2, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 13, 4, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    units[0].visionRange = 1;
    units[1].visionRange = 1;
    units[2].visionRange = 1;
    units[0].movementPoints = 2;
    units[1].movementPoints = 2;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    /* El segundo aliado aporta su alcance aunque esté lejos de la primera unidad. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 10, 2));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 10, 2));
    /* La envolvente conjunta de los alcances aliados cierra la bolsa central:
       no queda una isla de niebla entre unidades del mismo color. Esto solo
       descubre terreno; no concede visión táctica ni revela al enemigo. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 7, 2));
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 7, 1));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 7, 1));
}

static void testNewlyReachableAreaIsExploredWhenVisibleEnemyLeavesCorridor(void)
{
    static const char *const rows[] = {
        "WWWWWWW", "PPPPPPP", "WWWPWWW"
    };
    CellData cells[21];
    u8 costs[21], parents[21], reachable[21];
    PathfinderWorkspace workspace = { costs, parents, 21, reachable };
    MapData map = { .width = 7, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 21,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 1, 1, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    CellData *oldCell;
    CellData *newCell;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    units[0].visionRange = 2;
    units[1].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    /* El enemigo visible bloquea el único pasillo: x=6 queda sin explorar. */
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 3, 1));
    assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 6, 1));

    oldCell = MapData_getCell(&map, 3, 1);
    newCell = MapData_getCell(&map, 3, 2);
    assert(oldCell != NULL && newCell != NULL);
    assert(oldCell->occupantId == 1);
    assert(MapData_setOccupant(&map, 3, 1, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(&map, 3, 2, 1));
    units[1].x = 3;
    units[1].y = 2;

    /* Al abandonar su celda, el enemigo ya no bloquea el corredor. Se vuelve
       a calcular solo el rango azul que podía beneficiarse de esa liberación. */
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 6, 1));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 6, 1));
    assert(oldCell->occupantId == CELL_NO_OCCUPANT);
    assert(newCell->occupantId == 1);
    assert(!FogOfWar_hasPendingMovementExploration(&fog));
}

static void testMovementExplorationUsesFullRangeAfterMovementWithNoPointsLeft(void)
{
    static const char *const rows[] = {
        "PPPPPPPPPPPPPPPPPPPP", "PPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPP", "PPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPP", "PPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPP", "PPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPP"
    };
    CellData cells[180];
    u8 costs[180], parents[180], reachable[180];
    PathfinderWorkspace workspace = { costs, parents, 180, reachable };
    MapData map = { .width = 20, .height = 9, .terrainRows = rows,
        .terrainRowCount = 9, .cells = cells, .cellCapacity = 180,
        .pathWorkspace = &workspace };
    Unit units[1];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 2, 4, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    units[0].visionRange = 1;
    assert(units[0].maxMovementPoints == 6);
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    /* El rango inicial termina en x=8; la zona más allá continúa oculta. */
    assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 14, 4));

    /* Simula una ruta hasta x=8 que consume todos los puntos de este turno. */
    assert(MapData_setOccupant(&map, 2, 4, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(&map, 8, 4, 0));
    units[0].x = 8;
    units[0].movementPoints = 0;

    assert(FogOfWar_update(&fog, TEAM_BLUE));
    /* La exploración usa el rango máximo desde la nueva celda, aunque haya
       gastado todos los puntos actuales. La visión táctica sigue siendo corta. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 14, 4));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 14, 4));
    assert(FogOfWar_getDirtyCellCount(&fog) > 0);
}

static void testMovementOfOneAllyDoesNotQueueRedundantAllySearches(void)
{
    static const char *const rows[] = {
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP"
    };
    CellData cells[81];
    u8 costs[81], parents[81], reachable[81];
    PathfinderWorkspace workspace = { costs, parents, 81, reachable };
    MapData map = { .width = 9, .height = 9, .terrainRows = rows,
        .terrainRowCount = 9, .cells = cells, .cellCapacity = 81,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 4, 8, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 8, 8, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    u8 i;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    /* La nueva posición amplía el alcance de la unidad 0 hasta x=8. */
    assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 8, 1));
    assert(MapData_setOccupant(&map, 1, 1, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(&map, 2, 1, 0));
    units[0].x = 2;
    (void)FogOfWar_update(&fog, TEAM_BLUE);

    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 8, 1));
    /* La unidad movida se procesa inmediatamente; las demás no generan
       búsquedas adicionales en fotogramas sucesivos. */
    assert(!FogOfWar_hasPendingMovementExploration(&fog));
    for (i = 0; i < 3; i++) assert(!fog.movementRecalcMask[i]);
    assert(MapData_getCellConst(&map, 2, 1)->occupantId == 0);
    assert(MapData_getCellConst(&map, 1, 1)->occupantId == CELL_NO_OCCUPANT);
}

static void testMovementExplorationDoesNotQueueRedundantAllySearches(void)
{
    static const char *const rows[] = {
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP",
        "PPPPPPPPP", "PPPPPPPPP", "PPPPPPPPP"
    };
    CellData cells[81];
    u8 costs[81], parents[81], reachable[81];
    PathfinderWorkspace workspace = { costs, parents, 81, reachable };
    MapData map = { .width = 9, .height = 9, .terrainRows = rows,
        .terrainRowCount = 9, .cells = cells, .cellCapacity = 81,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 4, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 1, 4, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 3, 8, 8, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    u8 i;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));

    assert(MapData_setOccupant(&map, 1, 1, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(&map, 2, 1, 0));
    units[0].x = 2;
    (void)FogOfWar_update(&fog, TEAM_BLUE);

    assert(!FogOfWar_hasPendingMovementExploration(&fog));
    for (i = 0; i < 4; i++) assert(!fog.movementRecalcMask[i]);
    assert(MapData_getCellConst(&map, 2, 1)->occupantId == 0);
    assert(MapData_getCellConst(&map, 1, 1)->occupantId == CELL_NO_OCCUPANT);
}


static void testRedTurnAccumulatesOnlyAffectedBlueMovementRanges(void)
{
    static const char *const rows[] = {
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP",
        "PPPPPPPPPPPPPPPPPPPPPPPPP"
    };
    CellData cells[300];
    u8 costs[300], parents[300], reachable[300];
    PathfinderWorkspace workspace = { costs, parents, 300, reachable };
    MapData map = { .width = 25, .height = 12, .terrainRows = rows,
        .terrainRowCount = 12, .cells = cells, .cellCapacity = 300,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    FogOfWar fog;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 8, 5, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 22, 10, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 3, 9, 5, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    CellData *oldCell;
    CellData *newCell;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    units[0].visionRange = 1;
    units[1].visionRange = 6;
    units[2].visionRange = 1;
    units[3].visionRange = 1;
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    assert(FogOfWar_isVisible(&map, TEAM_BLUE, 9, 5));

    /* Al pasar al turno rojo no se encolan de entrada las tres rangos azules. */
    assert(FogOfWar_update(&fog, TEAM_RED));
    assert(!fog.movementRecalcAllPending);
    assert(!FogOfWar_hasPendingMovementExploration(&fog));

    /* El rojo visible abandona una casilla; solo el azul cercano puede tener
       un rango nuevo. Los otros dos están demasiado lejos para alcanzar ese
       cuello de botella con sus puntos de movimiento. */
    oldCell = MapData_getCell(&map, 9, 5);
    newCell = MapData_getCell(&map, 10, 5);
    assert(oldCell != NULL && newCell != NULL);
    assert(MapData_setOccupant(&map, 9, 5, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(&map, 10, 5, 3));
    units[3].x = 10;
    assert(FogOfWar_update(&fog, TEAM_RED));
    assert(!fog.movementRecalcAllPending);
    assert(fog.movementRecalcMask[1]);
    assert(!fog.movementRecalcMask[0]);
    assert(!fog.movementRecalcMask[2]);
    assert(FogOfWar_hasPendingMovementExploration(&fog));

    /* Al volver al azul procesa la única unidad afectada, no todo el ejército. */
    assert(FogOfWar_update(&fog, TEAM_BLUE));
    assert(!fog.movementRecalcAllPending);
    assert(!FogOfWar_hasPendingMovementExploration(&fog));
    assert(!fog.movementRecalcMask[0]);
    assert(!fog.movementRecalcMask[1]);
    assert(!fog.movementRecalcMask[2]);
    assert(oldCell->occupantId == CELL_NO_OCCUPANT);
    assert(newCell->occupantId == 3);
}


static void testMovementExplorationHonorsUnitTerrainProfile(void)
{
    static const char *const rows[] = { "PFFFFF" };
    CellData cells[6];
    u8 costs[6], parents[6], reachable[6];
    PathfinderWorkspace workspace = { costs, parents, 6, reachable };
    MapData map = { .width = 6, .height = 1, .terrainRows = rows,
        .terrainRowCount = 1, .cells = cells, .cellCapacity = 6,
        .pathWorkspace = &workspace };
    Unit units[1];
    UnitController controller;
    FogOfWar fog;
    const UnitStart start[] = {
        { 0, 0, 0, 0, UNIT_TYPE_SCOUT, TEAM_BLUE }
    };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 1, start, 1));
    assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
    /* El scout atraviesa bosque a coste 1 y alcanza la última celda con sus
       ocho puntos; allí la visión normal ya no llega, pero sí la exploración. */
    assert(FogOfWar_isExplored(&map, TEAM_BLUE, 5, 0));
    assert(!FogOfWar_isVisible(&map, TEAM_BLUE, 5, 0));

    /* Reiniciar el mapa con infantería: bosque cuesta 2 y el alcance llega
       solo hasta x=3 con seis puntos, así que x=4 y x=5 quedan desconocidas. */
    {
        const UnitStart infantry[] = {
            { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE }
        };
        assert(MapData_init(&map));
        assert(UnitController_init(&controller, &map, units, 1, infantry, 1));
        assert(FogOfWar_init(&fog, &map, &controller, TEAM_BLUE));
        assert(FogOfWar_isExplored(&map, TEAM_BLUE, 3, 0));
        assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 4, 0));
        assert(!FogOfWar_isExplored(&map, TEAM_BLUE, 5, 0));
    }
}

int main(void)
{
    testTeamVisionAndExploration();
    testTerrainBlocksVisionBehindIt();
    testOverlappingVisionSurvivesOneUnitMovingAway();
    testSingleCellMoveOnlyScansLocalRegion();
    testLargeDirtyRegionUsesWideCounter();
    testHiddenEnemyDoesNotLeakIntoMovementPreview();
    testMovementExplorationUnitesAllFriendlyUnits();
    testNewlyReachableAreaIsExploredWhenVisibleEnemyLeavesCorridor();
    testMovementExplorationUsesFullRangeAfterMovementWithNoPointsLeft();
    testMovementOfOneAllyDoesNotQueueRedundantAllySearches();
    testMovementExplorationDoesNotQueueRedundantAllySearches();
    testRedTurnAccumulatesOnlyAffectedBlueMovementRanges();
    testMovementExplorationHonorsUnitTerrainProfile();
    puts("PASS: niebla incremental, oclusión por terreno, área móvil explorada,\n"
         "      exploración persistente y ocultación de enemigos");
    return 0;
}
