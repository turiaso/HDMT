#include <assert.h>
#include <stdio.h>
#include "cell_events.h"
#include "enemy_ai.h"
#include "game_config.h"

static void testRedAIApproachesAndAttacks(void)
{
    static const char *const rows[] =
    {
        "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[15];
    u8 costs[15], parents[15], reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 4, 1, 8, UNIT_TYPE_SCOUT, TEAM_RED }
    };
    EnemyAIResult result;
    MissionObjective objective = { "Eliminar", 2, 1, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    u16 frame;
    u16 decisionFrame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    /* La IA solo puede perseguir objetivos que conoce: revela al azul para Rojo. */
    MapData_getCell(&map, 0, 1)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);
    assert(EnemyAI_getNextActorIndex(&ai, &controller, 2) == 1);
    assert(EnemyAI_getFocusUnitIndex(&ai) == -1);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_FLANK);
    assert(EnemyAI_getFocusUnitIndex(&ai) == 1);
    assert(EnemyAI_getFocusTargetIndex(&ai) == 0);
    assert(EnemyAI_getNextActorIndex(&ai, &controller, 2) == 1);
    assert(UnitController_isMoving(&controller));
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert(units[1].x < 4);
    assert(EnemyAI_getNextActorIndex(&ai, &controller, 2) == 1);

    /* Un explorador puede flanquear y gastar el movimiento restante antes de
       atacar; deja que el controlador resuelva todos esos segmentos. */
    for (decisionFrame = 0; decisionFrame < 8; decisionFrame++)
    {
        result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                                &objective, &combatResult);
        if (result == ENEMY_AI_ATTACK_RESOLVED) break;
        if ((result == ENEMY_AI_MOVE_STARTED) ||
            (result == ENEMY_AI_RETREAT_STARTED))
        {
            for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
                (void)UnitController_update(&controller);
            assert(frame < 100);
        }
        else if (result == ENEMY_AI_UNIT_FINISHED) break;
    }
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(EnemyAI_getFocusUnitIndex(&ai) == 1);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    assert(units[0].health < units[0].maxHealth);
    assert(units[1].hasAttackedThisTurn);
    assert(EnemyAI_getNextActorIndex(&ai, &controller, 2) == -1);

    puts("PASS: IA roja se acerca a un objetivo visible y ataca sin bloquear el bucle");
}

static void testCaptureMissionPrioritizesObjective(void)
{
    static const char *const rows[] =
    {
        "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 4, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 5, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED }
    };
    MissionObjective objective =
        { "Capturar", 1, 2, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 4, 0)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    assert(EnemyAI_getTacticalPlan(&ai) == AI_PLAN_OBJECTIVE_ASSAULT);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert(units[1].x == objective.x && units[1].y == objective.y);
    puts("PASS: la IA de captura prioriza el objetivo frente a un enemigo lejano");
}

static void testHoldPointDefendsAndUsesCover(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPPPPP", "PPPRPPPPPPP", "PPPPPPPPPPP",
        "PPPPPPPPPPP", "PPPPPPPPPPP"
    };
    CellData cells[55];
    u8 costs[55], parents[55], reachable[55];
    PathfinderWorkspace workspace = { costs, parents, 55, reachable };
    MapData map = { .width = 11, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 55,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 9, 2, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Mantener", 3, 2, MISSION_HOLD_POINT, 3, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 9, 2)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_GUARD);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    /* El enemigo está lejos del punto: la IA no abandona la zona defendida. */
    /* La casilla rocosa junto al objetivo aporta defensa y se prefiere. */
    assert((units[1].x == 3) && (units[1].y == 1));
    puts("PASS: la IA mantiene la zona del objetivo y busca cobertura rocosa");
}

