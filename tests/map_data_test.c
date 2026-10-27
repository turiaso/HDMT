#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "map_data.h"
#include "cell_events.h"



static void testCornerGrazingDoesNotBlockLineOfSight(void)
{
    static const char *const rows[] = { "PRP", "RPP", "PPP" };
    CellData cells[9];
    MapData map = { .width = 3, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 9 };

    assert(MapData_init(&map));
    /* El rayo (0,0)->(2,2) roza las dos rocas en la esquina, pero atraviesa
       el centro (1,1) por la diagonal. Las celdas laterales no bloquean. */
    assert(MapData_hasLineOfSight(&map, 0, 0, 2, 2));
    assert(MapData_hasLineOfSight(&map, 2, 2, 0, 0));

    /* Una roca en la celda diagonal intermedia sí bloquea en ambos sentidos. */
    MapData_getCell(&map, 1, 1)->terrain = TERRAIN_ROCK;
    assert(!MapData_hasLineOfSight(&map, 0, 0, 2, 2));
    assert(!MapData_hasLineOfSight(&map, 2, 2, 0, 0));
}

static void testTerrainLineOfSight(void)
{
    static const char *const rows[] = {
        "PPPPP", "PFRPP", "PPPPP"
    };
    CellData cells[15];
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15 };

    assert(MapData_init(&map));
    /* Un camino despejado permite ver de un extremo al otro. */
    assert(MapData_hasLineOfSight(&map, 0, 0, 4, 0));
    /* El terreno del objetivo no bloquea la propia casilla. */
    assert(MapData_hasLineOfSight(&map, 0, 1, 1, 1));
    assert(MapData_hasLineOfSight(&map, 1, 1, 2, 1)); /* roca como destino */
    /* Bosque y rocas intermedios ocultan lo que hay detrás. */
    assert(!MapData_hasLineOfSight(&map, 0, 1, 4, 1));
    assert(!MapData_hasLineOfSight(&map, 0, 1, 3, 1)); /* bosque intermedio */
    assert(!MapData_hasLineOfSight(&map, 1, 1, 3, 1)); /* roca intermedia */
    /* El algoritmo es simétrico y rechaza coordenadas fuera del mapa. */
    assert(!MapData_hasLineOfSight(&map, 4, 1, 0, 1));
    assert(!MapData_hasLineOfSight(&map, 0, 0, 5, 0));
    assert(MapData_getTerrainInfo(TERRAIN_FOREST)->blocksLineOfSight);
    assert(MapData_getTerrainInfo(TERRAIN_ROCK)->blocksLineOfSight);
    assert(!MapData_getTerrainInfo(TERRAIN_PATH)->blocksLineOfSight);
    {
        u16 x0, y0, x1, y1;
        for (y0 = 0; y0 < map.height; y0++)
            for (x0 = 0; x0 < map.width; x0++)
                for (y1 = 0; y1 < map.height; y1++)
                    for (x1 = 0; x1 < map.width; x1++)
                        assert(MapData_hasLineOfSight(&map, x0, y0, x1, y1) ==
                               MapData_hasLineOfSight(&map, x1, y1, x0, y0));
    }
}

static void testCombinedVisionState(void)
{
    CellData cell = { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, 0 };
    MapData map = { .width = 1, .height = 1, .cells = &cell,
                    .cellCapacity = 1 };

    /* Solo Azul ve la casilla: la perspectiva de espectador la muestra. */
    cell.visionState = (u8)(MAP_VISION_VISIBLE | (MAP_VISION_UNSEEN << 2));
    assert(MapData_getCombinedVisionState(&map, 0, 0) == MAP_VISION_VISIBLE);

    /* Azul recuerda el terreno, pero ninguno lo ve ahora: sigue explorado. */
    cell.visionState = (u8)(MAP_VISION_EXPLORED | (MAP_VISION_UNSEEN << 2));
    assert(MapData_getCombinedVisionState(&map, 0, 0) == MAP_VISION_EXPLORED);

    /* Si cualquiera de los dos lo ve ahora, la unión es visible. */
    cell.visionState = (u8)(MAP_VISION_EXPLORED | (MAP_VISION_VISIBLE << 2));
    assert(MapData_getCombinedVisionState(&map, 0, 0) == MAP_VISION_VISIBLE);

    /* Solo permanece oculto si ambos equipos lo desconocen. */
    cell.visionState = (u8)(MAP_VISION_UNSEEN | (MAP_VISION_UNSEEN << 2));
    assert(MapData_getCombinedVisionState(&map, 0, 0) == MAP_VISION_UNSEEN);

    /* Fuera del mapa se considera desconocido, sin acceder a memoria. */
    assert(MapData_getCombinedVisionState(&map, 1, 0) == MAP_VISION_UNSEEN);
}

