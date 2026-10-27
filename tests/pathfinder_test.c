#include <assert.h>
#include <stdio.h>
#include "pathfinder.h"
#include "unit_types.h"

static void testWeightedRoutePrefersLowerCost(void)
{
    static const char *const rows[] =
    {
        "PPPPP",
        "PFFFP",
        "PPPPP"
    };
    CellData cells[15];
    u8 costs[15];
    u8 parents[15];
    PathfinderWorkspace workspace = { costs, parents, 15, NULL };
    MapData map =
    {
        .width = 5, .height = 3,
        .terrainRows = rows, .terrainRowCount = 3,
        .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(MapData_init(&map));
    assert(Pathfinder_findRoute(&map, &workspace, 0, 1, 4, 1,
                               CELL_NO_OCCUPANT, 6, &steps, &totalCost));
    assert(steps == 6);
    assert(totalCost == 6);
    assert(Pathfinder_getStep(&workspace, steps, 0) == PATH_DIR_UP);
    assert(Pathfinder_getStep(&workspace, steps, 1) == PATH_DIR_RIGHT);
    assert(Pathfinder_getStep(&workspace, steps, 5) == PATH_DIR_DOWN);

    assert(!Pathfinder_findRoute(&map, &workspace, 0, 1, 4, 1,
                                CELL_NO_OCCUPANT, 5, &steps, &totalCost));
}

static void testImpassableAndOccupiedCells(void)
{
    static const char *const rows[] = { "PWP", "PPP" };
    CellData cells[6];
    u8 costs[6];
    u8 parents[6];
    PathfinderWorkspace workspace = { costs, parents, 6, NULL };
    MapData map =
    {
        .width = 3, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 6,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(MapData_init(&map));
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                 CELL_NO_OCCUPANT, 2, &steps, &totalCost));
    assert(Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                CELL_NO_OCCUPANT, 4, &steps, &totalCost));
    assert(steps == 4 && totalCost == 4);

    assert(MapData_setOccupant(&map, 1, 1, 7));
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                 CELL_NO_OCCUPANT, 4, &steps, &totalCost));
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 1, 1,
                                 CELL_NO_OCCUPANT, 6, &steps, &totalCost));
}

static void testTerrainEntryCostsAndBudgetBoundaries(void)
{
    static const char *const rows[] = { "PFP" };
    CellData cells[3];
    u8 costs[3];
    u8 parents[3];
    PathfinderWorkspace workspace = { costs, parents, 3, NULL };
    MapData map =
    {
        .width = 3, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 3,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(MapData_init(&map));

    /* Se cobra el terreno al entrar: bosque (2) + camino (1) = 3. */
    assert(Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                CELL_NO_OCCUPANT, 3, &steps, &totalCost));
    assert(steps == 2);
    assert(totalCost == 3);
    assert(Pathfinder_getStep(&workspace, steps, 0) == PATH_DIR_RIGHT);
    assert(Pathfinder_getStep(&workspace, steps, 1) == PATH_DIR_RIGHT);

    /* El presupuesto justo vale; una unidad por debajo no debe pasar. */
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                 CELL_NO_OCCUPANT, 2, &steps, &totalCost));
    assert(steps == 0 && totalCost == 0);

    /* Piedras también cuestan 2: bosque (2) + piedras (2) = 4. */
    cells[2].terrain = TERRAIN_ROCK;
    assert(Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                CELL_NO_OCCUPANT, 4, &steps, &totalCost));
    assert(steps == 2 && totalCost == 4);
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 2, 0,
                                 CELL_NO_OCCUPANT, 3, &steps, &totalCost));
    assert(steps == 0 && totalCost == 0);
}