static void testAIChoosesHighValueAttack(void)
{
    static const char *const rows[] =
    {
        "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 3, 2, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 2, 2, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    units[1].health = 3;
    MapData_getCell(&map, 3, 2)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    MapData_getCell(&map, 2, 4)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    assert(combatResult.defenderDefeated);
    assert(!units[1].alive);
    assert(units[0].alive && units[0].health == units[0].maxHealth);
    puts("PASS: la IA prioriza una eliminacion segura sobre un ataque menos rentable");
}

static void testObjectiveRunnerAndEscortRoles(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP",
        "PPPPPPPPPP", "PPPPPPPPPP"
    };
    CellData cells[50];
    u8 costs[50], parents[50], reachable[50];
    PathfinderWorkspace workspace = { costs, parents, 50, reachable };
    MapData map = { .width = 10, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 50,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 8, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 7, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED },
        { 3, 8, 4, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Capturar", 2, 2, MISSION_CAPTURE_POINT, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert((result == ENEMY_AI_MOVE_STARTED) ||
           (result == ENEMY_AI_UNIT_FINISHED));
    assert(EnemyAI_getTacticalPlan(&ai) == AI_PLAN_OBJECTIVE_ASSAULT);
    assert(EnemyAI_getTacticalRole(&ai, 2) == AI_ROLE_OBJECTIVE_RUNNER);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_ESCORT);
    assert(EnemyAI_getTacticalRole(&ai, 3) == AI_ROLE_SUPPORT);
    puts("PASS: corredor de objetivo, escolta y apoyo se asignan de forma coordinada");
}

static void testObjectiveRunnerAccountsForUnitMobility(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPPPPPP", "PPPPPPPPPPPP", "PPPPPPPPPPPP",
        "PPPPPPPPPPPP", "PPPPPPPPPPPP"
    };
    CellData cells[60];
    u8 costs[60], parents[60], reachable[60];
    PathfinderWorkspace workspace = { costs, parents, 60, reachable };
    MapData map = { .width = 12, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 60,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        /* La infanteria esta a 5 casillas, el explorador a 6; aun asi el
           explorador tiene mejor velocidad relativa de aproximacion (8 vs 6). */
        { 1, 7, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 8, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED }
    };
    MissionObjective objective =
        { "Capturar", 2, 2, MISSION_CAPTURE_POINT, 0, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert((result == ENEMY_AI_MOVE_STARTED) ||
           (result == ENEMY_AI_UNIT_FINISHED));
    assert(EnemyAI_getTacticalRole(&ai, 2) == AI_ROLE_OBJECTIVE_RUNNER);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_ESCORT);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert(!UnitController_isMoving(&controller));
    puts("PASS: el corredor se elige por movilidad relativa y no solo por cercania");
}

static void testLowHealthUnitRetreatsFromLethalThreat(void)
{
    static const char *const rows[] =
    {
        "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 4, 2, 0, UNIT_TYPE_TANK, TEAM_BLUE },
        { 1, 3, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    units[1].health = 2;
    MapData_getCell(&map, 4, 2)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_RETREAT_STARTED);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert(units[1].alive);
    assert((units[1].x != 3) || (units[1].y != 2));
    assert((units[1].x > 4 ? units[1].x - 4 : 4 - units[1].x) +
           (units[1].y > 2 ? units[1].y - 2 : 2 - units[1].y) > 1);
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_UNIT_FINISHED);
    assert(units[0].health == units[0].maxHealth);
    assert(units[1].alive && (units[1].health == 2));
    puts("PASS: una unidad crítica se retira de una amenaza letal sin atacar después");
}


