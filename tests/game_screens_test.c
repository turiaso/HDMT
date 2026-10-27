#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game_screens.h"
#include "game_config.h"

#define MAX_DRAW_RECORDS 64u
#define MAX_TEXT_LENGTH 40u

typedef struct
{
    char text[MAX_TEXT_LENGTH];
    u16 x;
    u16 y;
    u16 width;
} DrawRecord;

static DrawRecord drawRecords[MAX_DRAW_RECORDS];
static u16 drawRecordCount;
static u16 joySequence[8];
static u8 joySequenceLength;
static u8 joySequenceIndex;

static const char *reasonText = "OBJETIVO MANTENIDO";
static bool objectiveComplete = TRUE;
static u16 aliveBlue = 7;
static u16 aliveRed = 2;

void VDP_clearPlane(u16 plane, bool wait)
{
    (void)plane;
    (void)wait;
}

void VDP_setTextPlane(u16 plane) { (void)plane; }
void VDP_setTextPalette(u16 palette) { (void)palette; }
void VDP_setTextPriority(u16 priority) { (void)priority; }

void VDP_drawTextBGFill(u16 plane, const char *text, u16 x, u16 y, u16 len)
{
    DrawRecord *record;
    (void)plane;
    assert(drawRecordCount < MAX_DRAW_RECORDS);
    record = &drawRecords[drawRecordCount++];
    (void)snprintf(record->text, sizeof(record->text), "%s", text);
    record->x = x;
    record->y = y;
    record->width = len;
}

void VDP_setTileMapXY(u16 plane, u16 tile, u16 x, u16 y)
{
    (void)plane;
    (void)tile;
    (void)x;
    (void)y;
}

void SYS_doVBlankProcess(void) { }

u16 JOY_readJoypad(u16 joy)
{
    (void)joy;
    if (joySequenceIndex >= joySequenceLength) return 0;
    return joySequence[joySequenceIndex++];
}

void uintToStr(u32 value, char *dest, u16 minDigits)
{
    (void)snprintf(dest, 8, "%0*u", (int)minDigits, (unsigned)value);
}

/* Solo se simulan las consultas que usa la pantalla final. */
GameOutcome GameTurn_getOutcome(const GameTurn *game)
{
    return game->outcome;
}

const MissionObjective *GameTurn_getObjective(const GameTurn *game)
{
    return &game->objective;
}

const char *GameTurn_getOutcomeText(const GameTurn *game)
{
    switch (game->outcome)
    {
        case GAME_OUTCOME_BLUE_VICTORY:
            return "VICTORIA AZUL / DERROTA ROJA";
        case GAME_OUTCOME_RED_VICTORY:
            return "VICTORIA ROJA / DERROTA AZUL";
        case GAME_OUTCOME_DRAW:
            return "EMPATE";
        default:
            return "FIN DE LA MISION";
    }
}

const char *GameTurn_getOutcomeReasonText(const GameTurn *game)
{
    (void)game;
    return reasonText;
}

bool GameTurn_didCompleteObjective(const GameTurn *game)
{
    (void)game;
    return objectiveComplete;
}

u16 GameTurn_getTurnsElapsed(const GameTurn *game)
{
    return game->turnsElapsed;
}

u16 GameTurn_getAliveUnitCount(const GameTurn *game, TeamId team)
{
    (void)game;
    return (team == TEAM_BLUE) ? aliveBlue : aliveRed;
}

u16 GameTurn_getMissionProgress(const GameTurn *game)
{
    return game->missionProgress;
}

static bool hasDraw(const char *text, u16 x, u16 y)
{
    u16 i;
    for (i = 0; i < drawRecordCount; ++i)
    {
        const DrawRecord *record = &drawRecords[i];
        if ((record->x == x) && (record->y == y) &&
            (strcmp(record->text, text) == 0))
            return TRUE;
    }
    return FALSE;
}

static void resetDrawingAndJoy(void)
{
    memset(drawRecords, 0, sizeof(drawRecords));
    drawRecordCount = 0;
    joySequenceIndex = 0;
    joySequenceLength = 0;
}

