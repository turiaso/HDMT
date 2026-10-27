#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cell_events.h"
#include "game_turn.h"
#include "combat.h"
#include "game_config.h"

static u16 turnStartCount;
static u16 turnEndCount;
static u8 lastTeam;
static u16 lastTurn;
static UnitController *turnEndEffectController;
static u8 turnEndEffectUnitId;
static u16 turnEndEffectX;
static u16 turnEndEffectY;
static bool removeUnitAtTurnEnd;
static UnitController *turnEndSecondEffectController;
static u8 turnEndSecondEffectUnitId;
static u16 turnEndSecondEffectX;
static u16 turnEndSecondEffectY;
static bool removeSecondUnitAtTurnEnd;

static void recordTurnEvent(const CellEventContext *context)
{
    assert(context != NULL);
    assert(context->source == CELL_EVENT_SOURCE_SYSTEM);
    assert(context->teamId < TEAM_COUNT);
    assert(context->turnNumber > 0);
    lastTeam = context->teamId;
    lastTurn = context->turnNumber;
    if (context->type == CELL_EVENT_TURN_START) turnStartCount++;
    else if (context->type == CELL_EVENT_TURN_END)
    {
        turnEndCount++;
        if (removeUnitAtTurnEnd &&
            (context->x == turnEndEffectX) &&
            (context->y == turnEndEffectY))
        {
            assert(turnEndEffectController != NULL);
            assert(UnitController_removeUnit(turnEndEffectController,
                                             turnEndEffectUnitId));
            removeUnitAtTurnEnd = FALSE;
        }
        if (removeSecondUnitAtTurnEnd &&
            (context->x == turnEndSecondEffectX) &&
            (context->y == turnEndSecondEffectY))
        {
            assert(turnEndSecondEffectController != NULL);
            assert(UnitController_removeUnit(turnEndSecondEffectController,
                                             turnEndSecondEffectUnitId));
            removeSecondUnitAtTurnEnd = FALSE;
        }
    }
    else assert(FALSE);
}

static const CellEventSet eventSets[] =
{
    { { 0 } },
    { {
        [CELL_EVENT_TURN_START] = recordTurnEvent,
        [CELL_EVENT_TURN_END] = recordTurnEvent
    } }
};

static void finishMovement(UnitController *controller, GameTurn *game)
{
    u16 frame;
    for (frame = 0; UnitController_isMoving(controller) && (frame < 100); frame++)
    {
        (void)UnitController_update(controller);
        (void)GameTurn_checkObjective(game);
    }
    assert(frame < 100);
}

static void moveHorizontal(UnitController *controller, GameTurn *game,
                           u16 fromX, u16 toX)
{
    assert(UnitController_activate(controller, fromX, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(controller, toX, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(controller, toX, 0) == UNIT_ACTION_MOVING);
    finishMovement(controller, game);
}

typedef struct
{
    CellData cells[4];
    u8 costs[4];
    u8 parents[4];
    u8 reachable[4];
    PathfinderWorkspace workspace;
    Unit units[4];
    UnitController controller;
    GameTurn game;
    MapData map;
} GameTurnFixture;

static const char *const fourPathRows[] = { "PPPP" };

static bool initGameTurnFixture(GameTurnFixture *fixture,
                                const UnitStart *starts, u8 startCount,
                                const MissionObjective *objective,
                                bool useTurnEventHandlers)
{
    fixture->workspace = (PathfinderWorkspace){
        fixture->costs, fixture->parents, 4, fixture->reachable
    };
    fixture->map = (MapData){
        .width = 4,
        .height = 1,
        .terrainRows = fourPathRows,
        .terrainRowCount = 1,
        .cells = fixture->cells,
        .cellCapacity = 4,
        .pathWorkspace = &fixture->workspace
    };

    if (!MapData_init(&fixture->map)) return FALSE;
    if (useTurnEventHandlers)
        CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    else
        CellEvents_init(NULL, 0);

    if (!UnitController_init(&fixture->controller, &fixture->map,
                             fixture->units, 4, starts, startCount))
        return FALSE;
    return GameTurn_init(&fixture->game, &fixture->map, &fixture->controller,
                         TEAM_BLUE, objective);
}


static void testCaptureDuringRouteFinishesMatchAtObjective(void)
{
    static const char *const rows[] = { "PPPP" };
    CellData cells[4];
    u8 costs[4];
    u8 parents[4];
    u8 reachable[4];
    PathfinderWorkspace workspace = { costs, parents, 4, reachable };
    Unit units[1];
    UnitController controller;
    GameTurn game;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    const MissionObjective objective = { "OBJETIVO", 1, 0, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0 };
    MapData map =
    {
        .width = 4, .height = 1,
        .terrainRows = rows, .terrainRowCount = 1,
        .cells = cells, .cellCapacity = 4,
        .pathWorkspace = &workspace
    };
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    assert(UnitController_init(&controller, &map, units, 1, starts, 1));
    assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));

    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 3, 0) == UNIT_ACTION_MOVING);

    /* Pasar por el objetivo termina la partida y detiene la ruta en esa celda. */
    for (frame = 0; (frame < 100) && UnitController_isMoving(&controller); frame++)
    {
        (void)UnitController_update(&controller);
        (void)GameTurn_checkObjective(&game);
    }

    assert(frame < 100);
    assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_BLUE_VICTORY);
    assert(!UnitController_isMoving(&controller));
    assert(UnitController_getSelected(&controller) == NULL);
    assert(units[0].x == objective.x && units[0].y == objective.y);
    assert(MapData_getCell(&map, objective.x, objective.y)->occupantId == 0);
    assert(units[0].x != 3); /* La ruta no continúa después de capturar. */
}