static void testCoordinatedFocusFire(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP",
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[56];
    u8 costs[56], parents[56], reachable[56];
    PathfinderWorkspace workspace = { costs, parents, 56, reachable };
    MapData map = { .width = 8, .height = 7, .terrainRows = rows,
        .terrainRowCount = 7, .cells = cells, .cellCapacity = 56,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 3, 2, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
        { 1, 4, 3, 0, UNIT_TYPE_TANK, TEAM_BLUE },
        { 2, 2, 3, 0, UNIT_TYPE_TANK, TEAM_RED },
        { 3, 3, 3, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 7, 6, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 3, 2)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    MapData_getCell(&map, 4, 3)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    /* El tanque rojo valora el tanque azul por su amenaza y su resistencia.
       El blanco sobrevive al primer ataque. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    assert(!combatResult.defenderDefeated);
    assert(units[1].health == 11);
    assert(units[0].health == units[0].maxHealth);
    assert(EnemyAI_getFocusTargetIndex(&ai) == 1);

    /* La infantería roja prefiere normalmente al ingeniero azul cercano,
       pero el foco la dirige al tanque dañado para concentrar los ataques. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    assert(units[1].health == 10);
    assert(units[0].health == units[0].maxHealth);
    assert(EnemyAI_getFocusTargetIndex(&ai) == 1);
    puts("PASS: las unidades concentran fuego en el blanco prioritario que ya está dañado");
}

static void testRolesReassignWhenObjectiveChangesHands(void)
{
    static const char *const rows[] =
    {
        "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 4, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED },
        { 2, 6, 4, 0, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    MissionObjective objective =
        { "Mantener", 2, 2, MISSION_HOLD_POINT, 3, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_OBJECTIVE_RUNNER);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert((units[1].x == objective.x) && (units[1].y == objective.y));

    /* La propiedad del punto cambia de neutral a rojo durante el mismo turno.
       La siguiente decisión debe convertir al corredor en defensor. */
    (void)EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                        &objective, &combatResult);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_GUARD);
    assert(EnemyAI_getTacticalPlan(&ai) == AI_PLAN_OBJECTIVE_DEFENSE);
    assert(ai.lastObjectiveOwner == (u8)TEAM_RED);
    puts("PASS: la IA reasigna roles inmediatamente cuando captura el objetivo");
}

static void testSupportTakesSeparatedFiringPosition(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP",
        "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP"
    };
    CellData cells[70];
    u8 costs[70], parents[70], reachable[70];
    PathfinderWorkspace workspace = { costs, parents, 70, reachable };
    MapData map = { .width = 10, .height = 7, .terrainRows = rows,
        .terrainRowCount = 7, .cells = cells, .cellCapacity = 70,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 3, 3, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 8, 3, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 6, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 3, 3)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    assert(EnemyAI_getTacticalRole(&ai, 1) == AI_ROLE_SUPPORT);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);

    /* El apoyo entra en alcance y ocupa el flanco lateral designado por ID,
       en vez de apilarse directamente en la fila del blanco. */
    assert(units[1].x == 4);
    assert(units[1].y == 2);
    assert((units[1].x != units[0].x) && (units[1].y != units[0].y));
    assert((u16)((units[1].x > units[0].x) ? units[1].x - units[0].x :
                                               units[0].x - units[1].x) +
           (u16)((units[1].y > units[0].y) ? units[1].y - units[0].y :
                                               units[0].y - units[1].y) == 2u);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    puts("PASS: el apoyo ocupa una posición de tiro lateral separada y ataca a alcance útil");
}

static void testAIRepositionsToOpenBlockedShot(void)
{
    static const char *const rows[] = { "PPPPP", "PPRPP", "PPPPP" };
    CellData cells[15];
    u8 costs[15], parents[15], reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] = {
        { 0, 3, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 1, 1, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 2, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    /* Rango de prueba 2: el blanco está a alcance, pero la roca tapa el tiro. */
    units[1].attackRange = 2;
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 3, 1)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    assert(!MapData_hasLineOfSight(&map, units[1].x, units[1].y,
                                   units[0].x, units[0].y));

    EnemyAI_init(&ai);
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED); /* No atacó: eligió recolocarse. */
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert(MapData_hasLineOfSight(&map, units[1].x, units[1].y,
                                  units[0].x, units[0].y));

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    puts("PASS: la IA rodea un obstaculo para abrir linea de fuego antes de atacar");
}


