/*
 * Decisión táctica de la IA. Evalúa acciones y devuelve una orden por actualización;
 * el bucle principal conserva el control de animaciones y del avance de fotogramas.
 */
#include "enemy_ai.h"
#include "combat.h"
#include "performance.h"

/*
 * Planificación táctica por equipo. Una llamada a EnemyAI_update() resuelve como
 * máximo una acción; la animación queda en el bucle principal. El equipo dirigido
 * se pasa explícitamente a los cálculos que lo necesitan: no hay contexto global
 * mutable, así que las instancias de IA son independientes entre sí.
 */

#define AI_OBJECTIVE_DEFENSE_RADIUS 3u
#define AI_OBJECTIVE_PROGRESS_WEIGHT 4
#define AI_DEFENSE_SCORE_WEIGHT 2
#define AI_RISK_LOOKAHEAD_MARGIN 8
#define AI_TARGET_OBJECTIVE_RADIUS 5u
#define AI_MAX_VISIBLE_ENEMIES ENEMY_AI_MAX_UNITS
#define AI_MAX_TARGET_DISTANCE_BONUS 3u
#define AI_FIRE_LINE_POSITION_BONUS 24
#define AI_BLOCKED_FIRE_POSITION_PENALTY 28
#define AI_ALLY_ADJACENCY_PENALTY 2
#define AI_ALLY_ROUTE_BLOCK_PENALTY 6
#define AI_REPAIR_APPROACH_WEIGHT 8

/* Utilidades escalares y resolución del plan de misión. */

static u16 distanceBetween(u16 ax, u16 ay, u16 bx, u16 by)
{
    const u16 dx = (ax > bx) ? (u16)(ax - bx) : (u16)(bx - ax);
    const u16 dy = (ay > by) ? (u16)(ay - by) : (u16)(by - ay);
    return (u16)(dx + dy);
}

static u16 minimumU16(u16 a, u16 b)
{
    return (a < b) ? a : b;
}

static bool isVisibleToAI(const MapData *map, const Unit *unit, TeamId team)
{
    return (map != NULL) && (unit != NULL) &&
        (MapData_getVisionState(map, (u8)team, unit->x, unit->y) ==
         MAP_VISION_VISIBLE);
}

static TeamId getObjectiveOwner(const UnitController *units,
                                const MapData *map,
                                const MissionObjective *objective)
{
    const CellData *cell;
    u8 i;

    if ((units == NULL) || (map == NULL) || (objective == NULL))
        return TEAM_NONE;

    cell = MapData_getCellConst(map, objective->x, objective->y);
    if ((cell == NULL) || (cell->occupantId == CELL_NO_OCCUPANT))
        return TEAM_NONE;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        if ((unit != NULL) && unit->alive &&
            (unit->id == (u8)cell->occupantId))
            return unit->team;
    }
    return TEAM_NONE;
}

static EnemyAITacticalPlan resolveTacticalPlan(const MissionObjective *objective,
                                                TeamId objectiveOwner,
                                                TeamId team)
{
    if (objective == NULL) return AI_PLAN_ELIMINATION_SWEEP;

    switch (objective->type)
    {
        case MISSION_CAPTURE_POINT:
            return (objectiveOwner == team)
                ? AI_PLAN_OBJECTIVE_SECURED : AI_PLAN_OBJECTIVE_ASSAULT;

        case MISSION_HOLD_POINT:
            return (objectiveOwner == team)
                ? AI_PLAN_OBJECTIVE_DEFENSE : AI_PLAN_OBJECTIVE_ASSAULT;

        case MISSION_ELIMINATE_ALL:
            return AI_PLAN_ELIMINATION_SWEEP;

        case MISSION_SURVIVE_TURNS:
            return (objective->targetTeam == team)
                ? AI_PLAN_SURVIVE_DEFENSE : AI_PLAN_SURVIVE_COUNTERATTACK;

        default:
            return AI_PLAN_ELIMINATION_SWEEP;
    }
}

static u8 terrainDefenseAt(const MapData *map, u16 x, u16 y)
{
    const CellData *cell = MapData_getCellConst(map, x, y);
    const TerrainInfo *terrain;

    if (cell == NULL) return 0;
    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    return (terrain != NULL) ? terrain->defenseBonus : 0;
}

/*
 * Preferencias tacticas adicionales por clase.
 * El coste de entrada ya lo impone el pathfinder; esta puntuacion solo decide
 * entre destinos alcanzables que ofrecen un progreso estrategico parecido.
 * Menor puntuacion es mejor.
 */
static s32 classTerrainPositionPenalty(const Unit *unit, const MapData *map,
                                       u16 x, u16 y)
{
    const CellData *cell;
    const TerrainInfo *terrain;

    if ((unit == NULL) || (map == NULL)) return 0;
    cell = MapData_getCellConst(map, x, y);
    if (cell == NULL) return 0;

    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    if (terrain == NULL) return 0;

    switch (unit->type)
    {
        case UNIT_TYPE_INFANTRY:
            /* Infanteria: prioriza posiciones defensivas para sostener terreno. */
            return -(s32)terrain->defenseBonus * 2;

        case UNIT_TYPE_SCOUT:
            /* Exploradores: aprovechan cobertura ligera sin perder el flanqueo. */
            return (cell->terrain == TERRAIN_FOREST) ? -2 : 0;

        case UNIT_TYPE_ENGINEER:
            /* Ingenieros: evita exponer una unidad de apoyo poco ofensiva. */
            return -(s32)terrain->defenseBonus * 3;

        case UNIT_TYPE_TANK:
            /* Blindados: prefieren caminos y evitan quedar encajonados en roca. */
            if (cell->terrain == TERRAIN_PATH) return -4;
            if (cell->terrain == TERRAIN_FOREST) return 3;
            if (cell->terrain == TERRAIN_ROCK) return 7;
            return 0;

        default:
            return 0;
    }
}

/* Bonus/penalizacion para terminar el movimiento en una casilla desde la que
   el atacante pueda usar su propio alcance con linea de tiro despejada. */
static s32 firingLanePositionPenalty(const MapData *map, const Unit *unit,
                                     const Unit *target, u16 x, u16 y)
{
    if ((map == NULL) || (unit == NULL) || (target == NULL) || !target->alive ||
        (unit->attackRange == 0) ||
        (distanceBetween(x, y, target->x, target->y) > unit->attackRange))
        return 0;

    return MapData_hasLineOfSight(map, x, y, target->x, target->y)
        ? -(s32)AI_FIRE_LINE_POSITION_BONUS
        : (s32)AI_BLOCKED_FIRE_POSITION_PENALTY;
}

static bool mustPrioritizeObjective(const Unit *unit,
                                    const MissionObjective *objective,
                                    TeamId objectiveOwner,
                                    EnemyAITacticalRole role,
                                    TeamId team)
{
    if ((unit == NULL) || (objective == NULL) ||
        (role != AI_ROLE_OBJECTIVE_RUNNER) || (objectiveOwner == team))
        return FALSE;

    return (objective->type == MISSION_CAPTURE_POINT) ||
           (objective->type == MISSION_HOLD_POINT);
}

/* Distancia restante hasta una casilla desde la que pueda reparar al aliado. */
static s32 repairApproachPositionScore(const Unit *engineer, const Unit *ally,
                                       u16 x, u16 y)
{
    const u16 distance = distanceBetween(x, y, ally->x, ally->y);

    if (distance <= engineer->repairRange) return 0;
    return (s32)(distance - engineer->repairRange) * AI_REPAIR_APPROACH_WEIGHT;
}

/*
 * Objetivo de apoyo para movimiento: se calcula una sola vez por decision, no
 * dentro del bucle que puntua cada casilla alcanzable. La reparacion real sigue
 * pasando por Combat_predictRepairUnits()/Combat_repair() al llegar.
 */
static const Unit *findBestRepairApproachTarget(const UnitController *units,
                                                 const Unit *engineer)
{
    const Unit *best = NULL;
    u16 bestMissingPercent = 0;
    u8 bestMissingHealth = 0;
    u16 bestDistance = 0xFFFFu;
    u8 i;

    if ((units == NULL) || (engineer == NULL) || (engineer->repairAmount == 0) ||
        (engineer->repairRange == 0) || engineer->hasAttackedThisTurn ||
        engineer->hasRepairedThisTurn)
        return NULL;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *ally = UnitController_getUnit(units, i);
        u8 missingHealth;
        u16 missingPercent;
        u16 distance;

        if ((ally == NULL) || !ally->alive || (ally->team != engineer->team) ||
            (ally->id == engineer->id) || (ally->maxHealth == 0) ||
            (ally->health >= ally->maxHealth))
            continue;

        missingHealth = (u8)(ally->maxHealth - ally->health);
        missingPercent = (u16)(((u16)missingHealth * 100u) / ally->maxHealth);
        distance = distanceBetween(engineer->x, engineer->y, ally->x, ally->y);
        if ((best == NULL) || (missingPercent > bestMissingPercent) ||
            ((missingPercent == bestMissingPercent) &&
             (missingHealth > bestMissingHealth)) ||
            ((missingPercent == bestMissingPercent) &&
             (missingHealth == bestMissingHealth) && (distance < bestDistance)))
        {
            best = ally;
            bestMissingPercent = missingPercent;
            bestMissingHealth = missingHealth;
            bestDistance = distance;
        }
    }

    return best;
}

