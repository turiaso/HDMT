#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "map_data.h"
#include "pathfinder.h"

/*
 * Prueba de propiedades determinista del buscador ponderado.
 * Compara sus costes con Dijkstra independiente y comprueba que la cadena de
 * padres usada para previsualizar una ruta tenga el mismo número de pasos que
 * informa la caché de alcance. No depende de recursos ni de SGDK real.
 */
enum { TEST_WIDTH = 8, TEST_HEIGHT = 8, TEST_CELL_COUNT = TEST_WIDTH * TEST_HEIGHT,
       TEST_MAX_COST = 10, RANDOM_MAP_COUNT = 20000 };

static char terrainRowsStorage[TEST_HEIGHT][TEST_WIDTH + 1];
static const char *terrainRows[TEST_HEIGHT];
static CellData cells[TEST_CELL_COUNT];
static u8 costs[TEST_CELL_COUNT];
static u8 parents[TEST_CELL_COUNT];
static u8 reachable[TEST_CELL_COUNT];
static PathfinderWorkspace workspace =
    { costs, parents, TEST_CELL_COUNT, reachable };
static MapData map =
{
    .width = TEST_WIDTH,
    .height = TEST_HEIGHT,
    .terrainRows = terrainRows,
    .terrainRowCount = TEST_HEIGHT,
    .cells = cells,
    .cellCapacity = TEST_CELL_COUNT,
    .pathWorkspace = &workspace
};

static u16 enterCost(u16 index)
{
    const CellData *cell = &cells[index];
    const TerrainInfo *terrain = MapData_getTerrainInfo(cell->terrain);
    if ((terrain == NULL) || !terrain->passable) return 0xFFFFu;
    return terrain->moveCost;
}

static void referenceDijkstra(u16 startIndex, u16 maxCost,
                              u16 distances[TEST_CELL_COUNT])
{
    bool visited[TEST_CELL_COUNT] = { FALSE };
    u16 iteration;

    for (iteration = 0; iteration < TEST_CELL_COUNT; iteration++)
        distances[iteration] = 0xFFFFu;
    distances[startIndex] = 0;

    for (iteration = 0; iteration < TEST_CELL_COUNT; iteration++)
    {
        s32 bestIndex = -1;
        u16 i;
        u16 neighbours[4];
        u16 n;

        for (i = 0; i < TEST_CELL_COUNT; i++)
        {
            if (visited[i]) continue;
            if ((bestIndex < 0) || (distances[i] < distances[bestIndex]))
                bestIndex = (s32)i;
        }
        if ((bestIndex < 0) || (distances[bestIndex] == 0xFFFFu)) break;
        visited[bestIndex] = TRUE;

        {
            const u16 x = (u16)(bestIndex % TEST_WIDTH);
            const u16 y = (u16)(bestIndex / TEST_WIDTH);
            neighbours[0] = (y > 0) ? (u16)(bestIndex - TEST_WIDTH) : 0xFFFFu;
            neighbours[1] = (x + 1 < TEST_WIDTH) ? (u16)(bestIndex + 1) : 0xFFFFu;
            neighbours[2] = (y + 1 < TEST_HEIGHT) ? (u16)(bestIndex + TEST_WIDTH) : 0xFFFFu;
            neighbours[3] = (x > 0) ? (u16)(bestIndex - 1) : 0xFFFFu;
        }

        for (n = 0; n < 4; n++)
        {
            const u16 next = neighbours[n];
            const u16 movementCost = (next == 0xFFFFu) ? 0xFFFFu : enterCost(next);
            const u32 candidateCost = (u32)distances[bestIndex] + movementCost;

            if ((next == 0xFFFFu) || (movementCost == 0xFFFFu) ||
                (candidateCost > maxCost) || (candidateCost >= distances[next]))
                continue;
            distances[next] = (u16)candidateCost;
        }
    }
}

static void buildRandomMap(void)
{
    u16 x;
    u16 y;

    for (y = 0; y < TEST_HEIGHT; y++)
    {
        for (x = 0; x < TEST_WIDTH; x++)
        {
            const u8 value = (u8)(rand() % 10);
            terrainRowsStorage[y][x] = (value < 2) ? 'W' :
                (value < 6) ? 'P' : (value < 8) ? 'F' : 'R';
        }
        terrainRowsStorage[y][TEST_WIDTH] = '\0';
        terrainRows[y] = terrainRowsStorage[y];
    }
}

int main(void)
{
    u16 trial;
    srand(12345u);

    for (trial = 0; trial < RANDOM_MAP_COUNT; trial++)
    {
        const u16 startX = (u16)(rand() % TEST_WIDTH);
        const u16 startY = (u16)(rand() % TEST_HEIGHT);
        const u16 startIndex = (u16)(startY * TEST_WIDTH + startX);
        u16 referenceDistances[TEST_CELL_COUNT];
        u16 i;

        buildRandomMap();
        terrainRowsStorage[startY][startX] = 'P';
        assert(MapData_init(&map));
        assert(Pathfinder_buildReachability(&map, &workspace, startX, startY,
                                            CELL_NO_OCCUPANT, TEST_MAX_COST));
        referenceDijkstra(startIndex, TEST_MAX_COST, referenceDistances);

        for (i = 0; i < TEST_CELL_COUNT; i++)
        {
            if (referenceDistances[i] <= TEST_MAX_COST)
            {
                u16 current = i;
                u16 steps = 0;
                u16 pathCost = 0;

                assert(costs[i] == (u8)referenceDistances[i]);
                if (i == startIndex) continue;
                assert(reachable[i] != 0);

                while (current != startIndex && steps < TEST_CELL_COUNT)
                {
                    const PathDirection parent =
                        (PathDirection)(parents[current] & 0x07u);
                    const u16 movementCost = enterCost(current);
                    const u16 x = (u16)(current % TEST_WIDTH);
                    const u16 y = (u16)(current / TEST_WIDTH);

                    assert(parent >= PATH_DIR_UP && parent <= PATH_DIR_LEFT);
                    pathCost = (u16)(pathCost + movementCost);
                    if (parent == PATH_DIR_UP)
                    {
                        assert(y > 0);
                        current = (u16)(current - TEST_WIDTH);
                    }
                    else if (parent == PATH_DIR_RIGHT)
                    {
                        assert(x + 1 < TEST_WIDTH);
                        current++;
                    }
                    else if (parent == PATH_DIR_DOWN)
                    {
                        assert(y + 1 < TEST_HEIGHT);
                        current = (u16)(current + TEST_WIDTH);
                    }
                    else
                    {
                        assert(x > 0);
                        current--;
                    }
                    steps++;
                }

                assert(current == startIndex);
                assert(pathCost == referenceDistances[i]);
                assert((u16)(steps + 1u) == reachable[i]);
            }
            else
            {
                assert(reachable[i] == 0);
                assert(costs[i] == 0xFFu);
            }
        }
    }

    printf("PASS: %u mapas aleatorios; costes de Dijkstra y rutas previas coherentes\n",
           RANDOM_MAP_COUNT);
    return 0;
}