static void testMissionSummaryAndReplay(void)
{
    GameTurn game = { 0 };
    ScenarioDefinition scenario = { 0 };
    MissionResultAction action;
    u16 i;

    game.objective.type = MISSION_HOLD_POINT;
    game.objective.requiredTurns = 3;
    game.outcome = GAME_OUTCOME_BLUE_VICTORY;
    game.turnsElapsed = 8;
    game.missionProgress = 3;
    scenario.objective.name = "DEFENSA DEL PUENTE";
    reasonText = "OBJETIVO MANTENIDO";
    objectiveComplete = TRUE;
    aliveBlue = 7;
    aliveRed = 2;

    /* Lectura inicial sin botón, espera y pulsación A; después se libera. */
    joySequence[0] = 0;
    joySequence[1] = 0;
    joySequence[2] = BUTTON_A;
    joySequence[3] = 0;
    joySequenceLength = 4;

    action = GameScreens_showMissionResult(&game, &scenario, 42);
    assert(action == MISSION_RESULT_REPLAY);
    assert(hasDraw("RESUMEN DE MISION", 11, 3));
    assert(hasDraw("VICTORIA AZUL / DERROTA ROJA", 4, 5));
    assert(hasDraw("DEFENSA DEL PUENTE", 3, 7));
    assert(hasDraw("OBJETIVO MANTENIDO", 11, 9));
    assert(hasDraw("OBJETIVO: COMPLETADO", 3, 11));
    assert(hasDraw("TURNOS TRANSCURRIDOS:", 3, 13));
    assert(hasDraw("8", 27, 13));
    assert(hasDraw("7", 26, 15));
    assert(hasDraw("2", 26, 16));
    assert(hasDraw("3", 23, 18));
    assert(hasDraw("/", 26, 18));
    assert(hasDraw("3", 27, 18));
    assert(hasDraw("A: REINICIAR MISION", 7, 22));
    assert(hasDraw("B/START: SELECTOR DE MISION", 4, 24));

    /* Ningún texto invade las cuatro columnas de máscara ni la fila inferior. */
    for (i = 0; i < drawRecordCount; ++i)
    {
        assert((u16)(drawRecords[i].x + drawRecords[i].width) <= 35);
        assert(drawRecords[i].y < 27);
    }
}

static void testMenuInputAndDrawSummary(void)
{
    GameTurn game = { 0 };
    ScenarioDefinition scenario = { 0 };

    resetDrawingAndJoy();
    game.objective.type = MISSION_CAPTURE_POINT;
    game.outcome = GAME_OUTCOME_DRAW;
    game.turnsElapsed = 4;
    scenario.objective.name = "PUNTO DE CONTROL";
    reasonText = "AMBOS EQUIPOS ELIMINADOS";
    objectiveComplete = FALSE;
    aliveBlue = 0;
    aliveRed = 0;

    joySequence[0] = 0;          /* Espera a que se suelte el botón anterior. */
    joySequence[1] = BUTTON_B;   /* Volver al selector. */
    joySequence[2] = 0;          /* Espera a que se suelte B. */
    joySequenceLength = 3;

    assert(GameScreens_showMissionResult(&game, &scenario, 42) ==
           MISSION_RESULT_RETURN_TO_MENU);
    assert(hasDraw("EMPATE", 4, 5));
    assert(hasDraw("AMBOS EQUIPOS ELIMINADOS", 11, 9));
    assert(hasDraw("OBJETIVO: SIN RESOLVER", 3, 11));
    assert(hasDraw("0", 26, 15));
    assert(hasDraw("0", 26, 16));

    /* START debe volver al selector igual que B. */
    resetDrawingAndJoy();
    joySequence[0] = 0;
    joySequence[1] = BUTTON_START;
    joySequence[2] = 0;
    joySequenceLength = 3;
    assert(GameScreens_showMissionResult(&game, &scenario, 42) ==
           MISSION_RESULT_RETURN_TO_MENU);
}


static void testSurvivalDefeatSummary(void)
{
    GameTurn game = { 0 };
    ScenarioDefinition scenario = { 0 };

    resetDrawingAndJoy();
    game.objective.type = MISSION_SURVIVE_TURNS;
    game.objective.requiredTurns = 3;
    game.objective.targetTeam = TEAM_BLUE;
    game.outcome = GAME_OUTCOME_RED_VICTORY;
    game.turnsElapsed = 5;
    game.missionProgress = 1;
    scenario.objective.name = "SOBREVIVE TRES TURNOS";
    reasonText = "OBJETIVO ELIMINADO";
    objectiveComplete = FALSE;
    aliveBlue = 0;
    aliveRed = 1;

    joySequence[0] = 0;
    joySequence[1] = 0;
    joySequence[2] = BUTTON_A;
    joySequence[3] = 0;
    joySequenceLength = 4;

    assert(GameScreens_showMissionResult(&game, &scenario, 42) ==
           MISSION_RESULT_REPLAY);
    assert(hasDraw("VICTORIA ROJA / DERROTA AZUL", 4, 5));
    assert(hasDraw("OBJETIVO ELIMINADO", 11, 9));
    assert(hasDraw("OBJETIVO: NO CUMPLIDO", 3, 11));
    assert(hasDraw("1", 26, 16));
    assert(hasDraw("1", 23, 18));
    assert(hasDraw("/", 26, 18));
    assert(hasDraw("3", 27, 18));
}

int main(void)
{
    resetDrawingAndJoy();
    testMissionSummaryAndReplay();
    testMenuInputAndDrawSummary();
    testSurvivalDefeatSummary();
    puts("PASS: resumen de victoria, derrota, empate, supervivientes, progreso y controles finales");
    return 0;
}
