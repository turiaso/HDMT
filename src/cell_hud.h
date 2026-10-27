#ifndef CELL_HUD_H
#define CELL_HUD_H

#include <genesis.h>
#include "map_data.h"
#include "team.h"
#include "game_turn.h"
#include "unit_types.h"
#include "unit_controller.h"

void CellHud_init(void);
/* Permite mostrar tipo, equipo y PV de ocupantes que la niebla deja ver. */
void CellHud_setUnitController(const UnitController *controller);
void CellHud_setFogContext(TeamId viewerTeam, bool enabled,
                           bool sharedVision);
/* Actualiza el coste mostrado según el tipo de unidad seleccionada. */
void CellHud_setMovementTerrainProfile(UnitType type, bool hasSelectedUnit);
void CellHud_setUnitStatus(const char *status);
/* Estado del cursor y previsiones: respetan el feedback reciente de una accion. */
void CellHud_setContextStatus(const char *status);
/* Resultado de accion visible hasta que expire o el cursor cambie de casilla. */
void CellHud_setActionFeedback(const char *status);
/* Avanza el temporizador de feedback una vez por fotograma de juego. */
bool CellHud_tick(void);
void CellHud_setUnitCombatInfo(const char *shortName, u8 health,
                               u8 maxHealth, u8 attack, u8 defense,
                               u8 attackRange, u8 visionRange,
                               u8 repairRange, u8 repairAmount);
/* Vista previa y confirmacion de reparacion, con los PV resultantes del aliado. */
void CellHud_setRepairPreview(u8 healthRestored, u8 resultingHealth,
                              u8 maxHealth);
void CellHud_setRepairResult(u8 healthRestored, u8 resultingHealth,
                             u8 maxHealth);
void CellHud_setTurnInfo(const char *teamName, u16 turnNumber,
                         bool hasSelectedUnit, u8 movementPoints,
                         u8 maxMovementPoints, u16 objectiveX,
                         u16 objectiveY, const char *outcomeText,
                         TeamId missionHolder, MissionType missionType,
                         u16 missionProgress,
                         u8 requiredTurns);
void CellHud_setRouteStatus(bool reachable, u16 steps, u8 cost, u8 availablePoints);
void CellHud_setCombatPreview(u8 damage, u8 counterDamage,
                              u8 targetHealth, u8 targetMaxHealth);
/* Repinta la casilla actual sin disparar eventos de cursor. */
void CellHud_refresh(void);
/* Muestra una casilla sin despachar eventos STAY/CLICK (vista de espectador). */
void CellHud_inspectCell(MapData *map, u16 cellX, u16 cellY);
void CellHud_update(MapData *data, s16 cameraPixelX, s16 cameraPixelY,
                    s16 cursorPixelX, s16 cursorPixelY, bool clicked);

#endif
