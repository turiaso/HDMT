/*
 * Modelo lógico del mapa: dimensiones, terreno, ocupación y visibilidad.
 * No depende de sprites; toda consulta de celda debe validar coordenadas y límites.
 */
#include <genesis.h>
#include <string.h>
#include "map_data.h"
#include "cell_events.h"
#include "pathfinder.h"

/*
 * Fuente autoritativa del mapa lógico: terreno, ocupación, visión y línea de visión.
 * Deliberadamente no depende del MAP gráfico de SGDK; todos los consumidores comparten
 * las mismas reglas de terreno, transitabilidad y oclusión.
 */

/*
 * Una letra por casilla lógica de 24x24 px. Esta capa no dibuja el mapa:
 * describe el terreno para las reglas del juego.
 *
 * W = agua, F = bosque, P = camino, R = piedras.
 * La distribución inicial representa el terreno predominante del mapa gráfico.
 */
/*
 * Escenario de ejemplo. La anchura se deduce de su primera fila y la altura
 * del número de filas; no son límites globales del motor.
 */
static const char worldTerrainFirstRow[] = "WWWWWWWWWWWWWWWWWWWPPWWWWWWWWWWWWWWWWWWW";
static const char *const worldTerrainRows[] =
{
    worldTerrainFirstRow,
    "WWWWFFFFFFFFFFFFWWWPPWWWFFFFFFFFFFFFWWWW",
    "WWWWFFFFFFFFFFFWWWWPPWWWFFFFFFFFFFFWWWWW",
    "WWWWRRFFFFPPFFFFPWWPPWWWRRFFFFPPFFFFPWWW",
    "WWWRRFRFFPFFFFFFPWWPPWWRRFRFFPFFFFFFPWWW",
    "WWWRRRRFFPFFFFFPPWWPPWWRRRRFFPFFFFFPPWWW",
    "WWWFFFFFPFFFPPPPWWWPPWWFFFFFPFFFPPPPWWWW",
    "WWWWFFFFPPFPPFFFFWWPPWWWFFFFPPFPPFFFFWWW",
    "WWFFFFFFPPPPRRRFFFWPPWFFFFFFPPPPRRRFFFWW",
    "WWFFFFFPFFFRRFRFFFWPPWFFFFFPFFFRRFRFFFWW",
    "WWFFFFFPFFFRRFFFFFWPPWFFFFFPFFFRRFFFFFWW",
    "WWFFFFFPFFFFFFFFFFWPPWFFFFFPFFFFFFFFFFWW",
    "WWWFFFPPFFRWWWRFFWWPPWWFFFPPFFRWWWRFFWWW",
    "WWWFFPFFPFFWWWRFWWWPPWWFFPFFPFFWWWRFWWWW",
    "WWWWPRFFFPRFWWRFRWWPPWWWPRFFFPRFWWRFRWWW",
    "WWWPPFFFFFPFFRRRRWWPPWWPPFFFFFPFFRRRRWWW",
    "WWWRRRFFFFPPFFFFWWWPPWWRRRFFFFPPFFFFWWWW",
    "WWWWWFFFFFFFPPFFWWWPPWWWWFFFFFFFPPFFWWWW",
    "WWWWFFFFFFFFPFFFWWWPPWWWFFFFFFFFPFFFWWWW",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "WWWWFFFFFFFFFFFFWWWPPWWWFFFFFFFFFFFFWWWW",
    "WWWWFFFFFFFFFFFWWWWPPWWWFFFFFFFFFFFWWWWW",
    "WWWWRRFFFFPPFFFFPWWPPWWWRRFFFFPPFFFFPWWW",
    "WWWRRFRFFPFFFFFFPWWPPWWRRFRFFPFFFFFFPWWW",
    "WWWRRRRFFPFFFFFPPWWPPWWRRRRFFPFFFFFPPWWW",
    "WWWFFFFFPFFFPPPPWWWPPWWFFFFFPFFFPPPPWWWW",
    "WWWWFFFFPPFPPFFFFWWPPWWWFFFFPPFPPFFFFWWW",
    "WWFFFFFFPPPPRRRFFFWPPWFFFFFFPPPPRRRFFFWW",
    "WWFFFFFPFFFRRFRFFFWPPWFFFFFPFFFRRFRFFFWW",
    "WWFFFFFPFFFRRFFFFFWPPWFFFFFPFFFRRFFFFFWW",
    "WWFFFFFPFFFFFFFFFFWPPWFFFFFPFFFFFFFFFFWW",
    "WWWFFFPPFFRWWWRFFWWPPWWFFFPPFFRWWWRFFWWW",
    "WWWFFPFFPFFWWWRFWWWPPWWFFPFFPFFWWWRFWWWW",
    "WWWWPRFFFPRFWWRFRWWPPWWWPRFFFPRFWWRFRWWW",
    "WWWPPFFFFFPFFRRRRWWPPWWPPFFFFFPFFRRRRWWW",
    "WWWRRRFFFFPPFFFFWWWPPWWRRRFFFFPPFFFFWWWW",
    "WWWWWFFFFFFFPPFFWWWPPWWWWFFFFFFFPPFFWWWW",
    "WWWWFFFFFFFFPFFFWWWPPWWWFFFFFFFFPFFFWWWW",
    "WWWWWWWWWWWWWWWWWWWPPWWWWWWWWWWWWWWWWWWW",
};

