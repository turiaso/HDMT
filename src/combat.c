#include "combat.h" /* Define los tipos SGDK antes de incluir string.h. */
#include <string.h>

/*
 * Reglas autoritativas de combate. Combat_predict*() no muta el juego y Combat_attack()
 * aplica el resultado de esa misma predicción para mantener HUD, jugador e IA coherentes.
 */

static u16 cellDistance(const Unit *a, const Unit *b)
{
    const u16 dx = (a->x > b->x) ? (u16)(a->x - b->x) : (u16)(b->x - a->x);
    const u16 dy = (a->y > b->y) ? (u16)(a->y - b->y) : (u16)(b->y - a->y);
    return (u16)(dx + dy);
}

static u8 terrainDefenseBonus(const MapData *map, const Unit *unit)
{
    const CellData *cell;
    const TerrainInfo *terrain;

    if ((map == NULL) || (unit == NULL)) return 0;
    cell = MapData_getCellConst(map, unit->x, unit->y);
    if (cell == NULL) return 0;
    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    return (terrain != NULL) ? terrain->defenseBonus : 0;
}

static u8 calculateDamage(u8 attack, u8 defense, u8 terrainBonus)
{
    const u16 totalDefense = (u16)defense + terrainBonus;
    if (attack > totalDefense) return (u8)(attack - totalDefense);
    return 1;
}

static u8 clippedDamage(u8 damage, u8 health)
{
    return (damage >= health) ? health : damage;
}

static CombatResult resultWithCode(CombatResultCode code)
{
    CombatResult result;
    result.code = code;
    result.damageDealt = 0;
    result.counterDamage = 0;
    result.attackerDefeated = FALSE;
    result.defenderDefeated = FALSE;
    return result;
}

CombatResult Combat_predictUnits(const UnitController *units,
                                 const Unit *attacker, const Unit *defender,
                                 TeamId activeTeam)
{
    CombatResult result = resultWithCode(COMBAT_RESULT_INVALID);
    u16 distance;
    u8 damage;

    if ((units == NULL) || (attacker == NULL) || (defender == NULL) ||
        ((u8)activeTeam >= TEAM_COUNT) || (attacker->id == defender->id))
        return result;
    if (UnitController_getActiveTeam(units) != activeTeam)
    {
        result.code = COMBAT_RESULT_WRONG_TEAM;
        return result;
    }
    if (!attacker->alive || !defender->alive) return result;
    if (units->fogEnabled &&
        (MapData_getVisionState(units->map, (u8)activeTeam,
                                defender->x, defender->y) != MAP_VISION_VISIBLE))
    {
        result.code = COMBAT_RESULT_NOT_VISIBLE;
        return result;
    }
    if (attacker->team != activeTeam)
    {
        result.code = COMBAT_RESULT_WRONG_TEAM;
        return result;
    }
    if (attacker->team == defender->team)
    {
        result.code = COMBAT_RESULT_NOT_ENEMY;
        return result;
    }
    if (attacker->hasAttackedThisTurn || attacker->hasRepairedThisTurn)
    {
        result.code = COMBAT_RESULT_ALREADY_ACTED;
        return result;
    }

    distance = cellDistance(attacker, defender);
    if ((distance == 0) || (distance > attacker->attackRange))
    {
        result.code = COMBAT_RESULT_OUT_OF_RANGE;
        return result;
    }
    if (!MapData_hasLineOfSight(units->map, attacker->x, attacker->y,
                                defender->x, defender->y))
    {
        result.code = COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT;
        return result;
    }

    result.code = COMBAT_RESULT_ATTACKED;
    damage = calculateDamage(attacker->attack, defender->defense,
                             terrainDefenseBonus(units->map, defender));
    result.damageDealt = clippedDamage(damage, defender->health);
    result.defenderDefeated = damage >= defender->health;
    /* La predicción debe reflejar el resultado previsto, no solo el daño. */
    if (result.defenderDefeated) return result;

    if ((cellDistance(defender, attacker) <= defender->attackRange) &&
        MapData_hasLineOfSight(units->map, defender->x, defender->y,
                               attacker->x, attacker->y))
    {
        const u8 counter = calculateDamage(defender->attack, attacker->defense,
            terrainDefenseBonus(units->map, attacker));
        result.counterDamage = clippedDamage(counter, attacker->health);
        result.attackerDefeated = counter >= attacker->health;
    }
    return result;
}