static void testEngineerRepairsMostDamagedAdjacentAlly(void)
{
    static const char *const rows[] =
    {
        "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[15];
    u8 costs[15], parents[15], reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] = {
        { 0, 0, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 1, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
        { 2, 3, 1, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 2, 1, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    units[2].health = 2;
    units[0].health = units[0].maxHealth;
    /* Azul está lejos para que el ingeniero no tenga un ataque válido. */
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_REPAIR_RESOLVED);
    assert(units[2].health == 5);
    assert(units[1].hasRepairedThisTurn);
    assert(!units[1].hasAttackedThisTurn);

    puts("PASS: IA ingeniera repara al aliado adyacente más dañado");
}


static void testAIUsesItsOwnAttackRange(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[24];
    u8 costs[24], parents[24], reachable[24];
    PathfinderWorkspace workspace = { costs, parents, 24, reachable };
    MapData map = { .width = 8, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 24,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 7, 1, 0, UNIT_TYPE_TANK, TEAM_BLUE },
        { 1, 0, 1, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;
    u16 dx;
    u16 dy;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    MapData_getCell(&map, 7, 1)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);

    dx = (units[1].x > units[0].x)
        ? (u16)(units[1].x - units[0].x) : (u16)(units[0].x - units[1].x);
    dy = (units[1].y > units[0].y)
        ? (u16)(units[1].y - units[0].y) : (u16)(units[0].y - units[1].y);
    /* Infanteria (alcance 1) debe cerrar hasta su propio alcance, no el del blindado (2). */
    assert((u16)(dx + dy) == 1);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_ATTACK_RESOLVED);
    assert(combatResult.code == COMBAT_RESULT_ATTACKED);
    puts("PASS: la IA usa el alcance de ataque de su propia clase al aproximarse");
}

static void testEngineerMovesTowardDamagedAlly(void)
{
    static const char *const rows[] =
    {
        "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP", "PPPPPPP"
    };
    CellData cells[35];
    u8 costs[35], parents[35], reachable[35];
    PathfinderWorkspace workspace = { costs, parents, 35, reachable };
    MapData map = { .width = 7, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 35,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 6, 4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 0, 2, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
        { 2, 5, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 0, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    units[2].health = 2;
    EnemyAI_init(&ai);

    /* Sin enemigos visibles, el ingeniero debe acercarse al aliado herido. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);
    assert((units[1].x == 4) && (units[1].y == 2));
    assert(units[2].health == 2);

    /* Al terminar adyacente, la siguiente decision ejecuta la reparacion real. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_REPAIR_RESOLVED);
    assert(units[2].health == 5);
    assert(units[1].hasRepairedThisTurn);

    puts("PASS: la IA ingeniera se aproxima a un aliado herido y luego lo repara");
}


static void testTankPrefersRoadWhenProgressIsEquivalent(void)
{
    static const char *const rows[] =
    {
        "PPWWWW", "PPRWWW", "PPWWPP", "PPPWPP", "PPWWPP"
    };
    CellData cells[30];
    u8 costs[30], parents[30], reachable[30];
    PathfinderWorkspace workspace = { costs, parents, 30, reachable };
    MapData map = { .width = 6, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 30,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 5, 4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 3, 1, 2, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 4, 2, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    /* Azul no es visible: el blindado elige una posicion de aproximacion. */
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
        (void)UnitController_update(&controller);
    assert(frame < 100);

    /* El paso inferior de camino y el superior de roca avanzan igual; el
       blindado debe elegir camino, evitando pagar el coste tactico de la roca. */
    assert((units[1].x == 2) && (units[1].y == 3));
    assert(map.cells[(u32)units[1].y * map.width + units[1].x].terrain ==
           TERRAIN_PATH);
    puts("PASS: el blindado prioriza camino frente a roca cuando el progreso empata");
}


static void testAIDoesNotTargetHiddenEnemy(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[24];
    u8 costs[24], parents[24], reachable[24];
    PathfinderWorkspace workspace = { costs, parents, 24, reachable };
    MapData map = { .width = 8, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 24,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 2, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 1, 1, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 7, 1, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    u8 blueHealth;
    u16 frame;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    EnemyAI_init(&ai);
    blueHealth = units[0].health;
    /* Simula un foco heredado de una decisión anterior. Si el enemigo deja de
       ser visible, la IA debe invalidar ese índice antes de puntuar objetivos. */
    ai.focusTargetIndex = 0;

    /* El enemigo está adyacente, pero su celda no es visible para Rojo. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 2,
                            &objective, &combatResult);
    assert(result != ENEMY_AI_ATTACK_RESOLVED);
    assert(EnemyAI_getFocusTargetIndex(&ai) == -1);
    assert(units[0].health == blueHealth);

    if (result == ENEMY_AI_MOVE_STARTED)
    {
        for (frame = 0; UnitController_isMoving(&controller) && frame < 100; frame++)
            (void)UnitController_update(&controller);
        assert(frame < 100);
        assert(!((units[1].x == units[0].x) && (units[1].y == units[0].y)));
    }

    puts("PASS: la IA no selecciona ni ataca enemigos ocultos por la niebla");
}

static void testAIRejectsMismatchedTurnContext(void)
{
    static const char *const rows[] = { "PPPPP", "PPPPP", "PPPPP" };
    CellData cells[15];
    CellData otherCells[15];
    u8 costs[15], parents[15], reachable[15];
    u8 otherCosts[15], otherParents[15], otherReachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    PathfinderWorkspace otherWorkspace =
        { otherCosts, otherParents, 15, otherReachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    MapData otherMap = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = otherCells, .cellCapacity = 15,
        .pathWorkspace = &otherWorkspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 4, 1, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 2, 1, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;
    const u8 blueHealth = 10;
    const u8 redHealth = 10;

    assert(MapData_init(&map));
    assert(MapData_init(&otherMap));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 1));
    EnemyAI_init(&ai);

    /* Una llamada para el bando que no tiene turno no debe avanzar la IA. */
    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_IDLE);
    assert(!ai.initialized && (ai.turnNumber == 0));
    assert(ai.actorIndex == -1);
    assert(UnitController_getActiveTeam(&controller) == TEAM_BLUE);

    /* Tampoco se mezclan las coordenadas de otro mapa con el controlador actual. */
    result = EnemyAI_update(&ai, &controller, &otherMap, TEAM_BLUE, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_IDLE);
    assert(!ai.initialized && (ai.turnNumber == 0));
    assert(!UnitController_isMoving(&controller));
    assert(units[0].alive && units[1].alive);
    assert(units[0].health == blueHealth && units[1].health == redHealth);
    assert((units[0].x == 0) && (units[0].y == 1));
    assert((units[1].x == 4) && (units[1].y == 1));

    puts("PASS: la IA rechaza turnos/mapas incoherentes sin consumir estado ni acciones");
}

/* Ejecuta una accion completa de IA, incluidos todos los fotogramas de movimiento. */
static void finishAIMovementForTest(UnitController *controller)
{
    u16 frame = 0;
    while (UnitController_isMoving(controller) && frame < 100u)
    {
        (void)UnitController_update(controller);
        frame++;
    }
    assert(frame < 100u);
}

/* Resuelve un turno de IA host completo; falla si queda bloqueado en un bucle. */
static void runCompleteAITurnForTest(EnemyAI *ai, UnitController *controller,
                                     MapData *map, TeamId team, u16 turnNumber,
                                     const MissionObjective *objective)
{
    u16 decisions;
    for (decisions = 0; decisions < 64u; decisions++)
    {
        EnemyAIResult result;
        CombatResult combatResult;

        if (UnitController_isMoving(controller))
        {
            finishAIMovementForTest(controller);
            continue;
        }

        result = EnemyAI_update(ai, controller, map, team, turnNumber,
                                objective, &combatResult);
        if (result == ENEMY_AI_TURN_FINISHED) return;
        assert(result != ENEMY_AI_IDLE);
        if ((result == ENEMY_AI_MOVE_STARTED) ||
            (result == ENEMY_AI_RETREAT_STARTED))
            finishAIMovementForTest(controller);
    }
    assert(!"La IA no terminó el turno dentro del límite de decisiones");
}

static u16 testDistance(const Unit *a, const Unit *b)
{
    const u16 dx = (a->x > b->x) ? (u16)(a->x - b->x) : (u16)(b->x - a->x);
    const u16 dy = (a->y > b->y) ? (u16)(a->y - b->y) : (u16)(b->y - a->y);
    return (u16)(dx + dy);
}

static void assertNoUnitOverlapAndWater(const UnitController *controller,
                                        const MapData *map)
{
    const u8 count = UnitController_getCount(controller);
    u8 i;
    for (i = 0; i < count; i++)
    {
        const Unit *unit = UnitController_getUnit(controller, i);
        const CellData *cell;
        u8 j;
        assert(unit != NULL);
        if (!unit->alive) continue;
        cell = MapData_getCellConst(map, unit->x, unit->y);
        assert(cell != NULL);
        assert(cell->terrain != TERRAIN_WATER);
        assert(cell->occupantId == unit->id);
        for (j = (u8)(i + 1u); j < count; j++)
        {
            const Unit *other = UnitController_getUnit(controller, j);
            if (other != NULL && other->alive)
                assert((unit->x != other->x) || (unit->y != other->y));
        }
    }
}

static void testInfantryUsesCoverWhenProgressIsEquivalent(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPP", "PPPRPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[40];
    u8 costs[40], parents[40], reachable[40];
    PathfinderWorkspace workspace = { costs, parents, 40, reachable };
    MapData map = { .width = 8, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 40,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 2, 2, 0, UNIT_TYPE_TANK, TEAM_BLUE },
        { 1, 7, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 0, 4, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    /* En esta prueba se fija alcance 3 para comparar dos posiciones desde las
       que ambas pueden disparar: una abierta y otra rocosa con mayor defensa. */
    units[1].attackRange = 3;
    MapData_getCell(&map, 2, 2)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    finishAIMovementForTest(&controller);

    /* La roca (defensa +2) queda dentro del alcance y tiene tiro despejado;
       la infantería no debe preferir una casilla abierta tácticamente idéntica. */
    assert((units[1].x == 3u) && (units[1].y == 1u));
    assert(map.cells[(u32)units[1].y * map.width + units[1].x].terrain ==
           TERRAIN_ROCK);
    assert(MapData_hasLineOfSight(&map, units[1].x, units[1].y,
                                  units[0].x, units[0].y));
    puts("PASS: la infantería aprovecha cobertura rocosa con progreso y tiro equivalentes");
}

static void testTwoTanksChooseSeparatedFiringPositions(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP",
        "PPPPPPPPPP", "PPPPPPPPPP", "PPPPPPPPPP"
    };
    CellData cells[70];
    u8 costs[70], parents[70], reachable[70];
    PathfinderWorkspace workspace = { costs, parents, 70, reachable };
    MapData map = { .width = 10, .height = 7, .terrainRows = rows,
        .terrainRowCount = 7, .cells = cells, .cellCapacity = 70,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    const UnitStart starts[] =
    {
        { 0, 2, 3, 0, UNIT_TYPE_TANK, TEAM_BLUE },
        { 1, 8, 1, 0, UNIT_TYPE_TANK, TEAM_RED },
        { 2, 8, 5, 0, UNIT_TYPE_TANK, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 9, 6, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    MapData_getCell(&map, 2, 3)->visionState |=
        (u8)(MAP_VISION_VISIBLE << (TEAM_RED * 2u));
    EnemyAI_init(&ai);

    runCompleteAITurnForTest(&ai, &controller, &map, TEAM_RED, 1, &objective);

    assert(units[1].hasAttackedThisTurn);
    assert(units[2].hasAttackedThisTurn);
    assert(testDistance(&units[1], &units[2]) >= 2u);
    assertNoUnitOverlapAndWater(&controller, &map);
    puts("PASS: dos blindados obtienen posiciones de tiro distintas sin amontonarse");
}

static void testAILeavesFriendlyApproachLaneOpen(void)
{
    static const char *const rows[] =
    {
        "PPPPPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP", "PPPPPPPP"
    };
    CellData cells[40];
    u8 costs[40], parents[40], reachable[40];
    PathfinderWorkspace workspace = { costs, parents, 40, reachable };
    MapData map = { .width = 8, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 40,
        .pathWorkspace = &workspace };
    Unit units[3];
    UnitController controller;
    EnemyAI ai;
    CombatResult combatResult;
    const UnitStart starts[] =
    {
        { 0, 0, 0, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        /* Esta unidad actúa primero pero está detrás de su compañera. */
        { 1, 6, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 4, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 1, 2, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    EnemyAIResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 3, starts, 3));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    EnemyAI_init(&ai);

    result = EnemyAI_update(&ai, &controller, &map, TEAM_RED, 1,
                            &objective, &combatResult);
    assert(result == ENEMY_AI_MOVE_STARTED);
    finishAIMovementForTest(&controller);

    /* La casilla (3,2) es el siguiente paso directo delante de la compañera.
       En campo abierto existe una ruta alternativa con el mismo progreso; la
       IA debe rodear la unidad y dejar libre su corredor hacia el objetivo. */
    assert(!((units[1].x == 3u) && (units[1].y == 2u)));
    {
        const u16 dx = (units[1].x > objective.x)
            ? (u16)(units[1].x - objective.x)
            : (u16)(objective.x - units[1].x);
        const u16 dy = (units[1].y > objective.y)
            ? (u16)(units[1].y - objective.y)
            : (u16)(objective.y - units[1].y);
        assert((u16)(dx + dy) < 5u);
    }
    assertNoUnitOverlapAndWater(&controller, &map);
    puts("PASS: en campo abierto la IA rodea a una aliada y deja libre su corredor");
}

static void testNarrowBridgeQueuesAndReleasesUnitsSafely(void)
{
    static const char *const rows[] =
    {
        "PPPPWPPPP", "PPPPWPPPP", "PPPPPPPPP",
        "PPPPWPPPP", "PPPPWPPPP"
    };
    CellData cells[45];
    u8 costs[45], parents[45], reachable[45];
    PathfinderWorkspace workspace = { costs, parents, 45, reachable };
    MapData map = { .width = 9, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 45,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    EnemyAI ai;
    const UnitStart starts[] =
    {
        { 0, 1, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 5, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 6, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 3, 7, 2, 0, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    MissionObjective objective =
        { "Eliminar", 1, 2, MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 1));
    /* Presupuesto reducido para crear una cola reproducible en el puente de una celda. */
    units[1].maxMovementPoints = units[1].movementPoints = 2;
    units[2].maxMovementPoints = units[2].movementPoints = 2;
    units[3].maxMovementPoints = units[3].movementPoints = 2;
    EnemyAI_init(&ai);

    runCompleteAITurnForTest(&ai, &controller, &map, TEAM_RED, 1, &objective);
    assert((units[1].x == 3u) && (units[1].y == 2u));
    assert((units[2].x == 4u) && (units[2].y == 2u));
    assert(units[3].x >= 5u); /* Espera en la orilla; no entra en agua ni atraviesa la unidad. */
    assertNoUnitOverlapAndWater(&controller, &map);

    /* Al comenzar un nuevo turno, la primera unidad libera el paso y el convoy
       avanza en orden. La última llega al puente solo cuando vuelve a estar libre. */
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    runCompleteAITurnForTest(&ai, &controller, &map, TEAM_RED, 2, &objective);
    assert((units[1].x <= 2u) && (units[1].y == 2u));
    assert((units[2].x <= 3u) && (units[2].y == 2u));
    assert((units[3].x <= 3u) && (units[3].y == 2u));
    assertNoUnitOverlapAndWater(&controller, &map);
    puts("PASS: en un puente estrecho la IA forma cola y cruza al liberar la casilla ocupada");
}

int main(void)
{
    testRedAIApproachesAndAttacks();
    testEngineerRepairsMostDamagedAdjacentAlly();
    testCaptureMissionPrioritizesObjective();
    testAIChoosesHighValueAttack();
    testHoldPointDefendsAndUsesCover();
    testObjectiveRunnerAndEscortRoles();
    testObjectiveRunnerAccountsForUnitMobility();
    testLowHealthUnitRetreatsFromLethalThreat();
    testCoordinatedFocusFire();
    testRolesReassignWhenObjectiveChangesHands();
    testSupportTakesSeparatedFiringPosition();
    testAIRepositionsToOpenBlockedShot();
    testAIUsesItsOwnAttackRange();
    testEngineerMovesTowardDamagedAlly();
    testTankPrefersRoadWhenProgressIsEquivalent();
    testAIDoesNotTargetHiddenEnemy();
    testAIRejectsMismatchedTurnContext();
    testInfantryUsesCoverWhenProgressIsEquivalent();
    testTwoTanksChooseSeparatedFiringPositions();
    testAILeavesFriendlyApproachLaneOpen();
    testNarrowBridgeQueuesAndReleasesUnitsSafely();
    return 0;
}