static const char bridgeTerrainFirstRow[] = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
static const char *const bridgeTerrainRows[] =
{
    bridgeTerrainFirstRow,
    "WFFFFFFFFFFFFFFFFFWWWWFFFFFFFFFFFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFRRRPFFFFFFFFFWWWWFFFFFFFFFPFFFFWWFW",
    "WFFFFRRRPFFFFFFFFFWWWWFFFFFFFFFPFFFFWWFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFWWFW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFRRRFFFWWWWFFFRRRFFFPFFFFFFFW",
    "WFFFFFFFPFFFRRRFFFWWWWFFFRRRFFFPFFFFFFFW",
    "WFFFFFFFPFFFRRRFFFWWWWFFFRRRFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFRRRRFFPFFFFFFFW",
    "WFFFFFFFPFFRRRRFFFWWWWFFFRRRRFFPFFFFFFFW",
    "WFWWFFFFPFFRRRRFFFWWWWFFFRRRRFFPFFFFFFFW",
    "WFWWFFFFPFFRRRRFFFWWWWFFFRRRRFFPFFFFFFFW",
    "WFWWFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPRRRFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPRRRFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPRRRFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFPFFFFFFFFFWWWWFFFFFFFFFPFFFFFFFW",
    "WFFFFFFFFFFFFFFFFFWWWWFFFFFFFFFFFFFFFFFW",
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
};

/* Tercer escenario: escaramuza 20x20, un puente central y un río
 * que obliga a converger por el paso. Es un recorte lógico coherente del
 * mapa de Defensa del puente, no una escala del mapa de 40x40.
 */
static const char bridgeSkirmishTerrainFirstRow[] = "FFFFFFFFWWWWFFFFFFFF";
static const char *const bridgeSkirmishTerrainRows[] =
{
    bridgeSkirmishTerrainFirstRow,
    "FFFFFFFFWWWWFFFFFFFF",
    "FFRRRFFFWWWWFFFRRRFF",
    "FFRRRFFFWWWWFFFRRRFF",
    "FFRRRFFFWWWWFFFRRRFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "PPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPP",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFRRRRF",
    "FRRRRFFFWWWWFFFRRRRF",
    "FRRRRFFFWWWWFFFRRRRF",
    "FRRRRFFFWWWWFFFRRRRF",
    "FFFFFFFFWWWWFFFFFFFF",
    "FFFFFFFFWWWWFFFFFFFF"
};

/* Cuarto escenario: el mismo campo de 20x20 girado 90 grados.
 * El río cruza de este a oeste y el paso central discurre de norte a sur.
 * Conserva el objetivo central y permite comparar partidas con el mismo
 * despliegue, composición y condición de victoria que el escenario 3. */