bool Combat_buildAttackableMask(const UnitController *units,
                                const Unit *attacker, TeamId activeTeam,
                                u8 *mask, u16 maskCapacity)
{
    const MapData *map;
    u32 cellCount;
    u8 i;

    if ((units == NULL) || (units->map == NULL) || (mask == NULL))
        return FALSE;

    map = units->map;
    cellCount = (u32)map->width * map->height;
    if ((map->width == 0) || (map->height == 0) ||
        (cellCount > maskCapacity))
        return FALSE;

    memset(mask, 0, maskCapacity);
    if ((attacker == NULL) || !attacker->alive ||
        (attacker->team != activeTeam) ||
        (UnitController_getActiveTeam(units) != activeTeam))
        return TRUE;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *target = UnitController_getUnit(units, i);
        CombatResult preview;
        u32 index;

        if ((target == NULL) || !target->alive ||
            (target->team == attacker->team) ||
            (target->x >= map->width) || (target->y >= map->height))
            continue;

        /* La máscara solo necesita objetivos geométricamente alcanzables.
           Descarta primero unidades lejanas para no consultar visión ni LOS
           de todo el ejército al seleccionar una unidad. La validación final
           sigue siendo Combat_predictUnits(), compartida con el ataque real. */
        {
            const u16 distance = cellDistance(attacker, target);
            if ((distance == 0) || (distance > attacker->attackRange))
                continue;
        }

        preview = Combat_predictUnits(units, attacker, target, activeTeam);
        if (preview.code != COMBAT_RESULT_ATTACKED) continue;

        index = (u32)target->y * map->width + target->x;
        if (index < cellCount) mask[index] = 1;
    }

    return TRUE;
}


static RepairResult repairResultWithCode(RepairResultCode code)
{
    RepairResult result;
    result.code = code;
    result.healthRestored = 0;
    return result;
}

RepairResult Combat_predictRepairUnits(const UnitController *units,
                                       const Unit *engineer, const Unit *ally,
                                       TeamId activeTeam)
{
    RepairResult result;
    u8 missingHealth;

    if ((units == NULL) || (engineer == NULL) || (ally == NULL) ||
        ((u8)activeTeam >= TEAM_COUNT) || (engineer->id == ally->id) ||
        !engineer->alive || !ally->alive || (engineer->maxHealth == 0) ||
        (ally->maxHealth == 0))
        return repairResultWithCode(REPAIR_RESULT_INVALID);

    if ((UnitController_getActiveTeam(units) != activeTeam) ||
        (engineer->team != activeTeam))
        return repairResultWithCode(REPAIR_RESULT_WRONG_TEAM);

    if ((engineer->repairAmount == 0) || (engineer->repairRange == 0))
        return repairResultWithCode(REPAIR_RESULT_NOT_ENGINEER);

    if ((ally->team != engineer->team) || (ally->id == engineer->id))
        return repairResultWithCode(REPAIR_RESULT_NOT_ALLY);

    /* La accion es compartida: no se puede reparar despues de atacar, ni al reves. */
    if (engineer->hasAttackedThisTurn || engineer->hasRepairedThisTurn)
        return repairResultWithCode(REPAIR_RESULT_ALREADY_ACTED);

    if ((cellDistance(engineer, ally) == 0) ||
        (cellDistance(engineer, ally) > engineer->repairRange))
        return repairResultWithCode(REPAIR_RESULT_OUT_OF_RANGE);

    if (ally->health >= ally->maxHealth)
        return repairResultWithCode(REPAIR_RESULT_TARGET_FULL_HEALTH);

    /* La predicción informa también del efecto esperado, sin tocar las unidades. */
    missingHealth = (u8)(ally->maxHealth - ally->health);
    result = repairResultWithCode(REPAIR_RESULT_CAN_REPAIR);
    result.healthRestored = (engineer->repairAmount < missingHealth)
        ? engineer->repairAmount : missingHealth;
    return result;
}

