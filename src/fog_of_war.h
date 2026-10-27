#ifndef FOG_OF_WAR_H
#define FOG_OF_WAR_H

#include <genesis.h>
#include "map_data.h"
#include "team.h"
#include "unit_controller.h"

/* Capacidad estática de la partida; no es una restricción general de MapData. */
#define FOG_MAX_MAP_CELLS GAMEPLAY_MAP_MAX_CELLS
#define FOG_MAX_UNITS     GAME_MAX_UNITS

typedef enum
{
    FOG_CELL_UNSEEN = MAP_VISION_UNSEEN,
    FOG_CELL_EXPLORED = MAP_VISION_EXPLORED,
    FOG_CELL_VISIBLE = MAP_VISION_VISIBLE
} FogCellState;

typedef struct
{
    u16 x;
    u16 y;
    u8 visionRange;
    /* Rango completo de exploración; no se reduce al gastar puntos este turno. */
    u8 maxMovementPoints;
    u8 id;
    UnitType type;
    TeamId team;
    bool alive;
    bool valid;
} FogUnitSnapshot;

typedef struct
{
    u16 minX;
    u16 minY;
    u16 maxX;
    u16 maxY;
    bool active;
} FogDirtyRegion;

/*
 * Cachés temporales de un solo equipo observador. La exploración persistente vive
 * en MapData; dirtyCellIndices solo comunica qué debe repintar la capa visual.
 */
typedef struct
{
    MapData *map;
    const UnitController *units;
    TeamId viewerTeam;
    bool initialized;
    /* Las candidatas se acumulan durante la animación y se procesan al acabar. */
    bool movementExplorationPending;
    bool movementRecalcAllPending;
    bool movementRecalcMask[FOG_MAX_UNITS];
    /* La unidad que acaba de moverse se explora antes que las demás candidatas. */
    u8 movementPriorityUnitIndex;

    /* Caché de la última posición lógica para evitar recorrer el mapa entero. */
    FogUnitSnapshot unitSnapshots[FOG_MAX_UNITS];
    u8 cachedUnitCount;
    /* Índices compactos por equipo: evita filtrar todos los snapshots en cada celda. */
    u8 teamUnitIndices[TEAM_COUNT][FOG_MAX_UNITS];
    u8 teamUnitCount[TEAM_COUNT];

    /* Índices lineales de las casillas cuya visión cambió para viewerTeam. */
    u16 dirtyCellIndices[FOG_MAX_MAP_CELLS];
    u8 dirtyTileFlags[FOG_MAX_MAP_CELLS];
    u16 dirtyCellCount;
    u16 lastProcessedCellCount;
} FogOfWar;

bool FogOfWar_init(FogOfWar *fog, MapData *map,
                   const UnitController *units, TeamId viewerTeam);
/* Actualiza solo los rectángulos de visión afectados por cambios en unidades. */
bool FogOfWar_update(FogOfWar *fog, TeamId viewerTeam);
FogCellState FogOfWar_getCellState(const MapData *map, TeamId team,
                                   u16 x, u16 y);
bool FogOfWar_isVisible(const MapData *map, TeamId team, u16 x, u16 y);
bool FogOfWar_isExplored(const MapData *map, TeamId team, u16 x, u16 y);
u16 FogOfWar_getDirtyCellCount(const FogOfWar *fog);
bool FogOfWar_getDirtyCell(const FogOfWar *fog, u16 dirtyIndex,
                           u16 *x, u16 *y);
/* TRUE solo si cambió unseen/known y es necesario tocar los tiles del mapa. */
bool FogOfWar_didTileChange(const FogOfWar *fog, u16 dirtyIndex);
u16 FogOfWar_getLastProcessedCellCount(const FogOfWar *fog);
/* TRUE mientras quede alcance aliado pendiente de procesar en fotogramas posteriores. */
bool FogOfWar_hasPendingMovementExploration(const FogOfWar *fog);

#endif
