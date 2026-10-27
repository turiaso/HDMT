#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "scenario.h"
#include "game_config.h"
#include "map_data.h"
#include "pathfinder.h"
#include "cell_events.h"

/* Recursos generados por SGDK: los stubs host solo necesitan simbolos. */
const MapDefinition world_map = { 0 };
const MapDefinition bridge_map = { 0 };
const MapDefinition bridge_skirmish_map = { 0 };
const MapDefinition ruins_crossing_map = { 0 };

static void validateScenario(const ScenarioDefinition *scenario, u8 expectedCount)
{
    u8 teamCounts[2] = { 0, 0 };
    u8 typeCounts[2][UNIT_TYPE_COUNT] = { { 0 } };
    u16 waterCount = 0;
    u16 rockCount = 0;
    u8 x;
    u8 y;
    u8 i;
    u8 j;
    UnitController runtimeController;

    assert(scenario != NULL);
    assert(scenario->name != NULL);
    assert(scenario->menuDescription != NULL);
    assert(scenario->menuSummary != NULL);
    /* El menu reserva 31 columnas para cada campo a la derecha del margen. */
    assert(strlen(scenario->name) <= 31u);
    assert(strlen(scenario->menuDescription) <= 31u);
    assert(strlen(scenario->menuSummary) <= 31u);
    assert(scenario->graphics != NULL);
    assert(scenario->logic != NULL);
    assert(scenario->unitStarts != NULL);
    assert(scenario->unitStorage != NULL);
    assert(scenario->unitCount == expectedCount);
    assert(scenario->unitCapacity >= scenario->unitCount);
    assert(scenario->unitCount <= GAME_MAX_UNITS);
    assert(MapData_init(scenario->logic));
    assert((scenario->logic->width == 40 && scenario->logic->height == 40) ||
           (scenario->logic->width == 20 && scenario->logic->height == 20));
    assert(scenario->logic->cellCapacity >=
           (u32)scenario->logic->width * scenario->logic->height);
    assert((MAX_UNIT_SPRITES + FIXED_GAME_SPRITE_COUNT) <= MD_VDP_SPRITE_LIMIT);
    assert(MapData_isPassable(scenario->logic, scenario->objective.x,
                              scenario->objective.y));

    for (y = 0; y < scenario->logic->height; ++y)
    {
        for (x = 0; x < scenario->logic->width; ++x)
        {
            const CellData *cell = MapData_getCellConst(scenario->logic, x, y);
            assert(cell != NULL);
            if (cell->terrain == TERRAIN_WATER) ++waterCount;
            if (cell->terrain == TERRAIN_ROCK) ++rockCount;
        }
    }

    for (i = 0; i < scenario->unitCount; ++i)
    {
        const UnitStart *unit = &scenario->unitStarts[i];
        assert(unit->id == i);
        assert((u8)unit->team < 2);
        assert((u8)unit->type < UNIT_TYPE_COUNT);
        assert(MapData_isPassable(scenario->logic, unit->x, unit->y));
        assert(!(unit->x == scenario->objective.x &&
                 unit->y == scenario->objective.y));
        ++teamCounts[unit->team];
        ++typeCounts[unit->team][unit->type];
        for (j = 0; j < i; ++j)
        {
            assert(unit->id != scenario->unitStarts[j].id);
            assert(unit->x != scenario->unitStarts[j].x ||
                   unit->y != scenario->unitStarts[j].y);
        }
    }

    assert(teamCounts[TEAM_BLUE] > 0 && teamCounts[TEAM_RED] > 0);
    for (i = 0; i < 2; ++i)
    {
        u8 t;
        u8 sum = 0;
        for (t = 0; t < UNIT_TYPE_COUNT; ++t) sum = (u8)(sum + typeCounts[i][t]);
        assert(sum == teamCounts[i]);
    }

    assert(UnitController_init(&runtimeController, scenario->logic,
        scenario->unitStorage, scenario->unitCapacity,
        scenario->unitStarts, scenario->unitCount));
    assert(UnitController_getCount(&runtimeController) == expectedCount);
    for (i = 0; i < expectedCount; ++i)
    {
        const Unit *unit = UnitController_getUnit(&runtimeController, i);
        assert(unit != NULL && unit->alive);
        assert(MapData_getCellConst(scenario->logic, unit->x, unit->y)->occupantId ==
               (s8)unit->id);
    }

    if (expectedCount == SCENARIO_UNIT_COUNT)
    {
        assert(teamCounts[TEAM_BLUE] == 60 && teamCounts[TEAM_RED] == 60);
        /* El despliegue inicial conserva dos zonas separadas alrededor del centro. */
        for (i = 0; i < scenario->unitCount; ++i)
        {
            const UnitStart *unit = &scenario->unitStarts[i];
            if (unit->team == TEAM_BLUE)
                assert(unit->x < SCENARIO_OBJECTIVE_X &&
                       unit->y < SCENARIO_OBJECTIVE_Y);
            else
                assert(unit->x > SCENARIO_OBJECTIVE_X &&
                       unit->y > SCENARIO_OBJECTIVE_Y);
        }
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_INFANTRY] == 16);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_SCOUT] == 16);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_ENGINEER] == 14);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_TANK] == 14);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_INFANTRY] == 16);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_SCOUT] == 16);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_ENGINEER] == 14);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_TANK] == 14);
        assert(scenario->objective.type == MISSION_CAPTURE_POINT);
        assert(scenario->objective.x == SCENARIO_OBJECTIVE_X);
        assert(scenario->objective.y == SCENARIO_OBJECTIVE_Y);
        /* La captura es inmediata; requiredTurns solo aplica a objetivos temporizados. */
        assert(scenario->objective.requiredTurns == 0);
        assert(scenario->objective.targetTeam == TEAM_BLUE);

        /* A diferencia de los escenarios de puente, antes no se comprobaba
           que las 120 unidades pudieran alcanzar el objetivo en el mapa grande. */
        for (i = 0; i < scenario->unitCount; ++i)
        {
            u16 steps = 0;
            u8 cost = 0;
            const UnitStart *unit = &scenario->unitStarts[i];

            assert(Pathfinder_findRouteForType(
                scenario->logic, scenario->logic->pathWorkspace,
                unit->x, unit->y,
                scenario->objective.x, scenario->objective.y,
                (s8)unit->id, unit->type, 200u, &steps, &cost));
            assert(steps > 0 && cost > 0);
        }
    }
    else if (expectedCount == BRIDGE_SCENARIO_UNIT_COUNT)
    {
        assert(scenario->logic->width == 40 && scenario->logic->height == 40);
        assert(teamCounts[TEAM_BLUE] == 12 && teamCounts[TEAM_RED] == 12);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_INFANTRY] == 4);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_SCOUT] == 3);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_ENGINEER] == 2);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_TANK] == 3);
        assert(scenario->objective.type == MISSION_HOLD_POINT);
        assert(scenario->objective.x == 20 && scenario->objective.y == 20);
        assert(scenario->objective.requiredTurns == 3);
        assert(scenario->objective.targetTeam == TEAM_BLUE);
        assert(waterCount > 200);
        assert(rockCount > 20);
        /* El centro es un puente transitable y el río corta otros accesos. */
        assert(MapData_getCellConst(scenario->logic, 19, 20)->terrain == TERRAIN_PATH);
        assert(MapData_getCellConst(scenario->logic, 20, 20)->terrain == TERRAIN_PATH);
        assert(MapData_getCellConst(scenario->logic, 19, 10)->terrain == TERRAIN_WATER);
        assert(!MapData_isPassable(scenario->logic, 19, 10));
        /* Todas las posiciones iniciales deben tener una ruta al puente. */
        for (i = 0; i < scenario->unitCount; ++i)
        {
            u16 steps = 0;
            u8 cost = 0;
            assert(Pathfinder_findRouteForType(
                scenario->logic, scenario->logic->pathWorkspace,
                scenario->unitStarts[i].x, scenario->unitStarts[i].y,
                scenario->objective.x, scenario->objective.y,
                (s8)scenario->unitStarts[i].id, scenario->unitStarts[i].type,
                200u, &steps, &cost));
            assert(steps > 0 && cost > 0);
        }
    }
    else
    {
        assert(expectedCount == BRIDGE_SKIRMISH_UNIT_COUNT);
        assert(scenario->logic->width == 20 && scenario->logic->height == 20);
        assert(teamCounts[TEAM_BLUE] == 6 && teamCounts[TEAM_RED] == 6);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_INFANTRY] == 2);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_SCOUT] == 1);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_ENGINEER] == 1);
        assert(typeCounts[TEAM_BLUE][UNIT_TYPE_TANK] == 2);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_INFANTRY] == 2);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_SCOUT] == 1);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_ENGINEER] == 1);
        assert(typeCounts[TEAM_RED][UNIT_TYPE_TANK] == 2);
        assert(scenario->objective.type == MISSION_HOLD_POINT);
        assert(scenario->objective.x == 10 && scenario->objective.y == 9);
        assert(scenario->objective.requiredTurns == 3);
        assert(scenario->objective.targetTeam == TEAM_BLUE);
        assert(MapData_getCellConst(scenario->logic, 10, 9)->terrain == TERRAIN_PATH);
        assert(waterCount > 50 && rockCount > 10);
        if (scenario->logic == &bridgeSkirmishMapData)
        {
            assert(MapData_getCellConst(scenario->logic, 9, 8)->terrain == TERRAIN_WATER);
            assert(!MapData_isPassable(scenario->logic, 9, 8));
        }
        else
        {
            /* En el cuarto escenario el río es horizontal y el cruce vertical. */
            assert(MapData_getCellConst(scenario->logic, 8, 8)->terrain == TERRAIN_WATER);
            assert(!MapData_isPassable(scenario->logic, 8, 8));
        }
        for (i = 0; i < scenario->unitCount; ++i)
        {
            u16 steps = 0;
            u8 cost = 0;
            assert(Pathfinder_findRouteForType(
                scenario->logic, scenario->logic->pathWorkspace,
                scenario->unitStarts[i].x, scenario->unitStarts[i].y,
                scenario->objective.x, scenario->objective.y,
                (s8)scenario->unitStarts[i].id, scenario->unitStarts[i].type,
                200u, &steps, &cost));
            assert(steps > 0 && cost > 0);
        }
    }
}

