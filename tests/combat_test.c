#include <assert.h>
#include <stdio.h>
#include "combat.h"
#include "map_data.h"
#include "pathfinder.h"
#include "unit_controller.h"
#include "cell_events.h"


static void testTerrainBlocksRangedAttack(void)
{
    static const char *const rows[] = {
        "PPPPP", "PPRPP", "PPPPP"
    };
    CellData cells[15];
    u8 costs[15], parents[15], reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_TANK, TEAM_BLUE },
        { 1, 3, 1, 5, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    CombatResult result;
    u8 defenderHealth;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    defenderHealth = units[1].health;

    /* El tanque tiene alcance dos, pero la roca ocupa la celda intermedia. */
    result = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT);
    assert(!units[0].hasAttackedThisTurn);
    assert(units[1].health == defenderHealth);

    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT);
    assert(!units[0].hasAttackedThisTurn);
    assert(units[1].health == defenderHealth);

    /* Bosque y roca bloquean de la misma forma si están entre las unidades. */
    MapData_getCell(&map, 2, 1)->terrain = TERRAIN_FOREST;
    result = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT);
    assert(!units[0].hasAttackedThisTurn);

    /* Al despejar la línea, el mismo ataque vuelve a ser legal. La propia
       casilla objetivo puede tener cobertura sin bloquear el disparo. */
    MapData_getCell(&map, 2, 1)->terrain = TERRAIN_PATH;
    MapData_getCell(&map, 3, 1)->terrain = TERRAIN_FOREST;
    result = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    assert(units[0].hasAttackedThisTurn);
    assert(units[1].health < defenderHealth);
}

static void testStatsAndExchange(void)
{
    static const char *const rows[] =
    {
        "PPPPP", "PPPPP", "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[25];
    u8 costs[25];
    u8 parents[25];
    u8 reachable[25];
    PathfinderWorkspace workspace = { costs, parents, 25, reachable };
    MapData map = { .width = 5, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 25,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 1, 1, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 1, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    CombatResult result;
    const Unit *blue;
    const Unit *red;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    blue = UnitController_getUnit(&controller, 0);
    red = UnitController_getUnit(&controller, 1);
    assert(blue != NULL && red != NULL);
    assert(blue->health == 10 && blue->maxHealth == 10);
    assert(blue->attack == 4 && blue->defense == 2 && blue->attackRange == 1);
    assert(red->health == 8 && red->attack == 2 && red->defense == 2);

    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    assert(result.damageDealt == 2 && result.counterDamage == 1);
    assert(!result.defenderDefeated && !result.attackerDefeated);
    assert(units[0].health == 9 && units[1].health == 6);
    assert(units[0].hasAttackedThisTurn);
    assert(!units[1].hasAttackedThisTurn); /* La represalia no gasta su acción. */

    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ALREADY_ACTED);
    assert(units[0].health == 9 && units[1].health == 6);

    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    assert(!units[1].hasAttackedThisTurn);
    result = Combat_attack(&controller, 1, 0, TEAM_RED);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    assert(units[0].health == 8);
    assert(units[1].health == 4);
}

static void testRangeTeamAndRemoval(void)
{
    static const char *const rows[] = { "PPPPP", "PPPPP", "PPPPP" };
    CellData cells[15];
    u8 costs[15], parents[15], reachable[15];
    PathfinderWorkspace workspace = { costs, parents, 15, reachable };
    MapData map = { .width = 5, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 15,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 3, 0, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    CombatResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_OUT_OF_RANGE);
    assert(!units[0].hasAttackedThisTurn);
    result = Combat_attack(&controller, 1, 0, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_WRONG_TEAM);
    assert(UnitController_removeUnit(&controller, 1));
    assert(!units[1].alive && units[1].health == 0);
    assert(MapData_getCell(&map, 3, 0)->occupantId == CELL_NO_OCCUPANT);
    assert(UnitController_getUnitAt(&controller, 3, 0) == NULL);
    assert(!UnitController_removeUnit(&controller, 1));
}

static void testCombatRemovesDefeatedUnits(void)
{
    static const char *const rows[] = { "PPPP", "PPPP", "PPPP" };
    CellData cells[12];
    u8 costs[12], parents[12], reachable[12];
    PathfinderWorkspace workspace = { costs, parents, 12, reachable };
    MapData map = { .width = 4, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 12,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 1, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 1, 0, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    CombatResult result;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));

    units[1].health = 1;
    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    assert(result.damageDealt == 1 && result.defenderDefeated);
    assert(!units[1].alive && units[1].health == 0);
    assert(MapData_getCellConst(&map, 2, 1)->occupantId == CELL_NO_OCCUPANT);
    assert(UnitController_getUnitAt(&controller, 2, 1) == NULL);

    /* Una unidad con 1 HP puede caer en la represalia y también libera la celda. */
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 3));
    assert(UnitController_removeUnit(&controller, 1) == FALSE);
    units[0].health = 1;
    units[1].alive = TRUE;
    units[1].health = units[1].maxHealth;
    assert(MapData_setOccupant(&map, 2, 1, 1));
    result = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(result.code == COMBAT_RESULT_ATTACKED);
    assert(result.attackerDefeated && !units[0].alive);
    assert(MapData_getCellConst(&map, 1, 1)->occupantId == CELL_NO_OCCUPANT);
}