static const char ruinsCrossingTerrainFirstRow[] = "FFFFFFFFFPPFFFFFFFFF";
static const char *const ruinsCrossingTerrainRows[] =
{
    ruinsCrossingTerrainFirstRow,
    "FFRRRFFFFPPFFFFFFFFF",
    "FFRRRFFFFPPFFFFRRRFF",
    "FFRRRFFFFPPFFFFRRRFF",
    "FFRRRFFFFPPFFFFRRRFF",
    "FFFFFFFFFPPFFFFFFFFF",
    "FFFFFFFFFPPFFFFFFFFF",
    "FFFFFFFFFPPFFFFFFFFF",
    "WWWWWWWWWPPWWWWWWWWW",
    "WWWWWWWWWPPWWWWWWWWW",
    "WWWWWWWWWPPWWWWWWWWW",
    "WWWWWWWWWPPWWWWWWWWW",
    "FFFFFFFFFPPFFFFFFFFF",
    "FFFFFFFFFPPFFFFFFFFF",
    "FFFFFFFFFPPFFFFFFFFF",
    "FFRRRRFFFPPFFFFRRRFF",
    "FFRRRRFFFPPFFFFRRRFF",
    "FFRRRRFFFPPFFFFRRRFF",
    "FFRRRRFFFPPFFFFFFFFF",
    "FFFFFFFFFPPFFFFFFFFF"
};

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

static CellData worldCells[
    ARRAY_COUNT(worldTerrainRows) * (sizeof(worldTerrainFirstRow) - 1)
];
static u8 worldPathCosts[ARRAY_COUNT(worldCells)];
static u8 worldPathParents[ARRAY_COUNT(worldCells)];
static u8 worldReachable[ARRAY_COUNT(worldCells)];
static PathfinderWorkspace worldPathWorkspace =
{
    .costs = worldPathCosts,
    .parentDirections = worldPathParents,
    .capacity = (u32)ARRAY_COUNT(worldCells),
    .reachable = worldReachable
};

MapData worldMapData =
{
    .width = (u16)(sizeof(worldTerrainFirstRow) - 1),
    .height = (u16)ARRAY_COUNT(worldTerrainRows),
    .terrainRows = worldTerrainRows,
    .terrainRowCount = (u16)ARRAY_COUNT(worldTerrainRows),
    .cells = worldCells,
    .cellCapacity = (u32)ARRAY_COUNT(worldCells),
    .pathWorkspace = &worldPathWorkspace
};

/*
 * Segundo escenario: defensa de un puente sobre un rio norte-sur.
 * Comparte buffers de RAM con worldMapData porque solo se carga un escenario
 * por partida. No inicializar ambos mapas a la vez.
 */
MapData bridgeMapData =
{
    .width = (u16)(sizeof(bridgeTerrainFirstRow) - 1),
    .height = (u16)ARRAY_COUNT(bridgeTerrainRows),
    .terrainRows = bridgeTerrainRows,
    .terrainRowCount = (u16)ARRAY_COUNT(bridgeTerrainRows),
    .cells = worldCells,
    .cellCapacity = (u32)ARRAY_COUNT(worldCells),
    .pathWorkspace = &worldPathWorkspace
};

/* Escaramuza reducida 20x20. Reutiliza el mismo almacenamiento de RAM que
   los demás mapas; el índice lineal se calcula con su anchura de 20 celdas. */
MapData bridgeSkirmishMapData =
{
    .width = (u16)(sizeof(bridgeSkirmishTerrainFirstRow) - 1),
    .height = (u16)ARRAY_COUNT(bridgeSkirmishTerrainRows),
    .terrainRows = bridgeSkirmishTerrainRows,
    .terrainRowCount = (u16)ARRAY_COUNT(bridgeSkirmishTerrainRows),
    .cells = worldCells,
    .cellCapacity = (u32)ARRAY_COUNT(worldCells),
    .pathWorkspace = &worldPathWorkspace
};

/* Cuarto escenario: comparte RAM, pero tiene su propia definición de terreno. */
MapData ruinsCrossingMapData =
{
    .width = (u16)(sizeof(ruinsCrossingTerrainFirstRow) - 1),
    .height = (u16)ARRAY_COUNT(ruinsCrossingTerrainRows),
    .terrainRows = ruinsCrossingTerrainRows,
    .terrainRowCount = (u16)ARRAY_COUNT(ruinsCrossingTerrainRows),
    .cells = worldCells,
    .cellCapacity = (u32)ARRAY_COUNT(worldCells),
    .pathWorkspace = &worldPathWorkspace
};

/* Reglas provisionales para el primer escenario; pueden ajustarse aquí. */
const TerrainInfo terrainInfo[TERRAIN_COUNT] =
{
    /* El agua abierta no bloquea la vista; bosque y rocas sí. */
    { "Agua",    0, FALSE, 0, FALSE },
    { "Bosque",  2, TRUE,  1, TRUE  },
    { "Camino",  1, TRUE,  0, FALSE },
    { "Piedras", 2, TRUE,  2, TRUE  }
};