static void testHoldSurviveAndEliminateMissions(void)
{
    /* Mantener un punto: solo progresa al final del turno de quien lo ocupa. */
    {
        static const char *const rows[] = { "PPPP" };
        CellData cells[4];
        u8 costs[4], parents[4], reachable[4];
        PathfinderWorkspace workspace = { costs, parents, 4, reachable };
        MapData map = { .width = 4, .height = 1, .terrainRows = rows,
            .terrainRowCount = 1, .cells = cells, .cellCapacity = 4,
            .pathWorkspace = &workspace };
        Unit units[2];
        UnitController controller;
        GameTurn game;
        const UnitStart starts[] = {
            { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
            { 1, 3, 0, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
        };
        const MissionObjective objective = {
            "MANTENER", 1, 0, MISSION_HOLD_POINT, 2, TEAM_BLUE, 0
        };
        u16 frame;

        assert(MapData_init(&map));
        CellEvents_init(NULL, 0);
        assert(UnitController_init(&controller, &map, units, 2, starts, 2));
        assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));
        assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
        assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
        assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_MOVING);
        for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        {
            (void)UnitController_update(&controller);
            (void)GameTurn_checkObjective(&game);
        }
        assert(frame < 100 && units[0].x == 1);
        assert(GameTurn_getMissionProgress(&game) == 0);
        assert(GameTurn_endTurn(&game));
        assert(GameTurn_getMissionProgress(&game) == 1);
        assert(!GameTurn_isFinished(&game));
        assert(GameTurn_endTurn(&game)); /* Turno rojo no suma. */
        assert(GameTurn_getMissionProgress(&game) == 1);
        assert(GameTurn_endTurn(&game));
        assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_BLUE_VICTORY);
    }

    /* Sobrevivir N turnos cuenta únicamente los turnos del equipo objetivo. */
    {
        static const char *const rows[] = { "PPPP", "PPPP" };
        CellData cells[8];
        u8 costs[8], parents[8], reachable[8];
        PathfinderWorkspace workspace = { costs, parents, 8, reachable };
        MapData map = { .width = 4, .height = 2, .terrainRows = rows,
            .terrainRowCount = 2, .cells = cells, .cellCapacity = 8,
            .pathWorkspace = &workspace };
        Unit units[2];
        UnitController controller;
        GameTurn game;
        const UnitStart starts[] = {
            { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
            { 1, 3, 1, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
        };
        const MissionObjective objective = {
            "RESISTIR", 0, 1, MISSION_SURVIVE_TURNS, 2, TEAM_BLUE, 0
        };

        assert(MapData_init(&map));
        CellEvents_init(NULL, 0);
        assert(UnitController_init(&controller, &map, units, 2, starts, 2));
        assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));
        assert(GameTurn_endTurn(&game));
        assert(GameTurn_getMissionProgress(&game) == 1);
        assert(GameTurn_endTurn(&game));
        assert(GameTurn_getMissionProgress(&game) == 1);
        assert(GameTurn_endTurn(&game));
        assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_BLUE_VICTORY);
        assert(GameTurn_getTurnsElapsed(&game) == 3);
        assert(strcmp(GameTurn_getOutcomeReasonText(&game),
                      "SUPERVIVENCIA COMPLETADA") == 0);
        assert(GameTurn_didCompleteObjective(&game));
        assert(GameTurn_getAliveUnitCount(&game, TEAM_BLUE) == 1);
        assert(GameTurn_getAliveUnitCount(&game, TEAM_RED) == 1);
        assert(!GameTurn_canIssueCommands(&game));
        assert(!GameTurn_endTurn(&game));
        assert(GameTurn_getTurnsElapsed(&game) == 3);
    }

    /* Eliminar todas las unidades rivales termina la misión inmediatamente. */
    {
        static const char *const rows[] = { "PPP" };
        CellData cells[3];
        u8 costs[3], parents[3], reachable[3];
        PathfinderWorkspace workspace = { costs, parents, 3, reachable };
        MapData map = { .width = 3, .height = 1, .terrainRows = rows,
            .terrainRowCount = 1, .cells = cells, .cellCapacity = 3,
            .pathWorkspace = &workspace };
        Unit units[2];
        UnitController controller;
        GameTurn game;
        const UnitStart starts[] = {
            { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
            { 1, 1, 0, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
        };
        const MissionObjective objective = {
            "ELIMINAR", 2, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
        };
        CombatResult result;

        assert(MapData_init(&map));
        CellEvents_init(NULL, 0);
        assert(UnitController_init(&controller, &map, units, 2, starts, 2));
        assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));
        units[1].health = 1;
        result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
        assert(result.defenderDefeated);
        assert(GameTurn_checkObjective(&game));
        assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_BLUE_VICTORY);
        assert(strcmp(GameTurn_getOutcomeReasonText(&game),
                      "EJERCITO RIVAL ELIMINADO") == 0);
        assert(GameTurn_getAliveUnitCount(&game, TEAM_BLUE) == 1);
        assert(GameTurn_getAliveUnitCount(&game, TEAM_RED) == 0);
        assert(!GameTurn_canIssueCommands(&game));
    }
}