static void testVariableMapDimensions(void)
{
    static const char *const rows[] = { "WFP", "RRW" };
    CellData cells[6];
    CellData tooSmall[5];
    MapData map =
    {
        .width = 3,
        .height = 2,
        .terrainRows = rows,
        .terrainRowCount = 2,
        .cells = cells,
        .cellCapacity = 6
    };
    MapData invalidCapacity = map;
    MapData invalidRows = map;
    invalidCapacity.cells = tooSmall;
    invalidCapacity.cellCapacity = 5;
    invalidRows.terrainRowCount = 1;

    assert(MapData_init(&map));
    assert(MapData_getCell(&map, 2, 1) == &cells[5]);
    assert(MapData_getCell(&map, 3, 1) == NULL);
    assert(!MapData_init(&invalidCapacity));
    assert(!MapData_init(&invalidRows));
}

int main(void)
{
    u16 x, y;
    unsigned counts[TERRAIN_COUNT] = {0};

    testVariableMapDimensions();
    testCombinedVisionState();
    testTerrainLineOfSight();
    testCornerGrazingDoesNotBlockLineOfSight();

    assert(sizeof(CellData) == 4); /* terreno, ocupante, eventos y visión por equipo */
    assert(MapData_init(&worldMapData));
    /* El escenario actual es 40x40; MapData admite otras dimensiones con capacidad válida. */
    assert(worldMapData.width == strlen(worldMapData.terrainRows[0]));
    assert(worldMapData.height == worldMapData.terrainRowCount);
    assert(worldMapData.cellCapacity >= (u32)worldMapData.width * worldMapData.height);
    assert(worldMapData.width == 40 && worldMapData.height == 40);

    for (y = 0; y < worldMapData.height; ++y)
    {
        for (x = 0; x < worldMapData.width; ++x)
        {
            const CellData *c = MapData_getCellConst(&worldMapData, x, y);
            assert(c != NULL);
            assert(c->terrain < TERRAIN_COUNT);
            assert(c->occupantId == CELL_NO_OCCUPANT);
            assert(c->eventSetId == CELL_EVENT_SET_DEFAULT);
            ++counts[c->terrain];
        }
    }
    for (x = 0; x < TERRAIN_COUNT; ++x) assert(counts[x] > 0);

    assert(MapData_getCell(&worldMapData, 40, 0) == NULL);
    assert(MapData_getCellConst(&worldMapData, 0, 40) == NULL);
    assert(MapData_getCellConst(&worldMapData, 19, 19)->terrain == TERRAIN_PATH);
    assert(MapData_getCellConst(&worldMapData, 20, 20)->terrain == TERRAIN_PATH);
    for (x = 0; x < 40; ++x)
    {
        assert(MapData_getCellConst(&worldMapData, x, 19)->terrain == TERRAIN_PATH);
        assert(MapData_getCellConst(&worldMapData, x, 20)->terrain == TERRAIN_PATH);
    }
    for (y = 0; y < 40; ++y)
    {
        assert(MapData_getCellConst(&worldMapData, 19, y)->terrain == TERRAIN_PATH);
        assert(MapData_getCellConst(&worldMapData, 20, y)->terrain == TERRAIN_PATH);
    }
    assert(!MapData_isPassable(&worldMapData, 0, 0));
    assert(MapData_isPassable(&worldMapData, 4, 1));
    assert(!MapData_isOccupied(&worldMapData, 4, 1));
    assert(MapData_canEnter(&worldMapData, 4, 1));
    assert(MapData_setOccupant(&worldMapData, 4, 1, 7));
    assert(MapData_isOccupied(&worldMapData, 4, 1));
    assert(!MapData_canEnter(&worldMapData, 4, 1));
    assert(MapData_setOccupant(&worldMapData, 4, 1, CELL_NO_OCCUPANT));
    assert(MapData_canEnter(&worldMapData, 4, 1));
    assert(!MapData_setOccupant(&worldMapData, 40, 1, 7));
    assert(MapData_getTerrainInfo(TERRAIN_PATH)->moveCost == 1);

    printf("PASS: 1600 cells; LOS simetrica; water=%u forest=%u path=%u rock=%u\n",
           counts[TERRAIN_WATER], counts[TERRAIN_FOREST],
           counts[TERRAIN_PATH], counts[TERRAIN_ROCK]);
    return 0;
}
