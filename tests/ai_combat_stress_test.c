#include <assert.h>
#include <stdio.h>
#include "map_data.h"
#include "unit_controller.h"
#include "enemy_ai.h"
#include "combat.h"
#include "cell_events.h"

#define STRESS_UNIT_COUNT 120u
#define STRESS_MAP_SIZE 1600u

static const char *const rows[40] = {
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
};
static CellData cells[STRESS_MAP_SIZE];
static u8 costs[STRESS_MAP_SIZE], parents[STRESS_MAP_SIZE], reachable[STRESS_MAP_SIZE];
static PathfinderWorkspace workspace = { costs, parents, STRESS_MAP_SIZE, reachable };
static MapData map = { .width = 40, .height = 40, .terrainRows = rows,
    .terrainRowCount = 40, .cells = cells, .cellCapacity = STRESS_MAP_SIZE,
    .pathWorkspace = &workspace };
static Unit units[STRESS_UNIT_COUNT];
static UnitController controller;
static UnitStart starts[STRESS_UNIT_COUNT];
static EnemyAI aiRed;
static EnemyAI aiBlue;

/* Comprueba la coherencia entre el registro lógico de unidades y la ocupación
   del mapa en cada punto estable de la simulación, no solo al final del turno. */
static void assertSpatialIntegrity(void)
{
    u16 i;
    u16 cellIndex;

    for (i = 0; i < STRESS_UNIT_COUNT; i++)
    {
        const Unit *unit = UnitController_getUnit(&controller, (u8)i);
        u16 j;

        assert(unit != NULL);
        if (!unit->alive) continue;

        {
            const CellData *cell = MapData_getCellConst(&map, unit->x, unit->y);
            assert(cell != NULL);
            assert(cell->occupantId == (s8)unit->id);
        }

        for (j = (u16)(i + 1u); j < STRESS_UNIT_COUNT; j++)
        {
            const Unit *other = UnitController_getUnit(&controller, (u8)j);
            if (other != NULL && other->alive)
                assert(unit->x != other->x || unit->y != other->y);
        }
    }

    for (cellIndex = 0; cellIndex < STRESS_MAP_SIZE; cellIndex++)
    {
        const s8 occupantId = cells[cellIndex].occupantId;
        if (occupantId == CELL_NO_OCCUPANT) continue;
        assert(occupantId >= 0 && (u8)occupantId < STRESS_UNIT_COUNT);
        {
            const Unit *unit = UnitController_getUnit(&controller, (u8)occupantId);
            const u16 expectedX = (u16)(cellIndex % 40u);
            const u16 expectedY = (u16)(cellIndex / 40u);
            assert(unit != NULL && unit->alive);
            assert(unit->x == expectedX && unit->y == expectedY);
        }
    }
}

static UnitType typeFor(u8 slot)
{
    static const UnitType types[4] = {
        UNIT_TYPE_INFANTRY, UNIT_TYPE_SCOUT, UNIT_TYPE_ENGINEER, UNIT_TYPE_TANK
    };
    return types[slot % 4u];
}