static void testMovementActionConsumedAndRestoredPerTeamTurn(void)
{
    static const char *const rows[] = { "PPPP", "PPPP" };
    CellData cells[8];
    u8 costs[8], parents[8], reachable[8];
    PathfinderWorkspace workspace = { costs, parents, 8, reachable };
    MapData map = { .width = 4, .height = 2, .terrainRows = rows,
        .terrainRowCount = 2, .cells = cells, .cellCapacity = 8,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    GameTurn game;
    const UnitStart starts[] = {
        { 0, 0, 0, 5, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 1, 5, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    const MissionObjective objective = {
        "ELIMINAR", 2, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
    };
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));
    assert(units[0].movementPoints == 5 && units[1].movementPoints == 0);

    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_MOVING);
    finishMovement(&controller, &game);
    assert(units[0].x == 1 && units[0].y == 0);
    assert(units[0].movementPoints == 0);

    /* Elegir de nuevo no permite encadenar otro desplazamiento en el turno. */
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 1) ==
           UNIT_ACTION_NO_MOVEMENT_POINTS);
    assert(units[0].x == 1 && units[0].y == 0);

    assert(GameTurn_endTurn(&game));
    assert(GameTurn_getActiveTeam(&game) == TEAM_RED);
    assert(units[0].movementPoints == 0 && units[1].movementPoints == 5);

    assert(GameTurn_endTurn(&game));
    assert(GameTurn_getActiveTeam(&game) == TEAM_BLUE);
    assert(units[0].movementPoints == 5 && units[1].movementPoints == 0);

    /* El presupuesto solo vuelve cuando llega el siguiente turno azul. */
    assert(UnitController_activate(&controller, 1, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_MOVING);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100 && units[0].x == 0 && units[0].movementPoints == 0);
    puts("PASS: una orden por turno, sin recarga duplicada y con reposición en el siguiente turno propio");
}

