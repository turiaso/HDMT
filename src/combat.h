#ifndef COMBAT_H
#define COMBAT_H

#include <genesis.h>
#include "team.h"
#include "unit_controller.h"

typedef enum
{
    COMBAT_RESULT_ATTACKED,
    COMBAT_RESULT_INVALID,
    COMBAT_RESULT_NOT_ENEMY,
    COMBAT_RESULT_WRONG_TEAM,
    COMBAT_RESULT_OUT_OF_RANGE,
    COMBAT_RESULT_ALREADY_ACTED,
    COMBAT_RESULT_NOT_VISIBLE,
    COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT
} CombatResultCode;

typedef struct
{
    CombatResultCode code;
    u8 damageDealt;
    u8 counterDamage;
    bool attackerDefeated;
    bool defenderDefeated;
} CombatResult;

/* Valores de la máscara de objetivos consumida por MovementGlow. */
typedef enum
{
    COMBAT_TARGET_NONE = 0,
    COMBAT_TARGET_ATTACKABLE,
    COMBAT_TARGET_REPAIRABLE
} CombatTargetMaskValue;

typedef enum
{
    REPAIR_RESULT_CAN_REPAIR,
    REPAIR_RESULT_REPAIRED,
    REPAIR_RESULT_INVALID,
    REPAIR_RESULT_WRONG_TEAM,
    REPAIR_RESULT_NOT_ENGINEER,
    REPAIR_RESULT_NOT_ALLY,
    REPAIR_RESULT_OUT_OF_RANGE,
    REPAIR_RESULT_TARGET_FULL_HEALTH,
    REPAIR_RESULT_ALREADY_ACTED
} RepairResultCode;

typedef struct
{
    RepairResultCode code;
    u8 healthRestored;
} RepairResult;

/* Ataque Manhattan; bosque/roca bloquean LOS. Atacar y reparar comparten acción. */
CombatResult Combat_attack(UnitController *units, u8 attackerId,
                           u8 defenderId, TeamId activeTeam);
/* Predicción sin mutar unidades ni consumir su acción. Incluye cobertura y las
   banderas previstas de baja del atacante/defensor. */
CombatResult Combat_predict(const UnitController *units, u8 attackerId,
                            u8 defenderId, TeamId activeTeam);
CombatResult Combat_predictUnits(const UnitController *units,
                                 const Unit *attacker, const Unit *defender,
                                 TeamId activeTeam);
/* Construye, fuera del bucle por fotograma, una máscara con las celdas ocupadas
   por enemigos que este atacante puede golpear ahora. Reutiliza la validación
   real del combate (turno, acción, alcance, visibilidad y línea de visión).
   maskCapacity se expresa en bytes; se limpia por completo al construirla. */
bool Combat_buildAttackableMask(const UnitController *units,
                                const Unit *attacker, TeamId activeTeam,
                                u8 *mask, u16 maskCapacity);
/* Predice una reparacion sin mutar unidades. Si es valida, healthRestored contiene
   los PV que recuperaria el aliado (limitados por su vida maxima). El objetivo
   debe ser un aliado herido y dentro del alcance; reparar consume la misma accion
   que atacar. */
RepairResult Combat_predictRepairUnits(const UnitController *units,
                                       const Unit *engineer, const Unit *ally,
                                       TeamId activeTeam);
RepairResult Combat_repair(UnitController *units, u8 engineerId, u8 allyId,
                           TeamId activeTeam);
/* Una pasada construye destinos de ataque (COMBAT_TARGET_ATTACKABLE) y
   reparacion (COMBAT_TARGET_REPAIRABLE), reutilizando el mismo buffer de mapa. */
bool Combat_buildActionTargetMask(const UnitController *units,
                                  const Unit *actor, TeamId activeTeam,
                                  u8 *mask, u16 maskCapacity);

#endif
