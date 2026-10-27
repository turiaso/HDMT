#include <assert.h>
#include <stdio.h>
#include "scenario.h"
#include "map_data.h"
#include "unit_controller.h"
#include "game_turn.h"
#include "enemy_ai.h"
#include "combat.h"

/* Recursos generados por SGDK: el test host solo necesita los símbolos. */
const MapDefinition world_map = { 0 };
const MapDefinition bridge_map = { 0 };
const MapDefinition bridge_skirmish_map = { 0 };
const MapDefinition ruins_crossing_map = { 0 };

#define MAX_AI_FRAMES 50000u

static void assertScenarioIntegrity(const ScenarioDefinition *scenario,
                                   const UnitController *units)
{
    u8 i;
    u16 cellIndex;
    const MapData *map = scenario->logic;
    const u32 cellCount = (u32)map->width * map->height;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        const CellData *cell;
        u8 j;

        assert(unit != NULL);
        if (!unit->alive) continue;
        cell = MapData_getCellConst(map, unit->x, unit->y);
        assert(cell != NULL && cell->occupantId == (s8)unit->id);

        for (j = (u8)(i + 1u); j < UnitController_getCount(units); j++)
        {
            const Unit *other = UnitController_getUnit(units, j);
            if ((other != NULL) && other->alive)
                assert((unit->x != other->x) || (unit->y != other->y));
        }
    }

    for (cellIndex = 0; cellIndex < cellCount; cellIndex++)
    {
        const s8 occupantId = map->cells[cellIndex].occupantId;
        const u16 x = (u16)(cellIndex % map->width);
        const u16 y = (u16)(cellIndex / map->width);

        if (occupantId == CELL_NO_OCCUPANT) continue;
        assert(occupantId >= 0 && (u8)occupantId < UnitController_getCount(units));
        {
            const Unit *occupant = UnitController_getUnit(units, (u8)occupantId);
            assert(occupant != NULL && occupant->alive);
            assert(occupant->x == x && occupant->y == y);
        }
    }
}

static void runScenarioTurn(u8 scenarioIndex)
{
    const ScenarioDefinition *scenario = Scenario_get(scenarioIndex);
    UnitController units;
    GameTurn turn;
    EnemyAI ai;
    u32 frames = 0;
    u16 moves = 0;
    u16 attacks = 0;
    u16 repairs = 0;
    bool endedAiTurn = FALSE;

    assert(scenario != NULL);
    assert(MapData_init(scenario->logic));
    assert(UnitController_init(&units, scenario->logic,
        scenario->unitStorage, scenario->unitCapacity,
        scenario->unitStarts, scenario->unitCount));
    assert(GameTurn_init(&turn, scenario->logic, &units, TEAM_BLUE,
                         &scenario->objective));
    EnemyAI_init(&ai);

    assertScenarioIntegrity(scenario, &units);
    assert(GameTurn_endTurn(&turn));
    assert(GameTurn_getActiveTeam(&turn) == TEAM_RED);

    while ((GameTurn_getActiveTeam(&turn) == TEAM_RED) &&
           !GameTurn_isFinished(&turn) && (frames < MAX_AI_FRAMES))
    {
        if (UnitController_isMoving(&units))
        {
            (void)UnitController_update(&units);
            (void)GameTurn_checkObjective(&turn);
        }
        else
        {
            CombatResult combatResult;
            const EnemyAIResult result = EnemyAI_update(
                &ai, &units, scenario->logic, TEAM_RED,
                GameTurn_getNumber(&turn), &scenario->objective,
                &combatResult);

            if ((result == ENEMY_AI_MOVE_STARTED) ||
                (result == ENEMY_AI_RETREAT_STARTED))
                moves++;
            else if (result == ENEMY_AI_ATTACK_RESOLVED)
            {
                attacks++;
                (void)GameTurn_checkObjective(&turn);
            }
            else if (result == ENEMY_AI_REPAIR_RESOLVED)
                repairs++;
            else if (result == ENEMY_AI_TURN_FINISHED)
            {
                assert(GameTurn_endTurn(&turn));
                endedAiTurn = TRUE;
            }
        }
        frames++;
    }

    assert(frames < MAX_AI_FRAMES);
    assert(endedAiTurn || GameTurn_isFinished(&turn));
    assertScenarioIntegrity(scenario, &units);

    printf("PASS: IA escenario %u (%s), unidades=%u, frames=%lu, "
           "movimientos=%u, ataques=%u, reparaciones=%u\n",
           (unsigned)(scenarioIndex + 1u), scenario->name,
           (unsigned)scenario->unitCount, (unsigned long)frames,
           moves, attacks, repairs);
}

int main(void)
{
    u8 i;

    for (i = 0; i < Scenario_getCount(); i++)
        runScenarioTurn(i);

    puts("PASS: turnos de IA y coherencia espacial en los cuatro escenarios");
    return 0;
}