static EnemyAITacticalRole defaultRoleForUnit(const Unit *unit)
{
    if (unit == NULL) return AI_ROLE_ASSAULT;

    switch (unit->type)
    {
        case UNIT_TYPE_SCOUT: return AI_ROLE_FLANK;
        case UNIT_TYPE_TANK:  return AI_ROLE_SUPPORT;
        case UNIT_TYPE_ENGINEER: return AI_ROLE_SUPPORT;
        case UNIT_TYPE_INFANTRY:
        default: return AI_ROLE_ASSAULT;
    }
}

EnemyAITacticalRole EnemyAI_getTacticalRole(const EnemyAI *ai, u8 unitIndex)
{
    if ((ai == NULL) || (unitIndex >= ENEMY_AI_MAX_UNITS))
        return AI_ROLE_ASSAULT;
    if (ai->tacticalRoles[unitIndex] > (u8)AI_ROLE_SCREEN)
        return AI_ROLE_ASSAULT;
    return (EnemyAITacticalRole)ai->tacticalRoles[unitIndex];
}

EnemyAITacticalPlan EnemyAI_getTacticalPlan(const EnemyAI *ai)
{
    if ((ai == NULL) || (ai->tacticalPlan > (u8)AI_PLAN_SURVIVE_COUNTERATTACK))
        return AI_PLAN_ELIMINATION_SWEEP;
    return (EnemyAITacticalPlan)ai->tacticalPlan;
}

/* Asignación de roles: corredor, escolta, guardias, pantalla y apoyo. */

static s32 objectiveRunnerScore(const Unit *unit,
                                const MissionObjective *objective)
{
    u16 distance;
    u8 movementBudget;
    s32 score;

    if ((unit == NULL) || (objective == NULL)) return 0x7FFFFFFF;

    distance = distanceBetween(unit->x, unit->y, objective->x, objective->y);
    movementBudget = (unit->maxMovementPoints != 0)
        ? unit->maxMovementPoints : 1u;

    /* Estima el numero relativo de turnos de aproximacion, no solo casillas.
       Asi un explorador mas lejano puede ganar si su movilidad compensa la
       distancia. El pathfinder sigue siendo autoritativo al ejecutar rutas. */
    score = ((s32)distance * 1000L) / movementBudget;
    score += distance; /* desempate estable a favor de la unidad mas cercana */

    /* No asignar un corredor que ya gasto su orden de movimiento si hay otra
       unidad que todavia puede actuar este turno. */
    if (unit->movementPoints == 0) score += 100000L;

    /* En igualdad aproximada de llegada, evita exponer primero al mas herido. */
    if ((unit->maxHealth != 0) && (unit->health < unit->maxHealth))
    {
        const u8 missingHealth = (u8)(unit->maxHealth - unit->health);
        score += ((s32)missingHealth * 50L) / unit->maxHealth;
    }

    return score;
}

static s16 findObjectiveRunner(const UnitController *units,
                               const MissionObjective *objective,
                               TeamId team)
{
    const u8 count = UnitController_getCount(units);
    s32 bestScore = 0x7FFFFFFF;
    s16 bestIndex = -1;
    u8 i;

    for (i = 0; (i < count) && (i < ENEMY_AI_MAX_UNITS); i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        s32 score;

        if ((unit == NULL) || !unit->alive || (unit->team != team)) continue;
        score = objectiveRunnerScore(unit, objective);
        if ((bestIndex < 0) || (score < bestScore))
        {
            bestIndex = (s16)i;
            bestScore = score;
        }
    }
    return bestIndex;
}

static s16 findEscort(const UnitController *units, s16 runnerIndex,
                      TeamId team)
{
    const Unit *runner;
    const u8 count = UnitController_getCount(units);
    s32 bestScore = 0x7FFFFFFF;
    s16 bestIndex = -1;
    u8 i;

    if (runnerIndex < 0) return -1;
    runner = UnitController_getUnit(units, (u8)runnerIndex);
    if ((runner == NULL) || !runner->alive) return -1;

    for (i = 0; (i < count) && (i < ENEMY_AI_MAX_UNITS); i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        s32 score;

        if ((unit == NULL) || !unit->alive || (unit->team != team) ||
            (i == (u8)runnerIndex))
            continue;

        score = (s32)distanceBetween(unit->x, unit->y, runner->x, runner->y) * 10;
        /* Escolta primero una unidad resistente que pueda acompañar al corredor. */
        if (unit->type == UNIT_TYPE_TANK) score += 15;
        if (unit->type == UNIT_TYPE_ENGINEER) score -= 3;
        if (unit->health < (u8)(unit->maxHealth / 2u)) score += 4;

        if ((bestIndex < 0) || (score < bestScore))
        {
            bestIndex = (s16)i;
            bestScore = score;
        }
    }
    return bestIndex;
}

static s32 guardAssignmentScore(const Unit *unit,
                                const MissionObjective *objective)
{
    s32 score = (s32)distanceBetween(unit->x, unit->y,
                                     objective->x, objective->y) * 10;
    score -= (s32)unit->defense * 2;
    score -= (s32)unit->health / 4;
    return score;
}

static void assignTacticalRoles(EnemyAI *ai, const UnitController *units,
                                const MissionObjective *objective,
                                TeamId objectiveOwner, TeamId team)
{
    const u8 count = UnitController_getCount(units);
    const u8 usableCount = (count < ENEMY_AI_MAX_UNITS)
        ? count : ENEMY_AI_MAX_UNITS;
    bool guarded[ENEMY_AI_MAX_UNITS];
    s16 runner = -1;
    u8 i;

    ai->objectiveRunnerIndex = -1;
    ai->retreating = FALSE;
    for (i = 0; i < ENEMY_AI_MAX_UNITS; i++)
    {
        ai->tacticalRoles[i] = (u8)AI_ROLE_ASSAULT;
        guarded[i] = FALSE;
    }

    for (i = 0; i < usableCount; i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        if ((unit != NULL) && unit->alive && (unit->team == team))
            ai->tacticalRoles[i] = (u8)defaultRoleForUnit(unit);
    }

    if (((objective->type == MISSION_CAPTURE_POINT) ||
         (objective->type == MISSION_HOLD_POINT)) &&
        (objectiveOwner != team))
    {
        runner = findObjectiveRunner(units, objective, team);
        if (runner >= 0)
        {
            ai->objectiveRunnerIndex = runner;
            ai->tacticalRoles[(u8)runner] = (u8)AI_ROLE_OBJECTIVE_RUNNER;
            {
                const s16 escort = findEscort(units, runner, team);
                if (escort >= 0)
                    ai->tacticalRoles[(u8)escort] = (u8)AI_ROLE_ESCORT;
            }
        }
    }
    else if ((((objective->type == MISSION_HOLD_POINT) ||
               (objective->type == MISSION_CAPTURE_POINT)) &&
              (objectiveOwner == team)) ||
             ((objective->type == MISSION_SURVIVE_TURNS) &&
              (objective->targetTeam == team)))
    {
        /* Se asignan hasta dos guardias estables; las demás unidades cubren
           el perímetro con roles de pantalla, flanqueo o apoyo. */
        u8 guardNumber;
        for (guardNumber = 0; guardNumber < 2; guardNumber++)
        {
            s32 bestScore = 0x7FFFFFFF;
            s16 bestIndex = -1;
            for (i = 0; i < usableCount; i++)
            {
                const Unit *unit = UnitController_getUnit(units, i);
                s32 score;
                if (guarded[i] || (unit == NULL) || !unit->alive ||
                    (unit->team != team))
                    continue;
                score = guardAssignmentScore(unit, objective);
                if ((bestIndex < 0) || (score < bestScore))
                {
                    bestIndex = (s16)i;
                    bestScore = score;
                }
            }
            if (bestIndex >= 0)
            {
                guarded[(u8)bestIndex] = TRUE;
                ai->tacticalRoles[(u8)bestIndex] = (u8)AI_ROLE_GUARD;
            }
        }

        for (i = 0; i < usableCount; i++)
        {
            const Unit *unit = UnitController_getUnit(units, i);
            if ((unit == NULL) || !unit->alive || (unit->team != team) ||
                guarded[i])
                continue;
            if (ai->tacticalRoles[i] == (u8)AI_ROLE_ASSAULT)
                ai->tacticalRoles[i] = (u8)AI_ROLE_SCREEN;
        }
    }
}

/* Daño aproximado que los enemigos conocidos podrían infligir desde su
   posición actual. Solo usa enemigos que Rojo ve, nunca información oculta. */
/* Estimación de amenaza y selección de posiciones seguras. */

