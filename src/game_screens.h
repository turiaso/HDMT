#ifndef GAME_SCREENS_H
#define GAME_SCREENS_H

#include <genesis.h>
#include "game_turn.h"
#include "scenario.h"
#include "mission_result.h"

/* Dibuja un borde de máscara para pantallas de resultado que lo necesiten. */
void GameScreens_drawViewportMasks(u16 fogTileIndex);

/* Muestra el resultado y devuelve repetir misión o regresar al selector. */
MissionResultAction GameScreens_showMissionResult(const GameTurn *game,
                                                  const ScenarioDefinition *scenario,
                                                  u16 fogTileIndex);

#endif