static bool decodeTerrain(char symbol, TerrainType *terrain)
{
    switch (symbol)
    {
        case 'W': *terrain = TERRAIN_WATER;  return TRUE;
        case 'F': *terrain = TERRAIN_FOREST; return TRUE;
        case 'P': *terrain = TERRAIN_PATH;   return TRUE;
        case 'R': *terrain = TERRAIN_ROCK;   return TRUE;
        default: return FALSE;
    }
}

bool MapData_init(MapData *map)
{
    u16 x;
    u16 y;
    const u32 requiredCells = (map != NULL)
        ? (u32)map->width * map->height : 0;

    /* MapData valida su propia memoria, sin imponer límites de presentación. */
    if ((map == NULL) || (map->width == 0) || (map->height == 0) ||
        (map->terrainRows == NULL) || (map->terrainRowCount < map->height) ||
        (map->cells == NULL) || (map->cellCapacity < requiredCells))
        return FALSE;

    /* Valida toda la fuente antes de modificar el estado del escenario. */
    for (y = 0; y < map->height; y++)
    {
        const char *row = map->terrainRows[y];
        if ((row == NULL) || (strlen(row) != map->width)) return FALSE;

        for (x = 0; x < map->width; x++)
        {
            TerrainType terrain;
            if (!decodeTerrain(row[x], &terrain)) return FALSE;
        }
    }

    for (y = 0; y < map->height; y++)
    {
        const char *row = map->terrainRows[y];
        for (x = 0; x < map->width; x++)
        {
            const u32 index = (u32)y * map->width + x;
            TerrainType terrain;
            /* Defensivo además de la pasada de validación inicial: no deja
               depender la inicialización de que la fuente siga siendo válida. */
            if (!decodeTerrain(row[x], &terrain)) return FALSE;
            map->cells[index].terrain = terrain;
            map->cells[index].occupantId = CELL_NO_OCCUPANT;
            map->cells[index].eventSetId = CELL_EVENT_SET_DEFAULT;
            map->cells[index].visionState = 0;
        }
    }

    return TRUE;
}

static bool hasCell(const MapData *map, u16 x, u16 y)
{
    u32 index;

    if ((map == NULL) || (map->cells == NULL) ||
        (x >= map->width) || (y >= map->height))
        return FALSE;

    index = (u32)y * map->width + x;
    return index < map->cellCapacity;
}

CellData *MapData_getCell(MapData *map, u16 x, u16 y)
{
    if (!hasCell(map, x, y)) return NULL;
    return &map->cells[(u32)y * map->width + x];
}

const CellData *MapData_getCellConst(const MapData *map, u16 x, u16 y)
{
    if (!hasCell(map, x, y)) return NULL;
    return &map->cells[(u32)y * map->width + x];
}

u8 MapData_getVisionState(const MapData *map, u8 team, u16 x, u16 y)
{
    const CellData *cell;
    const u8 shift = (u8)(team * 2u);

    if (team >= 2u) return MAP_VISION_UNSEEN;
    cell = MapData_getCellConst(map, x, y);
    if (cell == NULL) return MAP_VISION_UNSEEN;
    return (u8)((cell->visionState >> shift) & 0x03u);
}

u8 MapData_getCombinedVisionState(const MapData *map, u16 x, u16 y)
{
    const u8 blue = MapData_getVisionState(map, 0u, x, y);
    const u8 red = MapData_getVisionState(map, 1u, x, y);

    if ((blue == MAP_VISION_VISIBLE) || (red == MAP_VISION_VISIBLE))
        return MAP_VISION_VISIBLE;
    if ((blue != MAP_VISION_UNSEEN) || (red != MAP_VISION_UNSEEN))
        return MAP_VISION_EXPLORED;
    return MAP_VISION_UNSEEN;
}

const TerrainInfo *MapData_getTerrainInfo(TerrainType terrain)
{
    if (terrain >= TERRAIN_COUNT) return NULL;
    return &terrainInfo[terrain];
}