/* Prueba cada definición real, no solo objetivos artificiales. Confirma que
 * captura termina al ocupar el punto y que las misiones de puente cuentan
 * exactamente tres finales de turno propios, sin avanzar durante el turno rival.
 */
static void validateScenarioMissionFlow(const ScenarioDefinition *scenario,
                                        TeamId occupyingTeam)
{
    UnitController controller;
    GameTurn game;
    Unit *unit;
    const UnitStart *selectedStart = NULL;
    const CellData *originCell;
    const CellData *objectiveCell;
    u8 startIndex;
    u8 unitIndex;
    u16 originX;
    u16 originY;
    const u16 objectiveX = scenario->objective.x;
    const u16 objectiveY = scenario->objective.y;

    for (startIndex = 0; startIndex < scenario->unitCount; ++startIndex)
    {
        if (scenario->unitStarts[startIndex].team == occupyingTeam)
        {
            selectedStart = &scenario->unitStarts[startIndex];
            break;
        }
    }
    assert(selectedStart != NULL);
    originX = selectedStart->x;
    originY = selectedStart->y;

    assert(MapData_init(scenario->logic));
    assert(UnitController_init(&controller, scenario->logic,
        scenario->unitStorage, scenario->unitCapacity,
        scenario->unitStarts, scenario->unitCount));
    CellEvents_init(NULL, 0);
    assert(GameTurn_init(&game, scenario->logic, &controller,
                        TEAM_BLUE, &scenario->objective));
    assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_ACTIVE);
    assert(GameTurn_getObjective(&game)->type == scenario->objective.type);
    assert(GameTurn_getObjective(&game)->x == objectiveX);
    assert(GameTurn_getObjective(&game)->y == objectiveY);

    unitIndex = selectedStart->id;
    unit = UnitController_getMutableUnitById(&controller, unitIndex);
    assert(unit != NULL && unit->alive && unit->team == occupyingTeam);
    originCell = MapData_getCellConst(scenario->logic, originX, originY);
    objectiveCell = MapData_getCellConst(scenario->logic, objectiveX, objectiveY);
    assert(originCell != NULL && originCell->occupantId == (s8)unit->id);
    assert(objectiveCell != NULL && objectiveCell->occupantId == CELL_NO_OCCUPANT);

    /* Simula el estado lógico tras completar un movimiento válido hasta el objetivo. */
    assert(MapData_setOccupant(scenario->logic, originX, originY, CELL_NO_OCCUPANT));
    assert(MapData_setOccupant(scenario->logic, objectiveX, objectiveY, (s8)unit->id));
    unit->x = objectiveX;
    unit->y = objectiveY;
    assert(GameTurn_checkObjective(&game) ==
           (scenario->objective.type == MISSION_CAPTURE_POINT));

    if (scenario->objective.type == MISSION_CAPTURE_POINT)
    {
        const GameOutcome expected = (occupyingTeam == TEAM_BLUE)
            ? GAME_OUTCOME_BLUE_VICTORY : GAME_OUTCOME_RED_VICTORY;
        assert(GameTurn_getOutcome(&game) == expected);
        assert(GameTurn_isFinished(&game));
    }
    else
    {
        u8 turnsHeld = 0;
        u8 turnCalls = 0;
        const GameOutcome expected = (occupyingTeam == TEAM_BLUE)
            ? GAME_OUTCOME_BLUE_VICTORY : GAME_OUTCOME_RED_VICTORY;
        const u8 requiredTurns = scenario->objective.requiredTurns;

        assert(scenario->objective.type == MISSION_HOLD_POINT);
        assert(requiredTurns > 0);
        assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_ACTIVE);
        assert(GameTurn_getObjectiveHoldTeam(&game) == occupyingTeam);
        assert(GameTurn_getMissionProgress(&game) == 0);

        /* Ambos equipos alternan; solo avanza la racha al cerrar el turno del ocupante. */
        while (!GameTurn_isFinished(&game) && turnCalls < (u8)(requiredTurns * 2u + 2u))
        {
            const TeamId endingTeam = GameTurn_getActiveTeam(&game);
            const u8 progressBeforeEnd = turnsHeld;
            const u8 expectedProgress = (endingTeam == occupyingTeam)
                ? (u8)(turnsHeld + 1u) : turnsHeld;
            u8 checkIndex;

            /* Consultar el objetivo en varios fotogramas no consume turnos ni
             * incrementa progreso: solo el cierre real del turno lo hace. */
            for (checkIndex = 0; checkIndex < 4u; checkIndex++)
            {
                assert(!GameTurn_checkObjective(&game));
                assert(GameTurn_getMissionProgress(&game) == progressBeforeEnd);
                assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_ACTIVE);
            }

            assert(GameTurn_endTurn(&game));
            turnCalls++;
            if (endingTeam == occupyingTeam) turnsHeld++;
            assert(GameTurn_getMissionProgress(&game) == expectedProgress);

            if (!GameTurn_isFinished(&game))
                assert(GameTurn_getOutcome(&game) == GAME_OUTCOME_ACTIVE);
        }

        assert(turnsHeld == requiredTurns);
        assert(GameTurn_getOutcome(&game) == expected);
        assert(GameTurn_isFinished(&game));
        assert(GameTurn_getMissionProgress(&game) == requiredTurns);

        /* Una vez terminada la misión, ni cerrar otro turno ni consultar el
         * objetivo puede repetir la victoria o modificar la cuenta final. */
        assert(!GameTurn_endTurn(&game));
        assert(!GameTurn_checkObjective(&game));
        assert(GameTurn_getMissionProgress(&game) == requiredTurns);
    }
}

