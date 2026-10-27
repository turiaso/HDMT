#ifndef ENEMY_AI_H
#define ENEMY_AI_H

#include <genesis.h>
#include "combat.h"
#include "game_turn.h"
#include "map_data.h"
#include "unit_controller.h"

#define ENEMY_AI_MAX_UNITS GAME_MAX_UNITS

typedef enum
{
    AI_ROLE_ASSAULT = 0,      /* Avanza y combate con libertad. */
    AI_ROLE_OBJECTIVE_RUNNER, /* Prioriza llegar/capturar el objetivo. */
    AI_ROLE_ESCORT,           /* Apoya al corredor y evita dejarlo aislado. */
    AI_ROLE_GUARD,            /* Mantiene la zona inmediata del objetivo. */
    AI_ROLE_FLANK,            /* Busca un ángulo lateral hacia el blanco. */
    AI_ROLE_SUPPORT,          /* Ocupa posiciones de tiro separadas. */
    AI_ROLE_SCREEN            /* Protege un perímetro alrededor del objetivo. */
} EnemyAITacticalRole;

typedef enum
{
    AI_PLAN_OBJECTIVE_ASSAULT = 0,
    AI_PLAN_OBJECTIVE_DEFENSE,
    AI_PLAN_OBJECTIVE_SECURED,
    AI_PLAN_ELIMINATION_SWEEP,
    AI_PLAN_SURVIVE_DEFENSE,
    AI_PLAN_SURVIVE_COUNTERATTACK
} EnemyAITacticalPlan;

typedef struct
{
    s16 actorIndex;       /* Unidad que puede continuar tras moverse. */
    s16 focusUnitIndex;   /* Última unidad roja que debe seguir la cámara. */
    s16 objectiveRunnerIndex; /* Unidad designada para llegar al objetivo. */
    s16 focusTargetIndex; /* Blanco coordinado para concentrar ataques y aproximación. */
    u8 nextUnitIndex;     /* Siguiente registro que se evaluará en este turno. */
    /* Un byte por unidad lógica; independiente del presupuesto de sprites. */
    u8 tacticalRoles[ENEMY_AI_MAX_UNITS];
    u8 lastObjectiveOwner; /* Detecta cambios de control para reasignar roles. */
    u8 tacticalPlan; /* Plan compacto; cambia con el objetivo/propietario. */
    u16 turnNumber;
    TeamId team; /* Equipo dirigido por esta instancia de IA. */
    bool initialized;
    bool retreating;      /* No ataca tras completar una retirada táctica. */
} EnemyAI;

typedef enum
{
    ENEMY_AI_IDLE = 0,
    ENEMY_AI_MOVE_STARTED,
    ENEMY_AI_RETREAT_STARTED,
    ENEMY_AI_ATTACK_RESOLVED,
    ENEMY_AI_REPAIR_RESOLVED,
    ENEMY_AI_UNIT_FINISHED,
    ENEMY_AI_TURN_FINISHED
} EnemyAIResult;

void EnemyAI_init(EnemyAI *ai);
/* Rol asignado al inicio del turno; los índices fuera del límite son assault. */
EnemyAITacticalRole EnemyAI_getTacticalRole(const EnemyAI *ai, u8 unitIndex);
/* Plan de grupo activo para el objetivo y su propietario actual. */
EnemyAITacticalPlan EnemyAI_getTacticalPlan(const EnemyAI *ai);
/* Índice del último actor del equipo rojo para la cámara espectadora. */
s16 EnemyAI_getFocusUnitIndex(const EnemyAI *ai);
/* Objetivo de fuego coordinado; -1 si no hay un blanco válido. */
s16 EnemyAI_getFocusTargetIndex(const EnemyAI *ai);
/* Próxima unidad roja que actuará, sin adelantar el estado de la IA. */
s16 EnemyAI_getNextActorIndex(const EnemyAI *ai,
                              const UnitController *units,
                              u16 turnNumber);
/*
 * Avanza una sola decisión por llamada; la animación sigue en el bucle común.
 * `map` debe ser exactamente `units->map` y `activeTeam` debe coincidir con el
 * equipo activo en UnitController. Si el contexto no coincide, devuelve IDLE
 * sin modificar el estado de la IA ni consumir la acción de ninguna unidad.
 */
EnemyAIResult EnemyAI_update(EnemyAI *ai, UnitController *units,
                             MapData *map, TeamId activeTeam,
                             u16 turnNumber, const MissionObjective *objective,
                             CombatResult *combatResult);

#endif