static bool blocksLineOfSightAt(const MapData *map, s32 x, s32 y)
{
    const CellData *cell;
    const TerrainInfo *terrain;

    if ((map == NULL) || (x < 0) || (y < 0) ||
        (x >= (s32)map->width) || (y >= (s32)map->height))
        return TRUE;
    cell = MapData_getCellConst(map, (u16)x, (u16)y);
    if (cell == NULL) return TRUE;
    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    return (terrain == NULL) || terrain->blocksLineOfSight;
}

bool MapData_hasLineOfSight(const MapData *map, u16 fromX, u16 fromY,
                            u16 toX, u16 toY)
{
    /*
     * Recorrido entero y simétrico de la rejilla. Si el rayo pasa exactamente
     * por una esquina, avanza en diagonal; las dos celdas que solo toca por
     * el borde no bloquean. La celda diagonal sí se comprueba como intermedia.
     */
    s32 x = (s32)fromX;
    s32 y = (s32)fromY;
    const s32 targetX = (s32)toX;
    const s32 targetY = (s32)toY;
    const s32 dx = (targetX >= x) ? targetX - x : x - targetX;
    const s32 dy = (targetY >= y) ? targetY - y : y - targetY;
    const s32 stepX = (x < targetX) ? 1 : -1;
    const s32 stepY = (y < targetY) ? 1 : -1;
    s32 crossedX = 0;
    s32 crossedY = 0;

    if ((map == NULL) || (MapData_getCellConst(map, fromX, fromY) == NULL) ||
        (MapData_getCellConst(map, toX, toY) == NULL))
        return FALSE;
    if ((fromX == toX) && (fromY == toY)) return TRUE;

    while ((crossedX < dx) || (crossedY < dy))
    {
        const s32 nextVertical = (1L + 2L * crossedX) * dy;
        const s32 nextHorizontal = (1L + 2L * crossedY) * dx;

        if (nextVertical == nextHorizontal)
        {
            /*
             * Cruce exacto de esquina: avanzar en diagonal sin bloquear la
             * visión por las dos celdas laterales. Esas celdas solo rozan el
             * rayo; tratarlas como obstáculos cerraría corredores diagonales
             * y produciría huecos de niebla alrededor de bosques/piedras.
             */
            x += stepX;
            y += stepY;
            crossedX++;
            crossedY++;
        }
        else if (nextVertical < nextHorizontal)
        {
            x += stepX;
            crossedX++;
        }
        else
        {
            y += stepY;
            crossedY++;
        }

        /* Una casilla de destino opaca puede verse/atacarse; no tapa su propia unidad. */
        if ((x == targetX) && (y == targetY)) return TRUE;
        if (blocksLineOfSightAt(map, x, y)) return FALSE;
    }

    return TRUE;
}

bool MapData_isPassable(const MapData *map, u16 x, u16 y)
{
    const CellData *cell = MapData_getCellConst(map, x, y);
    const TerrainInfo *info;

    if (cell == NULL) return FALSE;
    info = MapData_getTerrainInfo(cell->terrain);
    return (info != NULL) && info->passable;
}

bool MapData_isOccupied(const MapData *map, u16 x, u16 y)
{
    const CellData *cell = MapData_getCellConst(map, x, y);

    /* Fuera del mapa se trata como ocupado para impedir entrar por error. */
    return (cell == NULL) || (cell->occupantId != CELL_NO_OCCUPANT);
}

bool MapData_canEnter(const MapData *map, u16 x, u16 y)
{
    const CellData *cell = MapData_getCellConst(map, x, y);
    const TerrainInfo *terrain;

    if ((cell == NULL) || (cell->occupantId != CELL_NO_OCCUPANT)) return FALSE;
    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    return (terrain != NULL) && terrain->passable;
}

bool MapData_setOccupant(MapData *map, u16 x, u16 y, s8 occupantId)
{
    CellData *cell;

    if (occupantId < CELL_NO_OCCUPANT) return FALSE;
    cell = MapData_getCell(map, x, y);
    if (cell == NULL) return FALSE;

    cell->occupantId = occupantId;
    return TRUE;
}

/* Asigna un perfil compartido sin almacenar callbacks en la celda. */
bool MapData_setEventSet(MapData *map, u16 x, u16 y, u8 eventSetId)
{
    CellData *cell = MapData_getCell(map, x, y);
    if (cell == NULL) return FALSE;
    cell->eventSetId = eventSetId;
    return TRUE;
}
