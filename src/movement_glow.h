#ifndef MOVEMENT_GLOW_H
#define MOVEMENT_GLOW_H

#include <genesis.h>
#include "map_data.h"
#include "team.h"

/* Duplica la paleta del terreno en PAL1 y registra el parcheo del mapa. */
bool MovementGlow_init(Map *map, const u16 *terrainPalette);
/* Invalida referencias y cachés antes de liberar el mapa de una partida. */
void MovementGlow_release(void);

/* El puntero debe mantenerse valido mientras el efecto este activo. */
void MovementGlow_setReachableMask(const u8 *mask, u16 mapWidth,
                                   u16 mapHeight);
/* Compatibilidad: máscara binaria de enemigos actualmente atacables. */
void MovementGlow_setAttackableMask(const u8 *mask, u16 mapWidth,
                                    u16 mapHeight);
/* La máscara de acciones usa COMBAT_TARGET_ATTACKABLE (rojo) y
   COMBAT_TARGET_REPAIRABLE (resaltado de apoyo). Solo cambia con la selección
   o tras una acción; nunca se reconstruye durante el scroll por fotograma. */
bool MovementGlow_setMasks(const u8 *reachable, const u8 *actionTargets,
                           u16 mapWidth, u16 mapHeight,
                           u32 cameraX, u32 cameraY);
/* Quita solo el resaltado de blancos atacables y restaura el estilo inferior. */
bool MovementGlow_clearAttackableMask(u32 cameraX, u32 cameraY);
/* Retira el alcance sin reconstruir y redibujar todo el viewport. */
bool MovementGlow_clearReachableMask(u32 cameraX, u32 cameraY);
/* Las casillas nunca exploradas se sustituyen por un tile opaco de niebla. */
void MovementGlow_setFogContext(const MapData *map, TeamId viewerTeam,
                                u16 fogTileIndex, bool enabled,
                                bool sharedVision);

/* Redibuja el area visible para cambios globales de contexto. */
void MovementGlow_redraw(u32 cameraX, u32 cameraY);
/*
 * Refresca una fila de celdas lógicas contiguas. Agrupa los tiles visuales
 * en transferencias de fila para reducir el número de entradas DMA.
 * La caché se actualiza incluso si las celdas quedan fuera del viewport.
 */
bool MovementGlow_refreshFogCellRun(u16 firstCellX, u16 lastCellX, u16 cellY,
                                    u32 cameraX, u32 cameraY);
/* Compatibilidad para refrescar una sola celda. */
bool MovementGlow_refreshFogCell(u16 cellX, u16 cellY,
                                 u32 cameraX, u32 cameraY);

/* Pulso continuo de PAL1; llamar una vez por fotograma. */
void MovementGlow_update(void);

#endif