static void testTerrainDefenseAndCombatPrediction(void)
{
    static const char *const rows[] = { "PPP", "PRP", "PPP" };
    CellData cells[9];
    u8 costs[9], parents[9], reachable[9];
    PathfinderWorkspace workspace = { costs, parents, 9, reachable };
    MapData map = { .width = 3, .height = 3, .terrainRows = rows,
        .terrainRowCount = 3, .cells = cells, .cellCapacity = 9,
        .pathWorkspace = &workspace };
    Unit units[2];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 1, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 1, 1, 5, UNIT_TYPE_ENGINEER, TEAM_RED }
    };
    CombatResult prediction;
    CombatResult attack;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 2, starts, 2));
    prediction = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(prediction.code == COMBAT_RESULT_ATTACKED);
    assert(prediction.damageDealt == 1); /* DF 2 + cobertura rocosa 2. */
    assert(prediction.counterDamage == 1);
    assert(!prediction.defenderDefeated && !prediction.attackerDefeated);
    assert(units[0].health == units[0].maxHealth);
    assert(units[1].health == units[1].maxHealth);
    assert(!units[0].hasAttackedThisTurn);

    /* Las banderas de predicción deben anticipar una muerte y una represalia
       letal sin modificar los registros reales de las unidades. */
    units[1].health = 1;
    prediction = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(prediction.code == COMBAT_RESULT_ATTACKED);
    assert(prediction.defenderDefeated && !prediction.attackerDefeated);
    units[1].health = units[1].maxHealth;
    units[0].health = 1;
    prediction = Combat_predict(&controller, 0, 1, TEAM_BLUE);
    assert(!prediction.defenderDefeated && prediction.attackerDefeated);
    units[0].health = units[0].maxHealth;

    attack = Combat_attack(&controller, 0, 1, TEAM_BLUE);
    assert(attack.damageDealt == prediction.damageDealt);
    assert(attack.counterDamage == prediction.counterDamage);
    assert(units[1].health == (u8)(units[1].maxHealth - 1));
    assert(units[0].health == (u8)(units[0].maxHealth - 1));
}


