#include <assert.h>
#include <stdio.h>
#include "map_data.h"
#include "scenario.h"
#include "unit_controller.h"
#include "game_turn.h"
#include "fog_of_war.h"
#include "enemy_ai.h"
#include "combat.h"

/* Host stubs for generated SGDK MAP resources. */
const MapDefinition world_map = { 0 };
const MapDefinition bridge_map = { 0 };
const MapDefinition bridge_skirmish_map = { 0 };
const MapDefinition ruins_crossing_map = { 0 };

static Unit units[SCENARIO_UNIT_COUNT];
static UnitController controller;
static GameTurn turn;
static EnemyAI ai;
static FogOfWar fog;

int main(void)
{
    MissionObjective objective =
    {
        "Integracion IA", SCENARIO_OBJECTIVE_X, SCENARIO_OBJECTIVE_Y,
        MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0
    };
    u32 frames = 0;
    u16 movesStarted = 0;
    u16 attacksResolved = 0;
    u16 aiTurnsFinished = 0;

    assert(MapData_init(&worldMapData));
    assert(UnitController_init(&controller, &worldMapData, units,
        SCENARIO_UNIT_COUNT, worldUnitStarts, SCENARIO_UNIT_COUNT));
    assert(GameTurn_init(&turn, &worldMapData, &controller, TEAM_BLUE,
                         &objective));
    assert(FogOfWar_init(&fog, &worldMapData, &controller, TEAM_BLUE));
    assert(fog.cachedUnitCount == SCENARIO_UNIT_COUNT);
    UnitController_setFogEnabled(&controller, TRUE);
    EnemyAI_init(&ai);

    /* Simula el fin de turno azul y el turno rojo completo, sin VDP,
       con 60 unidades por bando y 120 registros lógicos en total. */
    assert(GameTurn_endTurn(&turn));
    assert(GameTurn_getActiveTeam(&turn) == TEAM_RED);
    (void)FogOfWar_update(&fog, TEAM_RED);

    while ((GameTurn_getActiveTeam(&turn) == TEAM_RED) &&
           !GameTurn_isFinished(&turn) && (frames < 50000u))
    {
        if (UnitController_isMoving(&controller))
        {
            const Unit *moving = UnitController_getSelected(&controller);
            const u16 oldX = (moving != NULL) ? moving->x : 0;
            const u16 oldY = (moving != NULL) ? moving->y : 0;

            (void)UnitController_update(&controller);
            if ((moving != NULL) &&
                ((moving->x != oldX) || (moving->y != oldY)))
            {
                (void)FogOfWar_update(&fog, TEAM_RED);
                (void)GameTurn_checkObjective(&turn);
            }
        }
        else
        {
            CombatResult combatResult;
            const EnemyAIResult result = EnemyAI_update(
                &ai, &controller, &worldMapData, TEAM_RED,
                GameTurn_getNumber(&turn), &objective,
                &combatResult);

            if ((result == ENEMY_AI_MOVE_STARTED) ||
                (result == ENEMY_AI_RETREAT_STARTED)) movesStarted++;
            else if (result == ENEMY_AI_ATTACK_RESOLVED)
            {
                attacksResolved++;
                (void)FogOfWar_update(&fog, TEAM_RED);
                (void)GameTurn_checkObjective(&turn);
            }
            else if (result == ENEMY_AI_TURN_FINISHED)
            {
                aiTurnsFinished++;
                assert(GameTurn_endTurn(&turn));
            }
        }
        frames++;
    }

    assert(frames < 50000u);
    assert(movesStarted > 0);
    assert(aiTurnsFinished == 1);

    /* Auditoria de integridad tras procesar el turno completo con 120 unidades:
       ninguna unidad viva comparte casilla y la ocupacion del mapa coincide
       con los registros logicos. Esto detecta errores acumulativos de movimiento
       que una simple comprobacion de fin de turno no descubriria. */
    {
        u8 i;
        u8 aliveCount = 0;
        for (i = 0; i < SCENARIO_UNIT_COUNT; i++)
        {
            const Unit *unit = UnitController_getUnit(&controller, i);
            const CellData *cell;
            u8 j;
            assert(unit != NULL);
            if (!unit->alive) continue;
            aliveCount++;
            cell = MapData_getCellConst(&worldMapData, unit->x, unit->y);
            assert(cell != NULL);
            assert(cell->occupantId == unit->id);
            for (j = (u8)(i + 1u); j < SCENARIO_UNIT_COUNT; j++)
            {
                const Unit *other = UnitController_getUnit(&controller, j);
                if ((other != NULL) && other->alive)
                    assert((unit->x != other->x) || (unit->y != other->y));
            }
        }
        assert(aliveCount > 0);
    }

    assert(GameTurn_getActiveTeam(&turn) == TEAM_BLUE);
    assert(GameTurn_getNumber(&turn) == 3);
    assert(!GameTurn_isFinished(&turn));

    printf("PASS: turno IA completo; frames=%lu, movimientos=%u, ataques=%u\n",
           (unsigned long)frames, movesStarted, attacksResolved);
    return 0;
}