int main(void)
{
    const ScenarioDefinition *first;
    const ScenarioDefinition *second;

    assert(Scenario_getCount() == SCENARIO_COUNT);
    const ScenarioDefinition *third;
    const ScenarioDefinition *fourth;

    assert(Scenario_cycleIndex(0, -1) == 3);
    assert(Scenario_cycleIndex(3, 1) == 0);
    assert(Scenario_cycleIndex(1, 1) == 2);
    assert(Scenario_cycleIndex(0, 0) == 0);
    assert(Scenario_cycleIndex(99, 1) == 1);
    assert(Scenario_get(0) != NULL);
    assert(Scenario_get(1) != NULL);
    assert(Scenario_get(2) != NULL);
    assert(Scenario_get(3) != NULL);
    assert(Scenario_get(4) == NULL);
    first = Scenario_get(0);
    second = Scenario_get(1);
    third = Scenario_get(2);
    fourth = Scenario_get(3);
    assert(first != second && second != third && first != third);
    assert(third != fourth && first != fourth);
    assert(first->logic != second->logic && second->logic != third->logic);
    assert(third->logic != fourth->logic);
    /* Cada escenario define sus reglas, pero todos reutilizan buffers RAM. */
    assert(first->logic->cells == second->logic->cells);
    assert(first->logic->pathWorkspace == second->logic->pathWorkspace);
    assert(first->unitStorage == second->unitStorage);
    assert(first->graphics != second->graphics);
    assert(second->graphics != third->graphics);
    assert(third->graphics != fourth->graphics);
    assert(first->objective.type != second->objective.type);
    assert(second->unitStorage == third->unitStorage);
    assert(third->unitStorage == fourth->unitStorage);
    assert(third->unitCount == fourth->unitCount);
    assert(third->objective.type == fourth->objective.type);
    assert(third->objective.x == fourth->objective.x);
    assert(third->objective.y == fourth->objective.y);
    assert(third->objective.requiredTurns == fourth->objective.requiredTurns);
    assert(third->unitStarts == fourth->unitStarts);
    validateScenario(first, SCENARIO_UNIT_COUNT);
    validateScenario(second, BRIDGE_SCENARIO_UNIT_COUNT);
    validateScenario(third, BRIDGE_SKIRMISH_UNIT_COUNT);
    validateScenario(fourth, BRIDGE_SKIRMISH_UNIT_COUNT);
    validateScenarioMissionFlow(first, TEAM_BLUE);
    validateScenarioMissionFlow(first, TEAM_RED);
    validateScenarioMissionFlow(second, TEAM_BLUE);
    validateScenarioMissionFlow(second, TEAM_RED);
    validateScenarioMissionFlow(third, TEAM_BLUE);
    validateScenarioMissionFlow(third, TEAM_RED);
    validateScenarioMissionFlow(fourth, TEAM_BLUE);
    validateScenarioMissionFlow(fourth, TEAM_RED);

    puts("PASS: 4 escenarios; despliegues, rutas de todas las unidades, "
         "objetivos reales, progreso exacto y resultados de misión comprobados");
    return 0;
}