static void testAttackRangesByClassAndAttackableMask(void)
{
    static const char *const rows[] = { "PPPPP", "PPPPP", "PPPPP", "PPPPP" };
    CellData cells[20];
    u8 costs[20], parents[20], reachable[20];
    PathfinderWorkspace workspace = { costs, parents, 20, reachable };
    MapData map = { .width = 5, .height = 4, .terrainRows = rows,
        .terrainRowCount = 4, .cells = cells, .cellCapacity = 20,
        .pathWorkspace = &workspace };
    Unit units[8];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 0, 0, 6, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 1, 2, 0, 6, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 2, 0, 1, 8, UNIT_TYPE_SCOUT, TEAM_BLUE },
        { 3, 2, 1, 6, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 4, 0, 2, 5, UNIT_TYPE_ENGINEER, TEAM_BLUE },
        { 5, 2, 2, 6, UNIT_TYPE_INFANTRY, TEAM_RED },
        { 6, 0, 3, 7, UNIT_TYPE_TANK, TEAM_BLUE },
        { 7, 2, 3, 6, UNIT_TYPE_INFANTRY, TEAM_RED }
    };
    const u8 expectedRanges[UNIT_TYPE_COUNT] = { 1, 1, 1, 2 };
    u8 attackable[20];
    u8 type;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 8, starts, 8));

    /* El alcance se define una sola vez en UnitTypeInfo y se copia a la unidad. */
    for (type = 0; type < UNIT_TYPE_COUNT; type++)
    {
        const UnitTypeInfo *info = UnitType_getInfo((UnitType)type);
        assert(info != NULL);
        assert(info->attackRange == expectedRanges[type]);
    }
    for (type = 0; type < 4; type++)
    {
        const Unit *attacker = &units[(u8)(type * 2u)];
        const Unit *target = &units[(u8)(type * 2u + 1u)];
        CombatResult result = Combat_predictUnits(
            &controller, attacker, target, TEAM_BLUE);

        assert(attacker->attackRange == expectedRanges[type]);
        if (type == UNIT_TYPE_TANK)
            assert(result.code == COMBAT_RESULT_ATTACKED);
        else
            assert(result.code == COMBAT_RESULT_OUT_OF_RANGE);

        /* Todas las clases alcanzan una casilla adyacente. */
        ((Unit *)target)->x = 1;
        result = Combat_predictUnits(&controller, attacker, target, TEAM_BLUE);
        assert(result.code == COMBAT_RESULT_ATTACKED);
        ((Unit *)target)->x = 2;
    }

    /* El pintado usa exactamente la misma predicción que el ataque real:
       desde dos celdas solo el blindado puede atacar al enemigo de su fila. */
    assert(Combat_buildAttackableMask(&controller, &units[6], TEAM_BLUE,
                                     attackable, sizeof(attackable)));
    assert(attackable[3u * map.width + 2u] == 1);
    assert(attackable[0u * map.width + 2u] == 0);
    assert(attackable[1u * map.width + 2u] == 0);
    assert(attackable[2u * map.width + 2u] == 0);

    /* La máscara no revela unidades ocultas y tampoco marca tiros bloqueados. */
    UnitController_setFogEnabled(&controller, TRUE);
    cells[3u * map.width + 2u].visionState = MAP_VISION_UNSEEN;
    assert(Combat_buildAttackableMask(&controller, &units[6], TEAM_BLUE,
                                     attackable, sizeof(attackable)));
    assert(attackable[3u * map.width + 2u] == 0);

    cells[3u * map.width + 2u].visionState = MAP_VISION_VISIBLE;
    cells[3u * map.width + 1u].terrain = TERRAIN_ROCK;
    assert(Combat_buildAttackableMask(&controller, &units[6], TEAM_BLUE,
                                     attackable, sizeof(attackable)));
    assert(attackable[3u * map.width + 2u] == 0);

    cells[3u * map.width + 1u].terrain = TERRAIN_PATH;
    assert(Combat_buildAttackableMask(&controller, &units[6], TEAM_BLUE,
                                     attackable, sizeof(attackable)));
    assert(attackable[3u * map.width + 2u] == 1);

    /* Un buffer demasiado pequeño falla de forma segura, sin escribir fuera. */
    assert(!Combat_buildAttackableMask(&controller, &units[6], TEAM_BLUE,
                                      attackable, 19));
    puts("PASS: rangos por clase y máscara de blancos validada por combate real");
}