static void testRouteDoesNotWrapAtRowEdges(void)
{
    static const char *const rows[] = { "PP", "PP" };
    CellData cells[4];
    u8 costs[4];
    u8 parents[4];
    PathfinderWorkspace workspace = { costs, parents, 4, NULL };
    MapData map =
    {
        .width = 2, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 4,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(MapData_init(&map));
    assert(Pathfinder_findRoute(&map, &workspace, 1, 0, 0, 1,
                                CELL_NO_OCCUPANT, 2, &steps, &totalCost));
    assert(steps == 2 && totalCost == 2);
    /* Hay dos rutas ortogonales válidas; ninguna debe salir del tablero. */
    if (Pathfinder_getStep(&workspace, steps, 0) == PATH_DIR_DOWN)
        assert(Pathfinder_getStep(&workspace, steps, 1) == PATH_DIR_LEFT);
    else
    {
        assert(Pathfinder_getStep(&workspace, steps, 0) == PATH_DIR_LEFT);
        assert(Pathfinder_getStep(&workspace, steps, 1) == PATH_DIR_DOWN);
    }
}

static void testWorkspaceAndCoordinatesAreValidated(void)
{
    static const char *const rows[] = { "PP", "PP" };
    CellData cells[4];
    u8 costs[3];
    u8 parents[3];
    PathfinderWorkspace smallWorkspace = { costs, parents, 3, NULL };
    MapData map =
    {
        .width = 2, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 4,
        .pathWorkspace = &smallWorkspace
    };
    u16 steps;
    u8 cost;

    assert(MapData_init(&map));
    assert(!Pathfinder_findRoute(&map, &smallWorkspace, 0, 0, 1, 1,
                                 CELL_NO_OCCUPANT, 4, &steps, &cost));
    assert(!Pathfinder_findRoute(&map, &smallWorkspace, 0, 0, 2, 1,
                                 CELL_NO_OCCUPANT, 4, &steps, &cost));
}

static void testSameCellStillRequiresTraversableTerrain(void)
{
    static const char *const rows[] = { "W" };
    CellData cells[1];
    u8 costs[1];
    u8 parents[1];
    PathfinderWorkspace workspace = { costs, parents, 1, NULL };
    MapData map =
    {
        .width = 1, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 1,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(MapData_init(&map));
    assert(!Pathfinder_findRoute(&map, &workspace, 0, 0, 0, 0,
                                 CELL_NO_OCCUPANT, 1, &steps, &totalCost));
}


static void testTerrainCostsDependOnUnitType(void)
{
    static const char *const forestRows[] = { "PF" };
    static const char *const rockRows[] = { "PR" };
    CellData cells[2];
    u8 costs[2];
    u8 parents[2];
    u8 reachable[2];
    PathfinderWorkspace workspace = { costs, parents, 2, reachable };
    MapData map =
    {
        .width = 2, .height = 1,
        .terrainRows = forestRows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 2,
        .pathWorkspace = &workspace
    };
    u16 steps;
    u8 totalCost;

    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_INFANTRY, TERRAIN_FOREST) == 2);
    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_SCOUT, TERRAIN_FOREST) == 1);
    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_ENGINEER, TERRAIN_FOREST) == 3);
    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_TANK, TERRAIN_FOREST) == 3);
    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_SCOUT, TERRAIN_WATER) == 0);
    assert(UnitType_getTerrainMoveCost((UnitType)0xFF, TERRAIN_PATH) == 0);
    assert(UnitType_getTerrainMoveCost(UNIT_TYPE_INFANTRY,
                                       (TerrainType)TERRAIN_COUNT) == 0);

    assert(MapData_init(&map));
    assert(Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_SCOUT, 1, &steps, &totalCost));
    assert(steps == 1 && totalCost == 1);
    assert(!Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_INFANTRY, 1, &steps, &totalCost));
    assert(Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_INFANTRY, 2, &steps, &totalCost));
    assert(totalCost == 2);
    assert(!Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_ENGINEER, 2, &steps, &totalCost));
    assert(Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_ENGINEER, 3, &steps, &totalCost));
    assert(totalCost == 3);

    map.terrainRows = rockRows;
    assert(MapData_init(&map));
    assert(Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_SCOUT, 1, &steps, &totalCost));
    assert(totalCost == 1);
    assert(!Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_TANK, 3, &steps, &totalCost));
    assert(Pathfinder_findRouteForType(&map, &workspace, 0, 0, 1, 0,
        CELL_NO_OCCUPANT, UNIT_TYPE_TANK, 4, &steps, &totalCost));
    assert(totalCost == 4);
}

int main(void)
{
    testTerrainCostsDependOnUnitType();
    testWeightedRoutePrefersLowerCost();
    testImpassableAndOccupiedCells();
    testTerrainEntryCostsAndBudgetBoundaries();
    testRouteDoesNotWrapAtRowEdges();
    testWorkspaceAndCoordinatesAreValidated();
    testSameCellStillRequiresTraversableTerrain();
    puts("PASS: rutas ponderadas, presupuesto, bordes, agua y ocupacion");
    return 0;
}