static void testEmptyTeamsAndDrawResolution(void)
{
    static const MissionObjective emptyBattleObjectives[] =
    {
        { "CAPTURA", 1, 0, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0 },
        { "MANTENER", 1, 0, MISSION_HOLD_POINT, 2, TEAM_BLUE, 0 },
        { "ELIMINAR", 1, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 },
        { "SOBREVIVIR", 1, 0, MISSION_SURVIVE_TURNS, 2, TEAM_BLUE, 0 }
    };
    static const UnitStart blueStart[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    static const UnitStart redStart[] =
    {
        { 1, 3, 0, 6, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    u8 i;

    /* Sin unidades no hay forma de completar ningún objetivo: siempre empate. */
    for (i = 0; i < (u8)(sizeof(emptyBattleObjectives) /
                         sizeof(emptyBattleObjectives[0])); i++)
    {
        GameTurnFixture fixture;
        assert(initGameTurnFixture(&fixture, NULL, 0,
                                   &emptyBattleObjectives[i], FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_DRAW);
        assert(GameTurn_isFinished(&fixture.game));
        assert(!GameTurn_canIssueCommands(&fixture.game));
        assert(GameTurn_getTurnsElapsed(&fixture.game) == 0);
        assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_BLUE) == 0);
        assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_RED) == 0);
        assert(strcmp(GameTurn_getOutcomeText(&fixture.game),
                      "EMPATE") == 0);
        assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                      "AMBOS EQUIPOS ELIMINADOS") == 0);
        assert(!GameTurn_didCompleteObjective(&fixture.game));
    }

    /* Eliminar todas: un único bando con unidades gana desde la inicialización. */
    {
        GameTurnFixture fixture;
        const MissionObjective objective = {
            "ELIMINAR", 1, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
        };
        assert(initGameTurnFixture(&fixture, blueStart, 1, &objective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_BLUE_VICTORY);
        assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                      "EJERCITO RIVAL ELIMINADO") == 0);
        assert(!GameTurn_canIssueCommands(&fixture.game));
        assert(initGameTurnFixture(&fixture, redStart, 1, &objective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_RED_VICTORY);
        assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                      "EJERCITO RIVAL ELIMINADO") == 0);
    }

    /* Sobrevivir: el rival sin unidades cumple el objetivo; el equipo objetivo
       sin unidades pierde. El caso con ambos bandos vacíos se resuelve arriba. */
    {
        GameTurnFixture fixture;
        const MissionObjective objective = {
            "SOBREVIVIR", 1, 0, MISSION_SURVIVE_TURNS, 3, TEAM_BLUE, 0
        };
        assert(initGameTurnFixture(&fixture, blueStart, 1, &objective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_BLUE_VICTORY);
        assert(initGameTurnFixture(&fixture, redStart, 1, &objective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_RED_VICTORY);
        assert(!GameTurn_didCompleteObjective(&fixture.game));
    }

    /* Captura y mantener conservan su regla explícita: una baja no sustituye
       al objetivo. Si solo queda Azul, la misión sigue activa hasta cumplirlo. */
    {
        GameTurnFixture fixture;
        const MissionObjective captureObjective = {
            "CAPTURA", 1, 0, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0
        };
        const MissionObjective holdObjective = {
            "MANTENER", 1, 0, MISSION_HOLD_POINT, 2, TEAM_BLUE, 0
        };

        assert(initGameTurnFixture(&fixture, blueStart, 1,
                                   &captureObjective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);

        /* Tener el mapa para uno solo no completa una misión de mantener:
           primero hay que ocupar la casilla y cerrar dos turnos azules. */
        assert(initGameTurnFixture(&fixture, blueStart, 1,
                                   &holdObjective, FALSE));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);
        moveHorizontal(&fixture.controller, &fixture.game, 0, 1);
        assert(GameTurn_endTurn(&fixture.game));
        assert(GameTurn_getMissionProgress(&fixture.game) == 1);
        assert(GameTurn_endTurn(&fixture.game)); /* Turno rojo sin unidades. */
        assert(GameTurn_getMissionProgress(&fixture.game) == 1);
        assert(GameTurn_endTurn(&fixture.game));
        assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_BLUE_VICTORY);
    }
    puts("PASS: equipos vacios, empate, supervivientes y objetivos primarios");
}