static void testEngineerRepairRulesAndTargetMask(void)
{
    static const char *const rows[] = {
        "PPPPP", "PPPPP", "PPPPP", "PPPPP", "PPPPP"
    };
    CellData cells[25];
    u8 costs[25], parents[25], reachable[25];
    u8 targetMask[25];
    PathfinderWorkspace workspace = { costs, parents, 25, reachable };
    MapData map = { .width = 5, .height = 5, .terrainRows = rows,
        .terrainRowCount = 5, .cells = cells, .cellCapacity = 25,
        .pathWorkspace = &workspace };
    Unit units[4];
    UnitController controller;
    const UnitStart starts[] = {
        { 0, 1, 1, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
        { 1, 2, 1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
        { 2, 1, 2, 0, UNIT_TYPE_SCOUT, TEAM_RED },
        { 3, 4, 4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE }
    };
    RepairResult repair;
    CombatResult attack;

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, 4, starts, 4));
    assert(units[0].repairRange == 1 && units[0].repairAmount == 3);
    assert(units[1].repairAmount == 0);
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[0], TEAM_BLUE).code ==
           REPAIR_RESULT_INVALID);
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[2], TEAM_BLUE).code ==
           REPAIR_RESULT_NOT_ALLY);

    units[1].health = 1;
    repair = Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE);
    assert(repair.code == REPAIR_RESULT_CAN_REPAIR);
    assert(repair.healthRestored == 3); /* Predicción informa de la recuperación sin mutar. */
    assert(units[1].health == 1 && !units[0].hasRepairedThisTurn);

    assert(Combat_buildActionTargetMask(&controller, &units[0], TEAM_BLUE,
                                        targetMask, sizeof(targetMask)));
    assert(targetMask[1u * map.width + 2u] == COMBAT_TARGET_REPAIRABLE);
    assert(targetMask[2u * map.width + 1u] == COMBAT_TARGET_ATTACKABLE);

    repair = Combat_repair(&controller, 0, 1, TEAM_BLUE);
    assert(repair.code == REPAIR_RESULT_REPAIRED && repair.healthRestored == 3);
    assert(units[1].health == 4 && units[0].hasRepairedThisTurn);
    attack = Combat_predict(&controller, 0, 2, TEAM_BLUE);
    assert(attack.code == COMBAT_RESULT_ALREADY_ACTED);

    assert(Combat_buildActionTargetMask(&controller, &units[0], TEAM_BLUE,
                                        targetMask, sizeof(targetMask)));
    assert(targetMask[1u * map.width + 2u] == COMBAT_TARGET_NONE);
    assert(targetMask[2u * map.width + 1u] == COMBAT_TARGET_NONE);

    /* La reparacion no excede la vida maxima y nunca puede hacerse a distancia. */
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 2));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 3));
    assert(!units[0].hasRepairedThisTurn && !units[0].hasAttackedThisTurn);
    units[1].health = (u8)(units[1].maxHealth - 2u);
    repair = Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE);
    assert(repair.code == REPAIR_RESULT_CAN_REPAIR && repair.healthRestored == 2);
    assert(units[1].health == (u8)(units[1].maxHealth - 2u));
    repair = Combat_repair(&controller, 0, 1, TEAM_BLUE);
    assert(repair.code == REPAIR_RESULT_REPAIRED && repair.healthRestored == 2);
    assert(units[1].health == units[1].maxHealth);
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE).code ==
           REPAIR_RESULT_ALREADY_ACTED);

    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 4));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 5));
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE).code ==
           REPAIR_RESULT_TARGET_FULL_HEALTH);
    units[1].health = 1;
    units[1].x = 3;
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE).code ==
           REPAIR_RESULT_OUT_OF_RANGE);
    assert(Combat_predictRepairUnits(&controller, &units[3], &units[1], TEAM_BLUE).code ==
           REPAIR_RESULT_NOT_ENGINEER);

    /* Atacar consume la misma acción e impide reparar después en ese turno. */
    assert(UnitController_beginTeamTurn(&controller, TEAM_RED, 6));
    assert(UnitController_beginTeamTurn(&controller, TEAM_BLUE, 7));
    units[1].x = 2;
    units[1].y = 1;
    units[1].health = 1;
    repair = Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE);
    assert(repair.code == REPAIR_RESULT_CAN_REPAIR);
    attack = Combat_attack(&controller, 0, 2, TEAM_BLUE);
    assert(attack.code == COMBAT_RESULT_ATTACKED);
    assert(Combat_predictRepairUnits(&controller, &units[0], &units[1], TEAM_BLUE).code ==
           REPAIR_RESULT_ALREADY_ACTED);
}

int main(void)
{
    testStatsAndExchange();
    testRangeTeamAndRemoval();
    testCombatRemovesDefeatedUnits();
    testTerrainDefenseAndCombatPrediction();
    testTerrainBlocksRangedAttack();
    testAttackRangesByClassAndAttackableMask();
    testEngineerRepairRulesAndTargetMask();
    puts("PASS: atributos, alcance, LOS, predicción, reparación, cobertura, daño y eliminación");
    return 0;
}