static s32 estimatePositionRisk(const UnitController *units,
                                const u8 *visibleEnemyIndices,
                                u8 visibleEnemyCount, const MapData *map,
                                const Unit *unit, u16 x, u16 y)
{
    s32 risk = 0;
    const u8 cover = terrainDefenseAt(map, x, y);
    u8 i;

    /* La lista ya está filtrada por visión para esta decisión. No repitas una
       consulta de mapa y un barrido de todas las unidades por cada casilla. */
    for (i = 0; i < visibleEnemyCount; i++)
    {
        const Unit *enemy = UnitController_getUnit(units, visibleEnemyIndices[i]);
        u16 distance;
        u16 defense;
        u8 damage;

        distance = distanceBetween(x, y, enemy->x, enemy->y);
        if ((distance == 0) || (distance > enemy->attackRange)) continue;
        if (!MapData_hasLineOfSight(map, enemy->x, enemy->y, x, y)) continue;

        defense = (u16)unit->defense + cover;
        damage = (enemy->attack > defense)
            ? (u8)(enemy->attack - defense) : 1u;
        risk += damage;
        if (damage >= unit->health) risk += 10;
        if (risk >= 60) return 60;
    }
    return risk;
}

/* Elige un blanco para aproximación, con una prioridad adicional según el rol.
   La lista solo incluye enemigos vivos que Rojo ve actualmente. */