static void testImmediateResultAtInitializationDoesNotEndAnUnplayedTurn(void)
{
    GameTurnFixture fixture;
    static const UnitStart blueOnly[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    const MissionObjective objective = {
        "ELIMINAR", 1, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
    };

    turnStartCount = 0;
    turnEndCount = 0;
    removeUnitAtTurnEnd = FALSE;
    removeSecondUnitAtTurnEnd = FALSE;
    assert(initGameTurnFixture(&fixture, blueOnly, 1, &objective, TRUE));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_BLUE_VICTORY);
    assert(!GameTurn_canIssueCommands(&fixture.game));
    assert(GameTurn_getTurnsElapsed(&fixture.game) == 0);
    assert(turnStartCount == 4);
    /* El resultado se decide al inicializar; no se simula un cierre de turno. */
    assert(turnEndCount == 0);

    turnStartCount = 0;
    turnEndCount = 0;
    assert(initGameTurnFixture(&fixture, NULL, 0, &objective, TRUE));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_DRAW);
    assert(!GameTurn_canIssueCommands(&fixture.game));
    assert(GameTurn_getTurnsElapsed(&fixture.game) == 0);
    assert(turnEndCount == 0);

    CellEvents_init(NULL, 0);
    puts("PASS: un resultado inmediato al inicializar no emite TURN_END prematuro");
}

static void testSimultaneousTurnEndCasualtiesResolveAsDraw(void)
{
    GameTurnFixture fixture;
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 6, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const MissionObjective objective = {
        "ELIMINAR", 1, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
    };

    turnStartCount = 0;
    turnEndCount = 0;
    removeUnitAtTurnEnd = FALSE;
    removeSecondUnitAtTurnEnd = FALSE;
    assert(initGameTurnFixture(&fixture, starts, 2, &objective, TRUE));

    turnEndEffectController = &fixture.controller;
    turnEndEffectUnitId = 0;
    turnEndEffectX = 0;
    turnEndEffectY = 0;
    removeUnitAtTurnEnd = TRUE;
    turnEndSecondEffectController = &fixture.controller;
    turnEndSecondEffectUnitId = 1;
    turnEndSecondEffectX = 3;
    turnEndSecondEffectY = 0;
    removeSecondUnitAtTurnEnd = TRUE;

    assert(GameTurn_endTurn(&fixture.game));
    assert(!fixture.units[0].alive && !fixture.units[1].alive);
    assert(MapData_getCell(&fixture.map, 0, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(MapData_getCell(&fixture.map, 3, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_DRAW);
    assert(strcmp(GameTurn_getOutcomeText(&fixture.game), "EMPATE") == 0);
    assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                  "AMBOS EQUIPOS ELIMINADOS") == 0);
    assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_BLUE) == 0);
    assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_RED) == 0);
    assert(GameTurn_getTurnsElapsed(&fixture.game) == 1);
    assert(!GameTurn_canIssueCommands(&fixture.game));
    assert(turnEndCount == 4);
    assert(!removeUnitAtTurnEnd && !removeSecondUnitAtTurnEnd);

    turnEndEffectController = NULL;
    turnEndSecondEffectController = NULL;
    CellEvents_init(NULL, 0);
    puts("PASS: bajas simultáneas al cierre producen empate y liberan ambas casillas");
}

static void testTurnEndEffectsRunBeforeObjectiveResolution(void)
{
    GameTurnFixture fixture;
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 6, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const MissionObjective objective = {
        "MANTENER", 1, 0, MISSION_HOLD_POINT, 1, TEAM_BLUE, 0
    };

    turnStartCount = 0;
    turnEndCount = 0;
    removeUnitAtTurnEnd = FALSE;
    assert(initGameTurnFixture(&fixture, starts, 2, &objective, TRUE));
    moveHorizontal(&fixture.controller, &fixture.game, 0, 1);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_BLUE);

    /* Simula un efecto TURN_END que elimina al ocupante. Con el orden anterior,
       la misión ganaba antes de ejecutar este callback. */
    turnEndEffectController = &fixture.controller;
    turnEndEffectUnitId = 0;
    turnEndEffectX = 1;
    turnEndEffectY = 0;
    removeUnitAtTurnEnd = TRUE;

    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);
    assert(GameTurn_getActiveTeam(&fixture.game) == TEAM_RED);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_NONE);
    assert(GameTurn_getMissionProgress(&fixture.game) == 0);
    assert(!fixture.units[0].alive);
    assert(!removeUnitAtTurnEnd);
    /* Cuatro celdas: un único despacho TURN_END, nunca uno duplicado al cerrar. */
    assert(turnEndCount == 4);

    turnEndEffectController = NULL;
    CellEvents_init(NULL, 0);
    puts("PASS: TURN_END aplica cambios antes de resolver el objetivo y se emite una vez");
}

