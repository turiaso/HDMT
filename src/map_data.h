#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <genesis.h>

/* Capacidad de los subsistemas de gameplay con buffers estáticos compartidos.
 * MapData por sí solo admite mapas mayores para herramientas y pruebas; no todos
 * los sistemas de la partida admiten dimensiones superiores a estos límites.
 */
#define GAMEPLAY_MAP_MAX_SIDE  40u
#define GAMEPLAY_MAP_MAX_CELLS (GAMEPLAY_MAP_MAX_SIDE * GAMEPLAY_MAP_MAX_SIDE)

/* Terrenos lógicos. Sus reglas se definen una sola vez en map_data.c. */
typedef u8 TerrainType;
enum
{
    TERRAIN_WATER = 0,
    TERRAIN_FOREST,
    TERRAIN_PATH,
    TERRAIN_ROCK,
    TERRAIN_COUNT
};

#define CELL_NO_OCCUPANT ((s8)-1)
#define MAP_VISION_UNSEEN   0u
#define MAP_VISION_EXPLORED 1u
#define MAP_VISION_VISIBLE  2u

typedef struct
{
    TerrainType terrain;
    s8 occupantId; /* -1 = casilla libre; otros valores identifican unidades. */
    u8 eventSetId; /* Índice a una tabla compartida; nunca guarda punteros. */
    /* Estado de visión de Azul en bits 0-1 y Rojo en bits 2-3. */
    u8 visionState;
} CellData;

typedef struct
{
    const char *name;
    u8 moveCost;       /* Coste base (infantería); 0 si ningún tipo puede entrar. */
    bool passable;
    u8 defenseBonus;   /* Cobertura adicional al defender en esta casilla. */
    bool blocksLineOfSight; /* Bloquea si el rayo atraviesa el interior de la celda. */
} TerrainInfo;

/* Capa lógica independiente del MAP gráfico de SGDK. MapData puede modelar
 * mapas mayores; cada consumidor debe validar sus propios límites de capacidad. */
struct PathfinderWorkspace;

typedef struct
{
    u16 width;                    /* Dimensión lógica del escenario activo. */
    u16 height;
    const char *const *terrainRows; /* W=agua, F=bosque, P=camino, R=piedras. */
    u16 terrainRowCount;          /* Capacidad de terrainRows. */
    CellData *cells;              /* Estado editable durante la partida. */
    u32 cellCapacity;             /* Número de celdas reservadas en cells. */
    struct PathfinderWorkspace *pathWorkspace; /* Búferes requeridos si hay unidades. */
} MapData;

extern const TerrainInfo terrainInfo[TERRAIN_COUNT];
extern MapData worldMapData;
/* Mismo almacenamiento de RAM que worldMapData; solo uno puede estar activo. */
extern MapData bridgeMapData;
/* Escaramuza compacta 20x20; comparte buffers con los otros escenarios. */
extern MapData bridgeSkirmishMapData;
/* Cruce de las ruinas: variante girada del mapa compacto, con RAM compartida. */
extern MapData ruinsCrossingMapData;

/*
 * Valida todas las filas y símbolos antes de escribir cells. MapData admite tamaños
 * mayores que la partida; los consumidores con buffers estáticos deben validar sus
 * propios límites (GAMEPLAY_MAP_MAX_SIDE / GAMEPLAY_MAP_MAX_CELLS).
 */
bool MapData_init(MapData *map);
CellData *MapData_getCell(MapData *map, u16 x, u16 y);
const CellData *MapData_getCellConst(const MapData *map, u16 x, u16 y);
/* Devuelve el estado de visión empaquetado para el equipo (0=Azul, 1=Rojo). */
u8 MapData_getVisionState(const MapData *map, u8 team, u16 x, u16 y);
/* Visión de espectador: visible si cualquiera ve la celda; explorada si al
   menos un equipo la conoce; desconocida solo si ambos la desconocen. */
u8 MapData_getCombinedVisionState(const MapData *map, u16 x, u16 y);
const TerrainInfo *MapData_getTerrainInfo(TerrainType terrain);
/* El destino puede ser opaco; las celdas tocadas solo por una esquina no bloquean. */
bool MapData_hasLineOfSight(const MapData *map, u16 fromX, u16 fromY,
                            u16 toX, u16 toY);
bool MapData_isPassable(const MapData *map, u16 x, u16 y);
bool MapData_isOccupied(const MapData *map, u16 x, u16 y);
bool MapData_canEnter(const MapData *map, u16 x, u16 y);
bool MapData_setOccupant(MapData *map, u16 x, u16 y, s8 occupantId);
bool MapData_setEventSet(MapData *map, u16 x, u16 y, u8 eventSetId);

#endif
