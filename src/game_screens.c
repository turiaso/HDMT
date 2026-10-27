#include <genesis.h>

#include "game_screens.h"
#include "game_config.h"

/*
 * Presentación de pantallas sin estado permanente de partida. Recibe los datos
 * que necesita por parámetro para no depender de globals del bucle principal.
 */

void GameScreens_drawViewportMasks(u16 fogTileIndex)
{
    const u16 maskTile = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, fogTileIndex);
    u16 x;
    u16 y;
    const u16 firstRightTile = VIEW_PIXELS_W / HARDWARE_TILE;
    const u16 firstBottomTile = VIEW_PIXELS_H / HARDWARE_TILE;

    /* BG_B ya contiene el HUD. Su ancho se limita a 35 caracteres para dejar
       libres las cuatro columnas derechas, de modo que los tiles de máscara
       nunca pisan texto. La fila inferior tampoco se usa para el HUD. */
    for (y = 0; y < SCREEN_PIXELS_H / HARDWARE_TILE; y++)
        for (x = firstRightTile; x < SCREEN_PIXELS_W / HARDWARE_TILE; x++)
            VDP_setTileMapXY(BG_B, maskTile, x, y);

    for (x = 0; x < SCREEN_PIXELS_W / HARDWARE_TILE; x++)
        VDP_setTileMapXY(BG_B, maskTile, x, firstBottomTile);
}


MissionResultAction GameScreens_showMissionResult(const GameTurn *game,
                                                  const ScenarioDefinition *scenario,
                                                  u16 fogTileIndex)
{
    const GameOutcome outcome = GameTurn_getOutcome(game);
    const MissionObjective *objective = GameTurn_getObjective(game);
    const char *resultText = GameTurn_getOutcomeText(game);
    const char *reasonText = GameTurn_getOutcomeReasonText(game);
    const char *objectiveStatus = GameTurn_didCompleteObjective(game)
        ? "OBJETIVO: COMPLETADO"
        : ((outcome == GAME_OUTCOME_DRAW) ? "OBJETIVO: SIN RESOLVER"
                                          : "OBJETIVO: NO CUMPLIDO");
    char valueText[8];
    char requiredText[8];
    u16 previousJoy = 0;

    VDP_clearPlane(BG_B, TRUE);
    VDP_setTextPlane(BG_B);
    VDP_setTextPalette(PAL3);
    VDP_setTextPriority(TRUE);
    GameScreens_drawViewportMasks(fogTileIndex);

    /* Resumen en filas fijas; todas quedan fuera de las máscaras del borde. */
    VDP_drawTextBGFill(BG_B, "RESUMEN DE MISION", 11, 3, 18);
    VDP_drawTextBGFill(BG_B, resultText, 4, 5, 31);
    VDP_drawTextBGFill(BG_B, scenario->objective.name, 3, 7, 32);
    VDP_drawTextBGFill(BG_B, "MOTIVO:", 3, 9, 7);
    VDP_drawTextBGFill(BG_B, reasonText, 11, 9, 24);
    VDP_drawTextBGFill(BG_B, objectiveStatus, 3, 11, 24);

    VDP_drawTextBGFill(BG_B, "TURNOS TRANSCURRIDOS:", 3, 13, 22);
    uintToStr(GameTurn_getTurnsElapsed(game), valueText, 1);
    VDP_drawTextBGFill(BG_B, valueText, 27, 13, 5);

    VDP_drawTextBGFill(BG_B, "AZUL SUPERVIVIENTES:", 3, 15, 21);
    uintToStr(GameTurn_getAliveUnitCount(game, TEAM_BLUE), valueText, 1);
    VDP_drawTextBGFill(BG_B, valueText, 26, 15, 4);
    VDP_drawTextBGFill(BG_B, "ROJO SUPERVIVIENTES:", 3, 16, 21);
    uintToStr(GameTurn_getAliveUnitCount(game, TEAM_RED), valueText, 1);
    VDP_drawTextBGFill(BG_B, valueText, 26, 16, 4);

    if ((objective != NULL) &&
        ((objective->type == MISSION_HOLD_POINT) ||
         (objective->type == MISSION_SURVIVE_TURNS)))
    {
        const u16 required = objective->requiredTurns
            ? objective->requiredTurns : 1u;
        VDP_drawTextBGFill(BG_B, "PROGRESO OBJETIVO:", 3, 18, 19);
        uintToStr(GameTurn_getMissionProgress(game), valueText, 1);
        uintToStr(required, requiredText, 1);
        VDP_drawTextBGFill(BG_B, valueText, 23, 18, 3);
        VDP_drawTextBGFill(BG_B, "/", 26, 18, 1);
        VDP_drawTextBGFill(BG_B, requiredText, 27, 18, 3);
    }

    VDP_drawTextBGFill(BG_B, "A: REINICIAR MISION", 7, 22, 24);
    VDP_drawTextBGFill(BG_B, "B/START: SELECTOR DE MISION", 4, 24, 31);
    SYS_doVBlankProcess();

    /* No reutilices el botón que pudo haber causado la victoria/derrota. */
    while ((JOY_readJoypad(JOY_1) &
            (BUTTON_A | BUTTON_B | BUTTON_START)) != 0)
        SYS_doVBlankProcess();

    while (TRUE)
    {
        const u16 joy = JOY_readJoypad(JOY_1);
        const bool replayPressed = ((joy & BUTTON_A) != 0) &&
                                   ((previousJoy & BUTTON_A) == 0);
        const bool menuPressed =
            (((joy & BUTTON_B) != 0) && ((previousJoy & BUTTON_B) == 0)) ||
            (((joy & BUTTON_START) != 0) &&
             ((previousJoy & BUTTON_START) == 0));
        const MissionResultAction action =
            MissionResult_resolveInput(replayPressed, menuPressed);

        if (action != MISSION_RESULT_WAIT)
        {
            while ((JOY_readJoypad(JOY_1) &
                    (BUTTON_A | BUTTON_B | BUTTON_START)) != 0)
                SYS_doVBlankProcess();
            return action;
        }

        previousJoy = joy;
        SYS_doVBlankProcess();
    }
}