static void testHoldPointAbandonmentAndControlSwitch(void)
{
    GameTurnFixture fixture;
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 6, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const MissionObjective objective = {
        "MANTENER", 1, 0, MISSION_HOLD_POINT, 2, TEAM_BLUE, 0
    };

    assert(initGameTurnFixture(&fixture, starts, 2, &objective, FALSE));

    moveHorizontal(&fixture.controller, &fixture.game, 0, 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);
    assert(GameTurn_endTurn(&fixture.game)); /* Turno rojo: no suma progreso. */
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);

    /* El abandono del punto reinicia inmediatamente toda la racha. */
    moveHorizontal(&fixture.controller, &fixture.game, 1, 0);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_NONE);
    assert(GameTurn_getMissionProgress(&fixture.game) == 0);
    assert(GameTurn_endTurn(&fixture.game));

    /* Rojo ocupa el punto vacío: la racha empieza desde cero para Rojo. */
    moveHorizontal(&fixture.controller, &fixture.game, 3, 1);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_RED);
    assert(GameTurn_getMissionProgress(&fixture.game) == 0);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_RED_VICTORY);
    assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                  "OBJETIVO MANTENIDO") == 0);
    assert(GameTurn_didCompleteObjective(&fixture.game));
    assert(!GameTurn_canIssueCommands(&fixture.game));
    puts("PASS: abandonar el punto reinicia la racha y el nuevo equipo empieza desde cero");
}

static void testHoldPointOccupantKilledAndCapturedByEnemy(void)
{
    GameTurnFixture fixture;
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 6, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const MissionObjective objective = {
        "MANTENER", 1, 0, MISSION_HOLD_POINT, 2, TEAM_BLUE, 0
    };
    CombatResult result;

    assert(initGameTurnFixture(&fixture, starts, 2, &objective, FALSE));
    moveHorizontal(&fixture.controller, &fixture.game, 0, 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);

    /* Rojo se acerca y elimina en combate al ocupante; la racha desaparece. */
    moveHorizontal(&fixture.controller, &fixture.game, 3, 2);
    /* La proximidad del rival no disputa automáticamente la casilla: el
       ocupante conserva el control hasta que la abandone o sea eliminado. */
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_BLUE);
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);
    assert(UnitController_activate(&fixture.controller, 2, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&fixture.controller, 1, 0) == UNIT_ACTION_ENEMY_UNIT);
    assert(MapData_getCell(&fixture.map, 1, 0)->occupantId == 0);
    fixture.units[0].health = 1;
    result = Combat_attack(&fixture.controller, 1, 0, TEAM_RED);
    assert(result.defenderDefeated);
    assert(GameTurn_checkObjective(&fixture.game) == FALSE);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_NONE);
    assert(GameTurn_getMissionProgress(&fixture.game) == 0);

    assert(GameTurn_endTurn(&fixture.game)); /* Fin de Rojo. */
    assert(GameTurn_endTurn(&fixture.game)); /* Azul ya no tiene unidades. */
    moveHorizontal(&fixture.controller, &fixture.game, 2, 1);
    assert(GameTurn_getObjectiveHoldTeam(&fixture.game) == TEAM_RED);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getMissionProgress(&fixture.game) == 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_RED_VICTORY);
    puts("PASS: al eliminar al ocupante se reinicia la racha y el rival puede capturar");
}

