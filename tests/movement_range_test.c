#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pathfinder.h"


static void testReachabilityMaskHonorsTerrainCostsAndOccupancy(void)
{
    static const char *const rows[] =
    {
        "PPWPP",
        "PFFFP",
        "PPPPP"
    };
    CellData cells[15];
    u8 costs[15];
    u8 parents[15];
    u8 reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map =
    {
        .width = 5, .height = 3,
        .terrainRows = rows, .terrainRowCount = 3,
        .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace
    };

    assert(MapData_init(&map));
    assert(Pathfinder_buildReachability(&map, &workspace, 0, 1,
                                        CELL_NO_OCCUPANT, 3));
    assert(reachable[0]);   /* Dos caminos de coste acumulado 2. */
    assert(reachable[1]);
    assert(!reachable[2]); /* Agua. */
    assert(!reachable[3]); /* El agua impide rodear por arriba. */
    assert(!reachable[5]); /* El origen permite el cursor, no se ilumina. */
    assert(reachable[6]);   /* Entrar en bosque cuesta 2. */
    assert(!reachable[7]);  /* Otro bosque suma 4: supera el presupuesto. */
    assert(reachable[10]);
    assert(reachable[11]);
    assert(reachable[12]);  /* Tres caminos cuestan exactamente 3. */
    assert(!reachable[13]); /* Un paso más costaría 4. */

    /* La máscara codifica pasos+1: el HUD obtiene pasos y coste sin recorrer
       de nuevo toda la ruta cada vez que cambia el cursor. */
    {
        u16 steps = 0;
        u8 cost = 0;
        assert(Pathfinder_getReachableInfo(&map, &workspace, 0, 1,
                                           2, 2, 3, &steps, &cost));
        assert(steps == 3);
        assert(cost == 3);
        assert(reachable[12] == steps + 1);
        assert(!Pathfinder_getReachableInfo(&map, &workspace, 0, 1,
                                            2, 1, 3, &steps, &cost));
    }

    assert(MapData_setOccupant(&map, 1, 2, 7));
    assert(Pathfinder_buildReachability(&map, &workspace, 0, 1,
                                        CELL_NO_OCCUPANT, 3));
    assert(!reachable[11]);
    assert(!reachable[12]); /* No puede pasar a traves de la unidad ocupante. */
    assert(!reachable[13]);
}



static void testLargeMapKeepsReachabilityInsideBudgetBounds(void)
{
    enum { WIDTH = 80, HEIGHT = 64, CELL_COUNT = WIDTH * HEIGHT };
    static char terrain[HEIGHT][WIDTH + 1];
    static const char *rows[HEIGHT];
    static CellData cells[CELL_COUNT];
    static u8 costs[CELL_COUNT];
    static u8 parents[CELL_COUNT];
    static u8 reachable[CELL_COUNT];
    PathfinderWorkspace workspace =
        { costs, parents, CELL_COUNT, reachable };
    MapData map = { 0 };
    u16 x;
    u16 y;
    const u16 startX = 40;
    const u16 startY = 32;

    for (y = 0; y < HEIGHT; y++)
    {
        for (x = 0; x < WIDTH; x++) terrain[y][x] = 'P';
        terrain[y][WIDTH] = '\0';
        rows[y] = terrain[y];
    }

    map.width = WIDTH;
    map.height = HEIGHT;
    map.terrainRows = rows;
    map.terrainRowCount = HEIGHT;
    map.cells = cells;
    map.cellCapacity = CELL_COUNT;
    map.pathWorkspace = &workspace;

    assert(MapData_init(&map));
    assert(MapData_setOccupant(&map, startX, startY, 1));
    assert(Pathfinder_buildReachability(&map, &workspace, startX, startY,
                                        1, 8));

    assert(reachable[(u32)startY * WIDTH + startX + 8] == 9);
    assert(reachable[(u32)startY * WIDTH + startX + 9] == 0);
    assert(reachable[(u32)(startY - 8) * WIDTH + startX] == 9);
    assert(reachable[(u32)(startY - 9) * WIDTH + startX] == 0);
}

static void testMaximumEncodedRouteLengthDoesNotOverflow(void)
{
    enum { WIDTH = 255, CELL_COUNT = WIDTH };
    static char row[WIDTH + 1];
    const char *rows[] = { row };
    CellData cells[CELL_COUNT];
    u8 costs[CELL_COUNT];
    u8 parents[CELL_COUNT];
    u8 reachable[CELL_COUNT];
    PathfinderWorkspace workspace =
        { costs, parents, CELL_COUNT, reachable };
    MapData map =
    {
        .width = WIDTH, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = CELL_COUNT,
        .pathWorkspace = &workspace
    };
    u16 steps = 0;
    u8 cost = 0;

    memset(row, 'P', WIDTH);
    row[WIDTH] = '\0';
    assert(MapData_init(&map));
    assert(MapData_setOccupant(&map, 0, 0, 1));
    assert(Pathfinder_buildReachability(&map, &workspace, 0, 0, 1, 254));
    assert(reachable[WIDTH - 1] == 255); /* 254 pasos + 1, sin desbordar. */
    assert(Pathfinder_getReachableInfo(&map, &workspace, 0, 0,
                                       WIDTH - 1, 0, 254, &steps, &cost));
    assert(steps == 254);
    assert(cost == 254);
}

int main(void)
{
    testReachabilityMaskHonorsTerrainCostsAndOccupancy();
    testLargeMapKeepsReachabilityInsideBudgetBounds();
    testMaximumEncodedRouteLengthDoesNotOverflow();
    puts("PASS: alcance ponderado, mapa grande, HUD O(1) y rutas largas");
    return 0;
}