static const Unit *findBestVisibleTarget(const UnitController *units,
                                         const Unit *attacker,
                                         const MissionObjective *objective,
                                         EnemyAITacticalRole role,
                                         const Unit *runner,
                                         s16 focusTargetIndex,
                                         EnemyAITacticalPlan plan,
                                         const u8 *visibleEnemyIndices,
                                         u8 visibleEnemyCount)
{
    const Unit *best = NULL;
    s32 bestScore = 0x7FFFFFFF;
    u8 slot;

    /* La lista ya se filtró una vez por vida, equipo y visión. */
    for (slot = 0; slot < visibleEnemyCount; slot++)
    {
        const u8 i = visibleEnemyIndices[slot];
        const Unit *candidate = UnitController_getUnit(units, i);
        const u16 distance = distanceBetween(attacker->x, attacker->y,
                                             candidate->x, candidate->y);
        s32 score;

        score = (s32)distance * 10;
        score -= (s32)candidate->attack * 2;
        score -= (s32)candidate->attackRange * 3;
        if (candidate->health <= attacker->attack) score -= 8;
        if ((objective != NULL) &&
            (distanceBetween(candidate->x, candidate->y,
                             objective->x, objective->y) <= 2u))
            score -= 12;

        /* Los escoltas priorizan amenazas cerca del corredor. */
        if ((role == AI_ROLE_ESCORT) && (runner != NULL) &&
            (distanceBetween(candidate->x, candidate->y,
                             runner->x, runner->y) <= 2u))
            score -= 12;
        /* El corredor no abandona el objetivo para perseguir un blanco distante. */
        if ((role == AI_ROLE_OBJECTIVE_RUNNER) && (objective != NULL) &&
            (distanceBetween(candidate->x, candidate->y,
                             objective->x, objective->y) > AI_TARGET_OBJECTIVE_RADIUS))
            score += 20;
        /* Los flanqueadores se interesan por los enemigos cercanos al objetivo,
           pero su aproximación lateral se resuelve al puntuar los destinos. */
        if ((role == AI_ROLE_FLANK) && (objective != NULL) &&
            (distanceBetween(candidate->x, candidate->y,
                             objective->x, objective->y) <= AI_TARGET_OBJECTIVE_RADIUS))
            score -= 4;

        /* El apoyo de fuego cubre la columna del corredor: no abandona la
           maniobra para perseguir un blanco secundario lejos del objetivo. */
        if ((role == AI_ROLE_SUPPORT) && (runner != NULL) && runner->alive &&
            (plan == AI_PLAN_OBJECTIVE_ASSAULT))
        {
            const u16 runnerDistance = distanceBetween(candidate->x, candidate->y,
                                                        runner->x, runner->y);
            const u16 objectiveDistance = distanceBetween(candidate->x, candidate->y,
                                                           objective->x, objective->y);
            if ((runnerDistance <= 3u) ||
                (objectiveDistance <= AI_TARGET_OBJECTIVE_RADIUS))
                score -= 10;
            else
                score += 24 + (s32)(runnerDistance - 3u) * 2;
        }
        else if ((role == AI_ROLE_SUPPORT) &&
                 (plan == AI_PLAN_OBJECTIVE_DEFENSE))
        {
            const u16 objectiveDistance = distanceBetween(candidate->x, candidate->y,
                                                           objective->x, objective->y);
            if (objectiveDistance <= AI_TARGET_OBJECTIVE_RADIUS)
                score -= 8;
            else
                score += (s32)(objectiveDistance - AI_TARGET_OBJECTIVE_RADIUS) * 5;
        }

        /* Una vez elegida una amenaza prioritaria, varias unidades convergen
           en ella. El corredor mantiene la prioridad del objetivo de misión. */
        if ((focusTargetIndex >= 0) &&
            ((s16)(candidate - units->units) == focusTargetIndex) &&
            (role != AI_ROLE_OBJECTIVE_RUNNER) &&
            ((role != AI_ROLE_GUARD) || (objective == NULL) ||
             (distanceBetween(candidate->x, candidate->y,
                              objective->x, objective->y) <=
              AI_OBJECTIVE_DEFENSE_RADIUS)))
            score -= 24;

        if ((best == NULL) || (score < bestScore))
        {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

/* Ataca el objetivo con el mejor intercambio y ajusta el blanco al rol táctico. */
static const Unit *findBestAttackTarget(const UnitController *units,
                                        const Unit *attacker,
                                        const MissionObjective *objective,
                                        EnemyAITacticalRole role,
                                        const Unit *runner,
                                        s16 focusTargetIndex,
                                        TeamId team,
                                        const u8 *visibleEnemyIndices,
                                        u8 visibleEnemyCount)
{
    const Unit *best = NULL;
    s32 bestScore = 0;
    u8 slot;

    if (attacker->hasAttackedThisTurn || attacker->hasRepairedThisTurn) return NULL;

    for (slot = 0; slot < visibleEnemyCount; slot++)
    {
        const u8 i = visibleEnemyIndices[slot];
        const Unit *candidate = UnitController_getUnit(units, i);
        CombatResult prediction;
        s32 score;

        prediction = Combat_predictUnits(units, attacker, candidate, team);
        if (prediction.code != COMBAT_RESULT_ATTACKED) continue;
        /* La coordinación nunca debe convertir un intercambio letal en una
           orden válida: si el rival sobrevive y la represalia mata al actor,
           se descarta el ataque antes de puntuar el foco compartido. */
        if (!prediction.defenderDefeated &&
            (prediction.counterDamage >= attacker->health))
            continue;

        score = 20 + (s32)prediction.damageDealt * 8;
        if (prediction.defenderDefeated) score += 100;
        score += (s32)candidate->attack * 2;
        score += (s32)candidate->attackRange * 3;
        score -= (s32)prediction.counterDamage * 6;
        if ((objective != NULL) &&
            (distanceBetween(candidate->x, candidate->y,
                             objective->x, objective->y) <= 2u))
            score += 20;

        if (role == AI_ROLE_SUPPORT)
        {
            /* Los blindados/unidades de apoyo eliminan primero amenazas potentes. */
            score += (s32)candidate->attack * 3;
            score += (s32)candidate->attackRange * 4;
            if (candidate->type == UNIT_TYPE_TANK) score += 8;
        }
        else if (role == AI_ROLE_GUARD)
        {
            const u16 objectiveDistance = (objective != NULL)
                ? distanceBetween(candidate->x, candidate->y,
                                  objective->x, objective->y) : 0xFFFFu;
            if (objectiveDistance <= AI_OBJECTIVE_DEFENSE_RADIUS) score += 18;
            else score -= 12;
        }
        else if ((role == AI_ROLE_ESCORT) && (runner != NULL) &&
                 (distanceBetween(candidate->x, candidate->y,
                                  runner->x, runner->y) <= 2u))
            score += 18;
        else if ((role == AI_ROLE_OBJECTIVE_RUNNER) && (objective != NULL) &&
                 (distanceBetween(candidate->x, candidate->y,
                                  objective->x, objective->y) <= 1u))
            score += 24;

        /* El foco compartido sirve para completar eliminaciones que requieren
           más de un ataque. Si este golpe ya elimina al blanco, la bonificación
           de derrota (+100) expresa toda la prioridad necesaria: no añadimos
           otra recompensa redundante que sesgue el resto de la puntuación.
           Combat_predictUnits ya limita damageDealt a la salud restante, por lo
           que el cálculo nunca premia daño ficticio por encima de los HP reales. */
        if ((focusTargetIndex >= 0) &&
            ((s16)(candidate - units->units) == focusTargetIndex) &&
            !prediction.defenderDefeated)
            score += 32;

        if (score > bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

/* Puntuación de destinos tácticos y preferencias por rol. */

static s32 approachScore(u16 x, u16 y, const Unit *target, u8 desiredRange)
{
    const u16 distance = distanceBetween(x, y, target->x, target->y);
    return (distance > desiredRange)
        ? (s32)(distance - desiredRange) : 0;
}

/* Puntuación estratégica por misión. Menor es mejor.
   Los guardias y pantallas usan un anillo mayor para no perseguir fuera del punto. */
static s32 strategicPositionScore(u16 x, u16 y, const Unit *actingUnit,
                                  const Unit *target,
                                  const MissionObjective *objective,
                                  TeamId objectiveOwner, TeamId team)
{
    const u16 objectiveDistance = distanceBetween(
        x, y, objective->x, objective->y);
    const u8 attackRange = (actingUnit != NULL) ? actingUnit->attackRange : 1u;
    s32 score;

    switch (objective->type)
    {
        case MISSION_CAPTURE_POINT:
            score = (s32)objectiveDistance * AI_OBJECTIVE_PROGRESS_WEIGHT;
            if ((target != NULL) &&
                (distanceBetween(target->x, target->y,
                                 objective->x, objective->y) <=
                 AI_TARGET_OBJECTIVE_RADIUS))
                score += minimumU16((u16)approachScore(x, y, target, attackRange),
                                    AI_MAX_TARGET_DISTANCE_BONUS);
            return score;

        case MISSION_HOLD_POINT:
            if (objectiveOwner == team)
            {
                score = (objectiveDistance > 5u)
                    ? (s32)(objectiveDistance - 5u) * 3 : 0;
                if ((target != NULL) && (objectiveDistance <= 5u) &&
                    (distanceBetween(target->x, target->y,
                                     objective->x, objective->y) <=
                     AI_TARGET_OBJECTIVE_RADIUS))
                    score += minimumU16((u16)approachScore(x, y, target, attackRange),
                                        AI_MAX_TARGET_DISTANCE_BONUS);
                return score;
            }

            score = (s32)objectiveDistance * AI_OBJECTIVE_PROGRESS_WEIGHT;
            if ((target != NULL) &&
                (distanceBetween(target->x, target->y,
                                 objective->x, objective->y) <=
                 AI_TARGET_OBJECTIVE_RADIUS))
                score += minimumU16((u16)approachScore(x, y, target, attackRange),
                                    AI_MAX_TARGET_DISTANCE_BONUS);
            return score;

        case MISSION_ELIMINATE_ALL:
            if (target != NULL) return approachScore(x, y, target, attackRange) * 3;
            return (s32)objectiveDistance * 2;

        case MISSION_SURVIVE_TURNS:
            if (objective->targetTeam == team)
                return (objectiveDistance > 5u)
                    ? (s32)(objectiveDistance - 5u) * 3 : 0;
            if (target != NULL) return approachScore(x, y, target, attackRange) * 3;
            return (s32)objectiveDistance * 2;

        default:
            return (s32)objectiveDistance * AI_OBJECTIVE_PROGRESS_WEIGHT;
    }
}

static u16 clampCoordinate(s32 coordinate, u16 limit)
{
    if (coordinate < 0) return 0;
    if ((u32)coordinate >= limit) return (u16)(limit - 1u);
    return (u16)coordinate;
}

/* Penaliza llegar de frente: el flanqueador busca el lateral del blanco. */
static s32 flankPositionPenalty(u16 x, u16 y, const Unit *unit,
                                const Unit *target,
                                const MissionObjective *objective,
                                const MapData *map)
{
    u16 anchorX = objective->x;
    u16 anchorY = objective->y;
    s32 waypointX;
    s32 waypointY;

    if ((target != NULL) &&
        ((objective->type == MISSION_ELIMINATE_ALL) ||
         (distanceBetween(target->x, target->y,
                          objective->x, objective->y) <=
          AI_TARGET_OBJECTIVE_RADIUS)))
    {
        anchorX = target->x;
        anchorY = target->y;

        if (distanceBetween(unit->x, unit->y, anchorX, anchorY) == 0)
        {
            /* Solo se usa como desempate en una situación poco probable. */
            waypointX = (s32)anchorX + ((unit->id & 1u) ? 1 : -1);
            waypointY = anchorY;
        }
        else
        {
            const s32 dx = (s32)unit->x - anchorX;
            const s32 dy = (s32)unit->y - anchorY;
            if (((dx < 0) ? -dx : dx) >= ((dy < 0) ? -dy : dy))
            {
                waypointX = anchorX;
                waypointY = (s32)anchorY + ((unit->id & 1u) ? 1 : -1);
            }
            else
            {
                waypointY = anchorY;
                waypointX = (s32)anchorX + ((unit->id & 1u) ? 1 : -1);
            }
        }
    }
    else
    {
        /* Al aproximarse al objetivo, el equipo se reparte a ambos flancos. */
        waypointX = (s32)anchorX + ((unit->id & 1u) ? 2 : -2);
        waypointY = (s32)anchorY + ((unit->id & 2u) ? 1 : -1);
    }

    waypointX = clampCoordinate(waypointX, map->width);
    waypointY = clampCoordinate(waypointY, map->height);
    return (s32)distanceBetween(x, y, (u16)waypointX, (u16)waypointY) * 2;
}

static s32 objectiveStationPenalty(u16 x, u16 y, const Unit *unit,
                                   const MissionObjective *objective,
                                   const MapData *map, u16 radius)
{
    s32 slotX;
    s32 slotY;

    /* Las unidades de un mismo grupo ocupan sectores distintos del anillo,
       en lugar de buscar exactamente la misma casilla del objetivo. */
    slotX = (s32)objective->x + ((unit->id & 1u) ? (s32)radius : -(s32)radius);
    slotY = (s32)objective->y + ((unit->id & 2u) ? 1 : -1);
    slotX = clampCoordinate(slotX, map->width);
    slotY = clampCoordinate(slotY, map->height);
    return (s32)distanceBetween(x, y, (u16)slotX, (u16)slotY) * 2;
}

static s32 supportStationPenalty(const Unit *unit, const Unit *target,
                                 const Unit *runner,
                                 const MissionObjective *objective,
                                 EnemyAITacticalPlan plan, const MapData *map,
                                 u16 x, u16 y)
{
    s32 score = 0;

    if (target != NULL)
    {
        const u16 preferredRange = (unit->attackRange > 0)
            ? unit->attackRange : 1u;
        const u16 distance = distanceBetween(x, y, target->x, target->y);
        const u16 rangeDifference = (distance > preferredRange)
            ? (u16)(distance - preferredRange)
            : (u16)(preferredRange - distance);
        const s32 dx = (s32)unit->x - target->x;
        const s32 dy = (s32)unit->y - target->y;
        const s32 absDx = (dx < 0) ? -dx : dx;
        const s32 absDy = (dy < 0) ? -dy : dy;
        const s32 lateralSign = (unit->id & 2u) ? 1 : -1;
        const s32 forwardSteps = (s32)preferredRange - 1;
        s32 slotX = target->x;
        s32 slotY = target->y;

        /* Posición de tiro desplazada un tile hacia un flanco, pero a alcance
           Manhattan útil. El lado depende del ID para repartir el apoyo. */
        if (absDx >= absDy)
        {
            slotX += (dx >= 0) ? forwardSteps : -forwardSteps;
            slotY += lateralSign;
        }
        else
        {
            slotY += (dy >= 0) ? forwardSteps : -forwardSteps;
            slotX += lateralSign;
        }
        slotX = clampCoordinate(slotX, map->width);
        slotY = clampCoordinate(slotY, map->height);

        score += (s32)rangeDifference * 3;
        score += (s32)distanceBetween(x, y, (u16)slotX, (u16)slotY) * 6;

        /* El slot lateral deriva del ID: no requiere barrer todas las unidades
           por cada casilla candidata, y evita que el apoyo elija el mismo eje. */
        return score;
    }

    if ((plan == AI_PLAN_OBJECTIVE_ASSAULT) && (runner != NULL) && runner->alive)
    {
        const u16 midX = (u16)(((u32)runner->x + objective->x) / 2u);
        const u16 midY = (u16)(((u32)runner->y + objective->y) / 2u);
        const s32 slotX = clampCoordinate((s32)midX + ((unit->id & 1u) ? 1 : -1),
                                          map->width);
        const s32 slotY = clampCoordinate((s32)midY + ((unit->id & 2u) ? 1 : -1),
                                          map->height);
        /* Sin enemigos visibles, el apoyo cubre la línea de avance del corredor. */
        score += (s32)distanceBetween(x, y, (u16)slotX, (u16)slotY) * 2;
        score += (s32)distanceBetween(x, y, runner->x, runner->y);
        return score;
    }

    /* Durante defensa, apoyo protege un sector exterior en vez de amontonarse
       sobre los guardias de la casilla objetivo. */
    return objectiveStationPenalty(x, y, unit, objective, map, 4u);
}

static s32 applyRolePositionScore(s32 baseScore, u16 x, u16 y,
                                  const Unit *unit, const Unit *target,
                                  const Unit *runner,
                                  const MissionObjective *objective,
                                  TeamId objectiveOwner,
                                  EnemyAITacticalRole role,
                                  EnemyAITacticalPlan plan,
                                  const MapData *map, TeamId team)
{
    const u16 objectiveDistance = distanceBetween(
        x, y, objective->x, objective->y);

    switch (role)
    {
        case AI_ROLE_OBJECTIVE_RUNNER:
            /* Es la unidad que tiene prioridad real sobre el punto. */
            baseScore += (s32)objectiveDistance * 2;
            break;

        case AI_ROLE_ESCORT:
            if ((runner != NULL) && runner->alive)
            {
                const u16 runnerDistance = distanceBetween(x, y, runner->x, runner->y);
                /* Mantiene un anillo de escolta de dos casillas: cerca para
                   interceptar amenazas, pero sin ocupar el tile del corredor. */
                if (runnerDistance < 2u)
                    baseScore += (s32)(2u - runnerDistance) * 5;
                else if (runnerDistance > 3u)
                    baseScore += (s32)(runnerDistance - 3u) * 5;
                baseScore += (s32)distanceBetween(x, y, objective->x, objective->y);
            }
            break;

        case AI_ROLE_GUARD:
            if (objectiveDistance > 2u)
                baseScore += (s32)(objectiveDistance - 2u) * 9 + 2;
            else baseScore += (s32)objectiveDistance * 2;
            baseScore += objectiveStationPenalty(x, y, unit, objective, map, 1u);
            baseScore -= (s32)terrainDefenseAt(map, x, y) * 3;
            break;

        case AI_ROLE_SCREEN:
            if (((objective->type == MISSION_HOLD_POINT) &&
                 (objectiveOwner == team)) ||
                ((objective->type == MISSION_SURVIVE_TURNS) &&
                 (objective->targetTeam == team)))
            {
                if (objectiveDistance > 5u)
                    baseScore += (s32)(objectiveDistance - 5u) * 7;
                baseScore += objectiveStationPenalty(x, y, unit, objective, map, 4u);
            }
            /* La pantalla busca también cobertura y un sector propio del anillo. */
            baseScore -= (s32)terrainDefenseAt(map, x, y) * AI_DEFENSE_SCORE_WEIGHT;
            break;

        case AI_ROLE_FLANK:
            baseScore += flankPositionPenalty(x, y, unit, target,
                                               objective, map);
            break;

        case AI_ROLE_SUPPORT:
            baseScore += supportStationPenalty(unit, target, runner, objective,
                plan, map, x, y);
            break;

        case AI_ROLE_ASSAULT:
        default:
            break;
    }
    return baseScore;
}

/*
 * Penaliza las posiciones que amontonan unidades o se colocan delante de una
 * compañera que avanza hacia el mismo objetivo. No convierte esas posiciones
 * en ilegales: en un puente de una sola casilla puede no existir alternativa.
 * El pathfinder sigue decidiendo qué casillas son realmente alcanzables.
 *
 * El coste es acotado al número de unidades (como máximo GAME_MAX_UNITS) y se
 * calcula por destino candidato. No realiza búsquedas de rutas ni consultas LOS.
 */
static u8 collectNearbyFriendlyIndices(const UnitController *units,
                                       const Unit *unit, u16 movementRadius,
                                       u8 *indices)
{
    const u8 count = UnitController_getCount(units);
    const u8 limit = (count < ENEMY_AI_MAX_UNITS)
        ? count : ENEMY_AI_MAX_UNITS;
    const u16 searchRadius = (u16)(movementRadius + 2u);
    u8 nearbyCount = 0;
    u8 i;

    if ((units == NULL) || (unit == NULL) || (indices == NULL)) return 0;

    /* Cualquier candidato alcanzable está a distancia Manhattan <= movimiento.
       Solo las aliadas a <= movimiento + 2 pueden afectar a su corona de dos
       casillas; filtrarlas aquí evita barrer todo el ejército por candidato. */
    for (i = 0; i < limit; i++)
    {
        const Unit *ally = UnitController_getUnit(units, i);
        if ((ally == NULL) || (ally == unit) || !ally->alive ||
            (ally->team != unit->team))
            continue;
        if (distanceBetween(unit->x, unit->y, ally->x, ally->y) <= searchRadius)
            indices[nearbyCount++] = i;
    }
    return nearbyCount;
}

static s32 friendlyTrafficPositionPenalty(const UnitController *units,
                                           const Unit *unit,
                                           const u8 *friendlyIndices,
                                           u8 friendlyCount,
                                           const Unit *goalUnit,
                                           const MissionObjective *objective,
                                           EnemyAITacticalRole role,
                                           u16 x, u16 y,
                                           bool retreatMode)
{
    const bool hasGoalUnit = (goalUnit != NULL) && goalUnit->alive;
    const bool hasObjective = objective != NULL;
    const u16 goalX = hasGoalUnit ? goalUnit->x :
        (hasObjective ? objective->x : x);
    const u16 goalY = hasGoalUnit ? goalUnit->y :
        (hasObjective ? objective->y : y);
    const bool hasGoal = hasGoalUnit || hasObjective;
    s32 adjacencyPenalty;
    s32 score = 0;
    u8 i;

    if ((units == NULL) || (unit == NULL) || (friendlyIndices == NULL) ||
        (role == AI_ROLE_OBJECTIVE_RUNNER))
        return 0;

    /* El corredor no se desvía por mantener espacio: su prioridad es el punto. */
    switch (role)
    {
        case AI_ROLE_GUARD:
        case AI_ROLE_SCREEN:
            adjacencyPenalty = 2;
            break;
        case AI_ROLE_ESCORT:
            adjacencyPenalty = 3;
            break;
        case AI_ROLE_SUPPORT:
        case AI_ROLE_FLANK:
            adjacencyPenalty = 3;
            break;
        case AI_ROLE_ASSAULT:
        default:
            adjacencyPenalty = AI_ALLY_ADJACENCY_PENALTY;
            break;
    }

    for (i = 0; i < friendlyCount; i++)
    {
        const Unit *ally = UnitController_getUnit(units, friendlyIndices[i]);
        const u16 allyDistance = (ally != NULL)
            ? distanceBetween(x, y, ally->x, ally->y) : 0xFFFFu;

        if ((ally == NULL) || !ally->alive || (ally->team != unit->team))
            continue;

        /* Una casilla libre pegada a una aliada suele ser una mala posición
           de apoyo si existe una opción igual de buena más despejada. */
        if (allyDistance == 1u)
            score += adjacencyPenalty;
        else if ((allyDistance == 2u) &&
                 ((role == AI_ROLE_SUPPORT) || (role == AI_ROLE_FLANK)))
            score += 1;

        /* Evita colocarse en el siguiente paso natural de una compañera. Se
           aplica solo como preferencia: un paso estrecho sigue siendo viable
           si no hay otro destino alcanzable, y el corredor de misión tiene
           prioridad para entrar en el objetivo. */
        if (!retreatMode && hasGoal && (allyDistance == 1u) &&
            (role != AI_ROLE_GUARD) && (role != AI_ROLE_SCREEN))
        {
            const u16 candidateGoalDistance = distanceBetween(
                x, y, goalX, goalY);
            const u16 allyGoalDistance = distanceBetween(
                ally->x, ally->y, goalX, goalY);

            if ((allyGoalDistance > 0u) &&
                ((u32)candidateGoalDistance + 1u == allyGoalDistance))
            {
                score += (role == AI_ROLE_ESCORT)
                    ? (AI_ALLY_ROUTE_BLOCK_PENALTY / 2)
                    : AI_ALLY_ROUTE_BLOCK_PENALTY;
            }
        }
    }

    return score;
}

/* Selección de objetivos visibles, foco y lógica de retirada. */

static u8 collectVisibleEnemyIndices(const UnitController *units,
                                    const MapData *map, TeamId team,
                                    u8 *indices)
{
    const u8 count = UnitController_getCount(units);
    const u8 limit = (count < AI_MAX_VISIBLE_ENEMIES)
        ? count : AI_MAX_VISIBLE_ENEMIES;
    u8 visibleCount = 0;
    u8 i;

    for (i = 0; i < limit; i++)
    {
        const Unit *enemy = UnitController_getUnit(units, i);
        if ((enemy == NULL) || !enemy->alive || (enemy->team == team) ||
            !isVisibleToAI(map, enemy, team))
            continue;
        indices[visibleCount++] = i;
    }
    return visibleCount;
}

static void refreshFocusTarget(EnemyAI *ai, const UnitController *units,
                               const MapData *map, TeamId team)
{
    const Unit *target;

    if ((ai == NULL) || (units == NULL) || (map == NULL) ||
        (ai->focusTargetIndex < 0))
        return;

    target = UnitController_getUnit(units, (u8)ai->focusTargetIndex);
    if ((target == NULL) || !target->alive || (target->team == team) ||
        !isVisibleToAI(map, target, team))
        ai->focusTargetIndex = -1;
}

static u16 closestVisibleEnemyDistance(const UnitController *units,
                                       const u8 *visibleEnemyIndices,
                                       u8 visibleEnemyCount, u16 x, u16 y)
{
    u16 closest = 0xFFFFu;
    u8 i;

    for (i = 0; i < visibleEnemyCount; i++)
    {
        const Unit *enemy = UnitController_getUnit(units, visibleEnemyIndices[i]);
        const u16 distance = distanceBetween(x, y, enemy->x, enemy->y);
        if (distance < closest) closest = distance;
    }
    return closest;
}

static s32 retreatPositionScore(const UnitController *units, const MapData *map,
                                const Unit *unit, const MissionObjective *objective,
                                const Unit *runner, EnemyAITacticalRole role,
                                const u8 *visibleEnemyIndices, u8 visibleEnemyCount,
                                const u8 *riskEnemyIndices, u8 riskEnemyCount,
                                u16 x, u16 y)
{
    const s32 risk = estimatePositionRisk(units, riskEnemyIndices,
        riskEnemyCount, map, unit, x, y);
    const u16 nearest = closestVisibleEnemyDistance(units, visibleEnemyIndices,
        visibleEnemyCount, x, y);
    const u16 objectiveDistance = distanceBetween(x, y, objective->x, objective->y);
    s32 score = risk * 24;

    /* En empate de riesgo, crea distancia y aprovecha cobertura del terreno. */
    if (nearest != 0xFFFFu) score -= (s32)nearest * 3;
    score -= (s32)terrainDefenseAt(map, x, y) * 4;
    score += classTerrainPositionPenalty(unit, map, x, y);

    /* La retirada protege a la unidad sin ordenar a los guardias que abandonen
       completamente el objetivo ni a los escoltas que dejen aislado al corredor. */
    if ((role == AI_ROLE_GUARD) && (objectiveDistance > 2u))
        score += (s32)(objectiveDistance - 2u) * 10;
    else if ((role == AI_ROLE_SCREEN) && (objectiveDistance > 5u))
        score += (s32)(objectiveDistance - 5u) * 6;
    if ((role == AI_ROLE_ESCORT) && (runner != NULL) && runner->alive)
        score += (s32)distanceBetween(x, y, runner->x, runner->y) * 2;
    return score;
}

static bool shouldRetreatFromThreat(const UnitController *units, const MapData *map,
                                    const Unit *unit, const Unit *attackTarget,
                                    TeamId team,
                                    const u8 *visibleEnemyIndices,
                                    u8 visibleEnemyCount)
{
    u8 riskEnemyIndices[AI_MAX_VISIBLE_ENEMIES];
    u8 riskEnemyCount = 0;
    u8 i;
    s32 risk;
    CombatResult prediction;

    /* La gran mayoría de unidades no necesita evaluar una retirada. Evita
       construir listas visibles, comprobar amenazas y lanzar LOS para cada
       unidad sana; antes todo ese trabajo se hacía antes de mirar su salud. */
    if ((unit == NULL) || (unit->maxHealth == 0) ||
        ((u16)unit->health * 3u > unit->maxHealth))
        return FALSE;

    /* Para riesgo en la casilla actual, los enemigos fuera de su alcance no
       pueden contribuir. Filtra una vez en lugar de repetir distancia/LOS. */
    for (i = 0; i < visibleEnemyCount; i++)
    {
        const Unit *enemy = UnitController_getUnit(units, visibleEnemyIndices[i]);
        if (distanceBetween(unit->x, unit->y, enemy->x, enemy->y) <=
            enemy->attackRange)
            riskEnemyIndices[riskEnemyCount++] = visibleEnemyIndices[i];
    }
    risk = estimatePositionRisk(units, riskEnemyIndices, riskEnemyCount,
                                map, unit, unit->x, unit->y);

    if (risk == 0) return FALSE;

    /* No huye de una baja segura que pueda conseguir antes de recibir daño. */
    if (attackTarget != NULL)
    {
        prediction = Combat_predictUnits(units, unit, attackTarget, team);
        if ((prediction.code == COMBAT_RESULT_ATTACKED) &&
            prediction.defenderDefeated)
            return FALSE;
        if ((prediction.code == COMBAT_RESULT_ATTACKED) &&
            (prediction.counterDamage >= unit->health))
            return TRUE;
    }

    return (risk >= unit->health) ||
        (((u16)unit->health * 2u <= unit->maxHealth) && (risk >= 2));
}

/* Selección de destino dentro del rectángulo alcanzable. */

static bool selectBestDestination(const EnemyAI *ai, UnitController *units,
                                  MapData *map, const Unit *unit,
                                  const Unit *target, const Unit *repairApproachTarget,
                                  const MissionObjective *objective,
                                  TeamId objectiveOwner, bool retreatMode,
                                  const u8 *visibleEnemyIndices,
                                  u8 visibleEnemyCount,
                                  u16 *destinationX, u16 *destinationY)
{
    const u8 *reachable = UnitController_getMovementRange(units);
    const PathfinderWorkspace *workspace = map->pathWorkspace;
    const u16 radius = unit->movementPoints;
    const u16 minX = (unit->x > radius) ? (u16)(unit->x - radius) : 0;
    const u16 minY = (unit->y > radius) ? (u16)(unit->y - radius) : 0;
    const u32 candidateMaxX = (u32)unit->x + radius;
    const u32 candidateMaxY = (u32)unit->y + radius;
    const u16 maxX = (candidateMaxX < map->width)
        ? (u16)candidateMaxX : (u16)(map->width - 1u);
    const u16 maxY = (candidateMaxY < map->height)
        ? (u16)candidateMaxY : (u16)(map->height - 1u);
    const u8 unitIndex = (u8)(unit - units->units);
    const EnemyAITacticalRole role = EnemyAI_getTacticalRole(ai, unitIndex);
    const Unit *trafficGoalUnit = (target != NULL) ? target : repairApproachTarget;
    const Unit *runner;
    u16 goalX;
    u16 goalY;
    u16 bestGoalDistance;
    s32 currentBase;
    s32 currentStrategic;
    u8 riskWeight;
    u8 riskEnemyIndices[AI_MAX_VISIBLE_ENEMIES];
    u8 riskEnemyCount = 0;
    u8 friendlyIndices[ENEMY_AI_MAX_UNITS];
    u8 friendlyCount;
    s32 bestScore;
    u16 bestCost = 0xFFFFu;
    s32 bestStrategic;
    u16 y;
    bool found = FALSE;
    const bool prioritizeRepair = !retreatMode && (target == NULL) &&
        (repairApproachTarget != NULL) && (unit->repairAmount > 0) &&
        (unit->repairRange > 0) && !unit->hasAttackedThisTurn &&
        !unit->hasRepairedThisTurn;

    if ((reachable == NULL) || (workspace == NULL) ||
        (workspace->costs == NULL) || (map->width == 0) ||
        (map->height == 0))
        return FALSE;

    goalX = ((trafficGoalUnit != NULL) && trafficGoalUnit->alive)
        ? trafficGoalUnit->x : objective->x;
    goalY = ((trafficGoalUnit != NULL) && trafficGoalUnit->alive)
        ? trafficGoalUnit->y : objective->y;
    bestGoalDistance = distanceBetween(unit->x, unit->y, goalX, goalY);

    runner = (ai->objectiveRunnerIndex >= 0)
        ? UnitController_getUnit(units, (u8)ai->objectiveRunnerIndex) : NULL;
    if ((runner != NULL) && !runner->alive) runner = NULL;
    friendlyCount = collectNearbyFriendlyIndices(units, unit, radius,
                                                  friendlyIndices);

    /* Si no hay enemigo visible, el ingeniero puede aproximarse al aliado que
       más necesita reparación. La prioridad se calcula una vez por decisión. */
    currentBase = prioritizeRepair
        ? repairApproachPositionScore(unit, repairApproachTarget, unit->x, unit->y)
        : strategicPositionScore(unit->x, unit->y, unit, target,
                                 objective, objectiveOwner, ai->team);
    currentBase += friendlyTrafficPositionPenalty(units, unit,
        friendlyIndices, friendlyCount,
        (target != NULL) ? target : repairApproachTarget, objective, role,
        unit->x, unit->y, retreatMode);
    currentStrategic = prioritizeRepair
        ? currentBase
        : applyRolePositionScore(currentBase, unit->x, unit->y,
            unit, target, runner, objective, objectiveOwner, role,
            (EnemyAITacticalPlan)ai->tacticalPlan, map, ai->team);
    if (!mustPrioritizeObjective(unit, objective, objectiveOwner, role, ai->team))
    {
        currentStrategic += classTerrainPositionPenalty(unit, map,
                                                         unit->x, unit->y);
        currentStrategic += firingLanePositionPenalty(map, unit, target,
                                                       unit->x, unit->y);
    }
    bestStrategic = currentStrategic;
    riskWeight = ((u16)unit->health * 3u <= unit->maxHealth) ? 3u : 1u;

    /* Un destino alcanzable está a distancia Manhattan <= movementPoints.
       Un enemigo más lejos que ese radio + su alcance no puede amenazar ninguna
       casilla candidata. El filtro es exacto (todos los costes son >= 1) y
       evita repetir decenas de miles de comprobaciones distancia/LOS por turno. */
    {
        u8 enemySlot;
        for (enemySlot = 0; enemySlot < visibleEnemyCount; enemySlot++)
        {
            const u8 enemyIndex = visibleEnemyIndices[enemySlot];
            const Unit *enemy = UnitController_getUnit(units, enemyIndex);
            if (distanceBetween(unit->x, unit->y, enemy->x, enemy->y) <=
                (u16)(radius + enemy->attackRange))
                riskEnemyIndices[riskEnemyCount++] = enemyIndex;
        }
    }

    if (retreatMode)
        bestScore = retreatPositionScore(units, map, unit, objective, runner, role,
            visibleEnemyIndices, visibleEnemyCount, riskEnemyIndices,
            riskEnemyCount, unit->x, unit->y) +
            friendlyTrafficPositionPenalty(units, unit, friendlyIndices,
                friendlyCount, target, objective, role,
                unit->x, unit->y, TRUE);
    else
        bestScore = currentStrategic +
            (s32)estimatePositionRisk(units, riskEnemyIndices,
                riskEnemyCount, map, unit, unit->x, unit->y) * riskWeight -
            (s32)terrainDefenseAt(map, unit->x, unit->y) * AI_DEFENSE_SCORE_WEIGHT;

    /* Solo explora la caja de movimiento; cada casilla se valora una vez. */
    for (y = minY; y <= maxY; y++)
    {
        u16 x;
        for (x = minX; x <= maxX; x++)
        {
            const u32 index = (u32)y * map->width + x;
            s32 baseStrategic;
            s32 strategic;
            s32 score;
            u16 cost;
            u16 goalDistance;
            bool better;

            /* No puntúes ni evalúes amenazas para terreno que no se puede
               alcanzar. Antes se ejecutaban estos cálculos antes del filtro,
               incluyendo la puntuación táctica específica del rol. */
            if (!reachable[index] || ((x == unit->x) && (y == unit->y))) continue;

            baseStrategic = prioritizeRepair
                ? repairApproachPositionScore(unit, repairApproachTarget, x, y)
                : strategicPositionScore(x, y, unit, target,
                                         objective, objectiveOwner, ai->team);
            baseStrategic += friendlyTrafficPositionPenalty(units, unit,
                friendlyIndices, friendlyCount,
                (target != NULL) ? target : repairApproachTarget, objective,
                role, x, y, retreatMode);
            if (!retreatMode &&
                (baseStrategic > currentBase + AI_RISK_LOOKAHEAD_MARGIN))
                continue;

            strategic = prioritizeRepair
                ? baseStrategic
                : applyRolePositionScore(baseStrategic,
                    x, y, unit, target, runner, objective, objectiveOwner, role,
                    (EnemyAITacticalPlan)ai->tacticalPlan, map, ai->team);
            if (!mustPrioritizeObjective(unit, objective, objectiveOwner, role, ai->team))
            {
                strategic += classTerrainPositionPenalty(unit, map, x, y);
                strategic += firingLanePositionPenalty(map, unit, target, x, y);
            }
            Performance_count(PERF_EVENT_AI_CANDIDATES, 1);
            cost = workspace->costs[index];
            goalDistance = distanceBetween(x, y, goalX, goalY);
            if (retreatMode)
            {
                score = retreatPositionScore(units, map, unit, objective, runner,
                    role, visibleEnemyIndices, visibleEnemyCount, riskEnemyIndices,
                    riskEnemyCount, x, y) +
                    friendlyTrafficPositionPenalty(units, unit, friendlyIndices,
                        friendlyCount, target, objective, role, x, y, TRUE);
                better = (score < bestScore);
            }
            else
            {
                const s32 scoreFloor = strategic -
                    (s32)terrainDefenseAt(map, x, y) * AI_DEFENSE_SCORE_WEIGHT;

                /* El riesgo nunca es negativo. Si incluso la puntuación ideal
                   supera la mejor actual, no calcules amenazas/LOS para esta
                   casilla: ese coste no puede cambiar la decisión. El '>'
                   estricto conserva los desempates por estrategia y coste. */
                if (scoreFloor > bestScore) continue;

                score = scoreFloor +
                    (s32)estimatePositionRisk(units, riskEnemyIndices,
                        riskEnemyCount, map, unit, x, y) * riskWeight;
                better = (score < bestScore) ||
                    ((score == bestScore) && (strategic < bestStrategic)) ||
                    (found && (score == bestScore) &&
                     (strategic == bestStrategic) &&
                     (goalDistance < bestGoalDistance)) ||
                    (found && (score == bestScore) &&
                     (strategic == bestStrategic) &&
                     (goalDistance == bestGoalDistance) && (cost < bestCost));
            }

            if (better)
            {
                found = TRUE;
                bestScore = score;
                bestStrategic = strategic;
                bestCost = cost;
                bestGoalDistance = goalDistance;
                *destinationX = x;
                *destinationY = y;
            }
        }
    }
    return found;
}

/* El ingeniero prioriza al aliado adyacente con mayor daño proporcional. */
static const Unit *findBestRepairTarget(const UnitController *units,
                                        const Unit *engineer, TeamId team)
{
    const Unit *best = NULL;
    u16 bestMissingPercent = 0;
    u8 bestMissingHealth = 0;
    u8 i;

    if ((units == NULL) || (engineer == NULL) ||
        (engineer->repairAmount == 0) || (engineer->repairRange == 0) ||
        engineer->hasAttackedThisTurn || engineer->hasRepairedThisTurn)
        return NULL;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *ally = UnitController_getUnit(units, i);
        RepairResult preview;
        u8 missingHealth;
        u16 missingPercent;

        if ((ally == NULL) || !ally->alive || (ally->team != engineer->team) ||
            (ally->id == engineer->id) || (ally->health >= ally->maxHealth) ||
            (ally->maxHealth == 0))
            continue;

        preview = Combat_predictRepairUnits(units, engineer, ally, team);
        if (preview.code != REPAIR_RESULT_CAN_REPAIR) continue;

        missingHealth = (u8)(ally->maxHealth - ally->health);
        missingPercent = (u16)(((u16)missingHealth * 100u) / ally->maxHealth);
        if ((best == NULL) || (missingPercent > bestMissingPercent) ||
            ((missingPercent == bestMissingPercent) &&
             (missingHealth > bestMissingHealth)))
        {
            best = ally;
            bestMissingPercent = missingPercent;
            bestMissingHealth = missingHealth;
        }
    }

    return best;
}

/*
 * Rechaza contextos incoherentes antes de tocar el estado de la IA. El mapa
 * tiene que ser el mismo que usa UnitController y el bando consultado debe ser
 * el que tiene el turno; de otro modo la IA podría avanzar su índice de actor
 * aunque el controlador vaya a rechazar todas sus órdenes.
 */
static bool hasValidUpdateContext(const EnemyAI *ai,
                                  const UnitController *units,
                                  const MapData *map, TeamId activeTeam,
                                  const MissionObjective *objective)
{
    if ((ai == NULL) || (units == NULL) || (map == NULL) ||
        (objective == NULL))
        return FALSE;

    if ((activeTeam != TEAM_RED) && (activeTeam != TEAM_BLUE))
        return FALSE;

    if ((units->map != map) ||
        (UnitController_getActiveTeam(units) != activeTeam))
        return FALSE;

    return (objective->x < map->width) && (objective->y < map->height);
}

/* API pública de IA: una acción determinista por llamada. */

void EnemyAI_init(EnemyAI *ai)
{
    u8 i;
    if (ai == NULL) return;
    ai->actorIndex = -1;
    ai->focusUnitIndex = -1;
    ai->objectiveRunnerIndex = -1;
    ai->focusTargetIndex = -1;
    ai->nextUnitIndex = 0;
    ai->lastObjectiveOwner = (u8)TEAM_NONE;
    ai->tacticalPlan = (u8)AI_PLAN_OBJECTIVE_ASSAULT;
    ai->turnNumber = 0;
    ai->team = TEAM_RED;
    ai->initialized = FALSE;
    ai->retreating = FALSE;
    for (i = 0; i < ENEMY_AI_MAX_UNITS; i++)
        ai->tacticalRoles[i] = (u8)AI_ROLE_ASSAULT;
}

s16 EnemyAI_getFocusUnitIndex(const EnemyAI *ai)
{
    return (ai != NULL) ? ai->focusUnitIndex : -1;
}

s16 EnemyAI_getFocusTargetIndex(const EnemyAI *ai)
{
    return (ai != NULL) ? ai->focusTargetIndex : -1;
}

s16 EnemyAI_getNextActorIndex(const EnemyAI *ai,
                              const UnitController *units,
                              u16 turnNumber)
{
    u8 firstIndex;
    u8 i;
    const u8 count = UnitController_getCount(units);

    if ((ai == NULL) || (units == NULL)) return -1;
    if (!ai->initialized || (ai->turnNumber != turnNumber))
        firstIndex = 0;
    else if (ai->actorIndex >= 0)
    {
        const Unit *current = UnitController_getUnit(units,
                                                      (u8)ai->actorIndex);
        if ((current != NULL) && current->alive &&
            (current->team == ai->team))
            return ai->actorIndex;
        firstIndex = ai->nextUnitIndex;
    }
    else firstIndex = ai->nextUnitIndex;

    for (i = firstIndex; i < count; i++)
    {
        const Unit *candidate = UnitController_getUnit(units, i);
        if ((candidate != NULL) && candidate->alive &&
            (candidate->team == ai->team))
            return (s16)i;
    }
    return -1;
}

EnemyAIResult EnemyAI_update(EnemyAI *ai, UnitController *units,
                             MapData *map, TeamId activeTeam,
                             u16 turnNumber, const MissionObjective *objective,
                             CombatResult *combatResult)
{
    const Unit *unit;
    const Unit *target;
    const Unit *attackTarget;
    const Unit *repairApproachTarget;
    const Unit *runner;
    TeamId objectiveOwner;
    EnemyAITacticalRole role;
    u8 count;
    u8 visibleEnemyIndices[AI_MAX_VISIBLE_ENEMIES];
    u8 visibleEnemyCount;
    u16 destinationX;
    u16 destinationY;
    UnitActionResult action;

    if (!hasValidUpdateContext(ai, units, map, activeTeam, objective) ||
        UnitController_isMoving(units))
        return ENEMY_AI_IDLE;

    /* La misma lógica táctica puede dirigir cualquiera de los dos bandos. */
    ai->team = activeTeam;
    count = UnitController_getCount(units);
    if (!ai->initialized || (ai->turnNumber != turnNumber))
    {
        ai->initialized = TRUE;
        ai->turnNumber = turnNumber;
        ai->nextUnitIndex = 0;
        ai->actorIndex = -1;
        ai->focusUnitIndex = -1;
        ai->focusTargetIndex = -1;
        ai->retreating = FALSE;
        objectiveOwner = getObjectiveOwner(units, map, objective);
        ai->tacticalPlan = (u8)resolveTacticalPlan(objective, objectiveOwner, activeTeam);
        assignTacticalRoles(ai, units, objective, objectiveOwner, activeTeam);
        ai->lastObjectiveOwner = (u8)objectiveOwner;
    }

    if (ai->actorIndex < 0)
    {
        while (ai->nextUnitIndex < count)
        {
            const u8 index = ai->nextUnitIndex++;
            const Unit *candidate = UnitController_getUnit(units, index);
            if ((candidate != NULL) && candidate->alive &&
                (candidate->team == ai->team))
            {
                ai->actorIndex = index;
                ai->focusUnitIndex = index;
                break;
            }
        }
        if (ai->actorIndex < 0) return ENEMY_AI_TURN_FINISHED;
    }

    unit = UnitController_getUnit(units, (u8)ai->actorIndex);
    if ((unit == NULL) || !unit->alive || (unit->team != activeTeam))
    {
        ai->actorIndex = -1;
        ai->retreating = FALSE;
        return ENEMY_AI_UNIT_FINISHED;
    }

    /* Tras la animación de retirada, la unidad no vuelve a entrar en combate
       ese turno: así no deshacemos tácticamente la orden de salvarla. */
    if (ai->retreating)
    {
        ai->retreating = FALSE;
        ai->actorIndex = -1;
        (void)UnitController_cancelOrder(units);
        return ENEMY_AI_UNIT_FINISHED;
    }

    objectiveOwner = getObjectiveOwner(units, map, objective);
    {
        const EnemyAITacticalPlan updatedPlan = resolveTacticalPlan(
            objective, objectiveOwner, activeTeam);
        if ((ai->lastObjectiveOwner != (u8)objectiveOwner) ||
            (ai->tacticalPlan != (u8)updatedPlan))
        {
            /* Cambiar el dueño o el modo de misión recompone los grupos en la
               siguiente decisión, sin esperar al siguiente turno. */
            assignTacticalRoles(ai, units, objective, objectiveOwner, activeTeam);
            ai->lastObjectiveOwner = (u8)objectiveOwner;
            ai->tacticalPlan = (u8)updatedPlan;
        }
    }
    refreshFocusTarget(ai, units, map, activeTeam);
    role = EnemyAI_getTacticalRole(ai, (u8)ai->actorIndex);
    runner = (ai->objectiveRunnerIndex >= 0)
        ? UnitController_getUnit(units, (u8)ai->objectiveRunnerIndex) : NULL;
    if ((runner != NULL) && !runner->alive) runner = NULL;

    /* Construye la lista visible una sola vez por decisión. Antes se volvía a
       inspeccionar el registro completo de 120 unidades por separado para
       atacar, elegir blanco, evaluar retirada y puntuar destinos. */
    visibleEnemyCount = collectVisibleEnemyIndices(units, map, activeTeam,
                                                    visibleEnemyIndices);
    attackTarget = findBestAttackTarget(units, unit, objective, role,
        runner, ai->focusTargetIndex, activeTeam,
        visibleEnemyIndices, visibleEnemyCount);

    /* Una unidad críticamente debilitada intenta ponerse a cubierto antes de
       atacar si la represalia o el fuego conocido puede eliminarla. */
    if ((unit->movementPoints > 0) &&
        shouldRetreatFromThreat(units, map, unit, attackTarget, activeTeam,
                                visibleEnemyIndices, visibleEnemyCount))
    {
        bool selectedForRetreat =
            (UnitController_getSelectedIndex(units) == ai->actorIndex);

        if (!selectedForRetreat)
        {
            action = UnitController_activate(units, unit->x, unit->y);
            selectedForRetreat = (action == UNIT_ACTION_SELECTED);
        }

        if (selectedForRetreat &&
            selectBestDestination(ai, units, map, unit, attackTarget, NULL, objective,
                                  objectiveOwner, TRUE, visibleEnemyIndices,
                                  visibleEnemyCount, &destinationX, &destinationY))
        {
            action = UnitController_activate(units, destinationX, destinationY);
            if (action == UNIT_ACTION_ROUTE_READY)
                action = UnitController_activate(units, destinationX, destinationY);
            if (action == UNIT_ACTION_MOVING)
            {
                ai->retreating = TRUE;
                return ENEMY_AI_RETREAT_STARTED;
            }
        }

        if (UnitController_getSelectedIndex(units) == ai->actorIndex)
            (void)UnitController_cancelOrder(units);
    }

    if (attackTarget != NULL)
    {
        const s16 targetIndex = (s16)(attackTarget - units->units);
        const CombatResult actual = Combat_attack(units, unit->id,
                                                   attackTarget->id, activeTeam);
        if (combatResult != NULL) *combatResult = actual;

        /* Si el blanco sobrevive, la siguiente unidad intenta completar la
           eliminación. Si cae, se libera el foco para reevaluar amenazas. */
        if ((actual.code == COMBAT_RESULT_ATTACKED) && attackTarget->alive)
            ai->focusTargetIndex = targetIndex;
        else if (ai->focusTargetIndex == targetIndex)
            ai->focusTargetIndex = -1;

        ai->actorIndex = -1;
        return ENEMY_AI_ATTACK_RESOLVED;
    }

    /* Tras descartar ataques validos, un ingeniero puede gastar su accion en
       reparar al aliado adyacente mas necesitado, incluso si ya no puede moverse. */
    if (unit->repairAmount > 0)
    {
        const Unit *repairTarget = findBestRepairTarget(units, unit, activeTeam);
        if (repairTarget != NULL)
        {
            const RepairResult repair = Combat_repair(
                units, unit->id, repairTarget->id, activeTeam);
            if (repair.code == REPAIR_RESULT_REPAIRED)
            {
                ai->actorIndex = -1;
                return ENEMY_AI_REPAIR_RESOLVED;
            }
        }
    }

    if (unit->movementPoints == 0)
    {
        ai->actorIndex = -1;
        return ENEMY_AI_UNIT_FINISHED;
    }

    target = findBestVisibleTarget(units, unit, objective, role, runner,
                                   ai->focusTargetIndex,
                                   (EnemyAITacticalPlan)ai->tacticalPlan,
                                   visibleEnemyIndices, visibleEnemyCount);
    repairApproachTarget = ((target == NULL) &&
        (unit->repairAmount > 0) && (unit->repairRange > 0))
        ? findBestRepairApproachTarget(units, unit) : NULL;
    if ((target != NULL) && (ai->focusTargetIndex < 0) &&
        (role != AI_ROLE_OBJECTIVE_RUNNER) &&
        ((role != AI_ROLE_GUARD) ||
         (distanceBetween(target->x, target->y, objective->x, objective->y) <=
          AI_OBJECTIVE_DEFENSE_RADIUS)))
        ai->focusTargetIndex = (s16)(target - units->units);

    if (UnitController_getSelectedIndex(units) != ai->actorIndex)
    {
        action = UnitController_activate(units, unit->x, unit->y);
        if (action != UNIT_ACTION_SELECTED)
        {
            ai->actorIndex = -1;
            return ENEMY_AI_UNIT_FINISHED;
        }
    }

    if (!selectBestDestination(ai, units, map, unit, target,
                               repairApproachTarget, objective,
                               objectiveOwner, FALSE, visibleEnemyIndices,
                               visibleEnemyCount, &destinationX, &destinationY))
    {
        (void)UnitController_cancelOrder(units);
        ai->actorIndex = -1;
        return ENEMY_AI_UNIT_FINISHED;
    }

    action = UnitController_activate(units, destinationX, destinationY);
    if (action == UNIT_ACTION_ROUTE_READY)
        action = UnitController_activate(units, destinationX, destinationY);

    if (action == UNIT_ACTION_MOVING) return ENEMY_AI_MOVE_STARTED;

    (void)UnitController_cancelOrder(units);
    ai->actorIndex = -1;
    return ENEMY_AI_UNIT_FINISHED;
}