static void testZeroRequiredTurnsMeansOneAndDispatchesOnce(void)
{
    GameTurnFixture fixture;
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 6, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    const MissionObjective objective = {
        "MANTENER", 1, 0, MISSION_HOLD_POINT, 0, TEAM_BLUE, 0
    };

    turnStartCount = 0;
    turnEndCount = 0;
    assert(initGameTurnFixture(&fixture, starts, 2, &objective, TRUE));
    moveHorizontal(&fixture.controller, &fixture.game, 0, 1);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_BLUE_VICTORY);
    assert(strcmp(GameTurn_getOutcomeText(&fixture.game),
                  "VICTORIA AZUL / DERROTA ROJA") == 0);
    assert(turnEndCount == 4); /* Ni el objetivo ni finish duplican el evento. */
    assert(!GameTurn_endTurn(&fixture.game));
    assert(turnEndCount == 4);
    CellEvents_init(NULL, 0);
    puts("PASS: requiredTurns=0 equivale a un turno sin duplicar TURN_END");
}


static void testRoundLimitAwardsSurvivorLeader(void)
{
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 1, 0, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 2, 0, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 3, 3, 0, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    const MissionObjective objective =
    {
        "ELIMINAR", 0, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 1
    };
    GameTurnFixture fixture;

    assert(initGameTurnFixture(&fixture, starts, 4, &objective, FALSE));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_RED_VICTORY);
    assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                  "LIMITE DE TURNOS") == 0);
    assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_BLUE) == 1);
    assert(GameTurn_getAliveUnitCount(&fixture.game, TEAM_RED) == 3);
}

static void testRoundLimitResolvesOnlyAfterBothTeamsAct(void)
{
    static const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    const MissionObjective objective =
    {
        "LIMITE", 1, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 1
    };
    GameTurnFixture fixture;

    assert(initGameTurnFixture(&fixture, starts, 2, &objective, FALSE));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);

    /* El limite no termina la partida al cerrar solo el turno azul. */
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getActiveTeam(&fixture.game) == TEAM_RED);
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_ACTIVE);

    /* La primera ronda termina tras el turno rojo; el empate se declara una vez. */
    assert(GameTurn_endTurn(&fixture.game));
    assert(GameTurn_getOutcome(&fixture.game) == GAME_OUTCOME_DRAW);
    assert(GameTurn_isFinished(&fixture.game));
    assert(!GameTurn_didCompleteObjective(&fixture.game));
    assert(strcmp(GameTurn_getOutcomeReasonText(&fixture.game),
                  "LIMITE DE TURNOS") == 0);
    assert(!GameTurn_endTurn(&fixture.game));
}