RepairResult Combat_repair(UnitController *units, u8 engineerId, u8 allyId,
                           TeamId activeTeam)
{
    Unit *engineer;
    Unit *ally;
    RepairResult result;

    if (units == NULL) return repairResultWithCode(REPAIR_RESULT_INVALID);
    engineer = UnitController_getMutableUnitById(units, engineerId);
    ally = UnitController_getMutableUnitById(units, allyId);
    result = Combat_predictRepairUnits(units, engineer, ally, activeTeam);
    if (result.code != REPAIR_RESULT_CAN_REPAIR) return result;

    ally->health = (u8)(ally->health + result.healthRestored);
    engineer->hasRepairedThisTurn = TRUE;
    result.code = REPAIR_RESULT_REPAIRED;
    return result;
}

bool Combat_buildActionTargetMask(const UnitController *units,
                                  const Unit *actor, TeamId activeTeam,
                                  u8 *mask, u16 maskCapacity)
{
    const MapData *map;
    u32 cellCount;
    u8 i;

    if ((units == NULL) || (units->map == NULL) || (mask == NULL))
        return FALSE;

    map = units->map;
    cellCount = (u32)map->width * map->height;
    if ((map->width == 0) || (map->height == 0) ||
        (cellCount > maskCapacity))
        return FALSE;

    /* La funcion legacy calcula los blancos de ataque con la validacion real. */
    if (!Combat_buildAttackableMask(units, actor, activeTeam, mask, maskCapacity))
        return FALSE;

    if ((actor == NULL) || !actor->alive ||
        (actor->team != activeTeam) ||
        (actor->repairAmount == 0) || (actor->repairRange == 0) ||
        actor->hasAttackedThisTurn || actor->hasRepairedThisTurn ||
        (UnitController_getActiveTeam(units) != activeTeam))
        return TRUE;

    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *ally = UnitController_getUnit(units, i);
        RepairResult preview;
        u32 index;

        if ((ally == NULL) || !ally->alive || (ally->team != actor->team) ||
            (ally->id == actor->id) || (ally->x >= map->width) ||
            (ally->y >= map->height))
            continue;

        preview = Combat_predictRepairUnits(units, actor, ally, activeTeam);
        if (preview.code != REPAIR_RESULT_CAN_REPAIR) continue;

        index = (u32)ally->y * map->width + ally->x;
        if ((index < cellCount) && (mask[index] == COMBAT_TARGET_NONE))
            mask[index] = COMBAT_TARGET_REPAIRABLE;
    }

    return TRUE;
}

CombatResult Combat_predict(const UnitController *units, u8 attackerId,
                            u8 defenderId, TeamId activeTeam)
{
    const Unit *attacker = NULL;
    const Unit *defender = NULL;
    u8 i;

    if (units == NULL) return resultWithCode(COMBAT_RESULT_INVALID);
    for (i = 0; i < UnitController_getCount(units); i++)
    {
        const Unit *unit = UnitController_getUnit(units, i);
        if (unit == NULL) continue;
        if (unit->id == attackerId) attacker = unit;
        if (unit->id == defenderId) defender = unit;
    }
    return Combat_predictUnits(units, attacker, defender, activeTeam);
}

CombatResult Combat_attack(UnitController *units, u8 attackerId,
                           u8 defenderId, TeamId activeTeam)
{
    CombatResult result = Combat_predict(units, attackerId, defenderId, activeTeam);
    Unit *attacker;
    Unit *defender;

    if ((result.code != COMBAT_RESULT_ATTACKED) || (units == NULL)) return result;
    attacker = NULL;
    defender = NULL;
    attacker = UnitController_getMutableUnitById(units, attackerId);
    defender = UnitController_getMutableUnitById(units, defenderId);
    if ((attacker == NULL) || (defender == NULL))
        return resultWithCode(COMBAT_RESULT_INVALID);

    attacker->hasAttackedThisTurn = TRUE;
    defender->health = (result.damageDealt >= defender->health)
        ? 0 : (u8)(defender->health - result.damageDealt);
    result.defenderDefeated = defender->health == 0;
    if (result.defenderDefeated)
    {
        result.defenderDefeated = UnitController_removeUnit(units, defenderId);
        UnitController_invalidateMovementRange(units);
        return result;
    }

    if (result.counterDamage > 0)
    {
        attacker->health = (result.counterDamage >= attacker->health)
            ? 0 : (u8)(attacker->health - result.counterDamage);
        if (attacker->health == 0)
            result.attackerDefeated = UnitController_removeUnit(units, attackerId);
    }

    UnitController_invalidateMovementRange(units);
    return result;
}