int main(void)
{
    MissionObjective objective = { "Batalla simetrica", 20, 20,
        MISSION_ELIMINATE_ALL, 1, TEAM_BLUE, 0 };
    u16 i;
    u32 decisions = 0;
    u16 moves[2] = { 0, 0 };
    u16 attacks[2] = { 0, 0 };
    u16 repairs[2] = { 0, 0 };
    u16 finishedTurns[2] = { 0, 0 };
    u16 integrityChecks = 0;
    u16 casualties[2] = { 0, 0 };
    u16 totalIterations = 0;
    u8 round;

    for (i = 0; i < STRESS_UNIT_COUNT; i++)
    {
        const u8 column = (u8)(i % 6u);
        const u8 row = (u8)((i / 6u) % 10u);
        starts[i].id = (u8)i;
        starts[i].x = (u16)(((i < 60u) ? 7u : 13u) + column);
        starts[i].y = (u16)(10u + row);
        starts[i].movementPointsPerTurn = 0;
        starts[i].type = typeFor((u8)i);
        starts[i].team = (i < 60u) ? TEAM_BLUE : TEAM_RED;
    }

    assert(MapData_init(&map));
    CellEvents_init(NULL, 0);
    assert(UnitController_init(&controller, &map, units, STRESS_UNIT_COUNT,
                               starts, STRESS_UNIT_COUNT));
    UnitController_setFogEnabled(&controller, TRUE);

    /* Escenario determinista: ambos equipos conocen las casillas para aislar
       la lógica táctica; la suite de niebla verifica el filtrado de visibilidad. */
    for (i = 0; i < STRESS_MAP_SIZE; i++)
    {
        cells[i].visionState = (u8)(cells[i].visionState |
            (MAP_VISION_VISIBLE << (TEAM_RED * 2u)) |
            (MAP_VISION_VISIBLE << (TEAM_BLUE * 2u)));
    }

    EnemyAI_init(&aiRed);
    EnemyAI_init(&aiBlue);

    /* Doce medias rondas como máximo: cada bando usa el mismo motor táctico,
       su propio estado de IA y las reglas comunes de movimiento/combate. */
    for (round = 1; round <= 12u; round++)
    {
        const TeamId teams[2] = { TEAM_RED, TEAM_BLUE };
        u8 side;
        for (side = 0; side < 2u; side++)
        {
            const TeamId team = teams[side];
            EnemyAI *ai = (team == TEAM_RED) ? &aiRed : &aiBlue;
            u16 iteration;
            bool turnFinished = FALSE;

            assert(UnitController_beginTeamTurn(&controller, team, round));
            for (iteration = 0; iteration < 10000u; iteration++)
            {
                EnemyAIResult result;
                CombatResult combatResult;
                if (UnitController_isMoving(&controller))
                {
                    (void)UnitController_update(&controller);
                    if (!UnitController_isMoving(&controller))
                    {
                        assertSpatialIntegrity();
                        integrityChecks++;
                    }
                    continue;
                }

                result = EnemyAI_update(ai, &controller, &map, team, round,
                                        &objective, &combatResult);
                decisions++;
                if ((result == ENEMY_AI_MOVE_STARTED) ||
                    (result == ENEMY_AI_RETREAT_STARTED))
                    moves[side]++;
                else if (result == ENEMY_AI_ATTACK_RESOLVED)
                {
                    attacks[side]++;
                    assertSpatialIntegrity();
                    integrityChecks++;
                }
                else if (result == ENEMY_AI_REPAIR_RESOLVED)
                    repairs[side]++;
                else if (result == ENEMY_AI_TURN_FINISHED)
                {
                    finishedTurns[side]++;
                    turnFinished = TRUE;
                    break;
                }
            }
            assert(turnFinished);
            totalIterations = (u16)(totalIterations + iteration);
        }
    }

    assert(moves[0] > 0u && moves[1] > 0u);
    assert(attacks[0] > 0u && attacks[1] > 0u);
    assert(finishedTurns[0] == 12u && finishedTurns[1] == 12u);

    assertSpatialIntegrity();
    integrityChecks++;
    for (i = 0; i < STRESS_UNIT_COUNT; i++)
    {
        const Unit *unit = UnitController_getUnit(&controller, (u8)i);
        assert(unit != NULL);
        if (!unit->alive)
        {
            if (unit->team == TEAM_RED) casualties[0]++;
            else casualties[1]++;
        }
    }

    printf("PASS: batalla simetrica 120 unidades; decisiones=%lu iteraciones=%u "
           "movimientos(R/A)=%u/%u ataques(R/A)=%u/%u reparaciones(R/A)=%u/%u "
           "bajas(R/A)=%u/%u turnos(R/A)=%u/%u integridad=%u\n",
           (unsigned long)decisions, totalIterations,
           moves[0], moves[1], attacks[0], attacks[1], repairs[0], repairs[1],
           casualties[0], casualties[1], finishedTurns[0], finishedTurns[1],
           integrityChecks);
    return 0;
}