int main(void)
{
    static const char *const rows[] = { "PWP", "PFP" };
    CellData cells[6];
    u8 costs[6];
    u8 parents[6];
    u8 reachable[6];
    PathfinderWorkspace workspace = { costs, parents, 6, reachable };
    Unit units[2];
    UnitController controller;
    GameTurn game;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 5, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 0, 8, UNIT_TYPE_SCOUT, TEAM_RED }
    };
    const MissionObjective objective = {
        "PUNTO DE CONTROL", 1, 1, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0
    };
    MapData map =
    {
        .width = 3, .height = 2,
        .terrainRows = rows, .terrainRowCount = 2,
        .cells = cells, .cellCapacity = 6,
        .pathWorkspace = &workspace
    };
    u16 frame;
    const Unit *blue;
    const Unit *red;

    turnStartCount = turnEndCount = 0;
    lastTeam = TEAM_NONE;
    lastTurn = 0;
    assert(MapData_init(&map));
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(GameTurn_init(&game, &map, &controller, TEAM_BLUE, &objective));

    blue = UnitController_getUnit(&controller, 0);
    red = UnitController_getUnit(&controller, 1);
    assert(blue->maxMovementPoints == 5 && blue->movementPoints == 5);
    assert(red->maxMovementPoints == 8 && red->movementPoints == 0);
    assert(GameTurn_getActiveTeam(&game) == TEAM_BLUE);
    assert(GameTurn_getNumber(&game) == 1);
    assert(GameTurn_getTurnsElapsed(&game) == 0);
    assert(GameTurn_canIssueCommands(&game));
    assert(GameTurn_getAliveUnitCount(&game, TEAM_BLUE) == 1);
    assert(GameTurn_getAliveUnitCount(&game, TEAM_RED) == 1);
    assert(turnStartCount == 6 && turnEndCount == 0);
    assert(lastTeam == TEAM_BLUE && lastTurn == 1);

    /* En turno azul, una unidad roja no puede seleccionarse ni recibir órdenes. */
    assert(UnitController_activate(&controller, 2, 0) == UNIT_ACTION_ENEMY_UNIT);
    assert(UnitController_getSelected(&controller) == NULL);

    assert(GameTurn_endTurn(&game));
    assert(GameTurn_getActiveTeam(&game) == TEAM_RED);
    assert(GameTurn_getNumber(&game) == 2);
    assert(GameTurn_getTurnsElapsed(&game) == 1);
    assert(blue->movementPoints == 0 && red->movementPoints == 8);
    assert(turnEndCount == 6 && turnStartCount == 12);
    assert(lastTeam == TEAM_RED && lastTurn == 2);

    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_ENEMY_UNIT);
    assert(GameTurn_endTurn(&game));
    assert(GameTurn_getActiveTeam(&game) == TEAM_BLUE);
    assert(GameTurn_getNumber(&game) == 3);
    assert(blue->movementPoints == 5 && red->movementPoints == 0);
    assert(lastTeam == TEAM_BLUE && lastTurn == 3);

    /* Camino a la meta: camino cuesta 1 y bosque cuesta 2. */
    assert(UnitController_activate(&controller, 0, 0) == UNIT_ACTION_SELECTED);
    assert(UnitController_activate(&controller, 1, 1) == UNIT_ACTION_ROUTE_READY);
    assert(UnitController_activate(&controller, 1, 1) == UNIT_ACTION_MOVING);
    for (frame = 0; frame < CELL_PIXELS / UNIT_MOVE_SPEED_PIXELS; frame++)
        assert(UnitController_update(&controller) == UNIT_ACTION_MOVING);
    assert(blue->x == 0 && blue->y == 1);
    assert(blue->movementPoints == 4); /* Camino: un punto. */

    finishMovement(&controller, &game);
    assert(blue->x == 1 && blue->y == 1);
    assert(blue->movementPoints == 0); /* La orden consume el movimiento del turno. */
    assert(!UnitController_isMoving(&controller));
    assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_BLUE_VICTORY);
    assert(GameTurn_isFinished(&game));
    assert(!GameTurn_canIssueCommands(&game));
    assert(GameTurn_getTurnsElapsed(&game) == 3);
    assert(GameTurn_getAliveUnitCount(&game, TEAM_BLUE) == 1);
    assert(GameTurn_getAliveUnitCount(&game, TEAM_RED) == 1);
    assert(strcmp(GameTurn_getOutcomeText(&game),
                  "VICTORIA AZUL / DERROTA ROJA") == 0);
    assert(strcmp(GameTurn_getOutcomeReasonText(&game),
                  "OBJETIVO CAPTURADO") == 0);
    assert(GameTurn_didCompleteObjective(&game));
    assert(!GameTurn_endTurn(&game));
    assert(turnEndCount == 18); /* Dos finales normales y fin por victoria. */
    {
        const MissionObjective occupiedObjective = {
            "OCUPADO", 1, 1, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0
        };
        assert(!GameTurn_init(&game, &map, &controller, TEAM_BLUE,
                              &occupiedObjective));
    }

    testCaptureDuringRouteFinishesMatchAtObjective();
    testHoldSurviveAndEliminateMissions();
    testMovementActionConsumedAndRestoredPerTeamTurn();
    testEmptyTeamsAndDrawResolution();
    testImmediateResultAtInitializationDoesNotEndAnUnplayedTurn();
    testSimultaneousTurnEndCasualtiesResolveAsDraw();
    testRoundLimitResolvesOnlyAfterBothTeamsAct();
    testRoundLimitAwardsSurvivorLeader();
    testTurnEndEffectsRunBeforeObjectiveResolution();
    testHoldPointAbandonmentAndControlSwitch();
    testHoldPointOccupantKilledAndCapturedByEnemy();
    testZeroRequiredTurnsMeansOneAndDispatchesOnce();
    puts("PASS: captura, mantener, sobrevivir, eliminar, limite de rondas, turnos y costes");
    return 0;
}
