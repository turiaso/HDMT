#ifndef GAME_TURN_H
#define GAME_TURN_H

#include <genesis.h>
#include "map_data.h"
#include "team.h"
#include "unit_controller.h"

typedef enum
{
    MISSION_CAPTURE_POINT = 0, /* Capturar el punto termina la misión al entrar. */
    MISSION_HOLD_POINT,        /* Mantener el punto durante N finales de turno. */
    MISSION_ELIMINATE_ALL,     /* Gana el equipo que elimina a todas las unidades rivales. */
    MISSION_SURVIVE_TURNS      /* El equipo objetivo gana al terminar N de sus turnos. */
} MissionType;

typedef struct
{
    const char *name;
    u16 x;
    u16 y;
    MissionType type;   /* Cero conserva el comportamiento clásico de captura. */
    u8 requiredTurns;   /* Para HOLD_POINT y SURVIVE_TURNS; cero significa uno. */
    TeamId targetTeam;  /* Equipo que debe sobrevivir; normalmente Azul. */
    u16 turnLimit;      /* Limite de rondas completas; 0 desactiva el limite. */
} MissionObjective;

typedef enum
{
    GAME_OUTCOME_ACTIVE = 0,
    GAME_OUTCOME_BLUE_VICTORY,
    GAME_OUTCOME_RED_VICTORY,
    GAME_OUTCOME_DRAW
} GameOutcome;

/* Estado de reglas de la partida, independiente de los sprites y del VDP. */
typedef struct
{
    MapData *map;
    UnitController *units;
    MissionObjective objective;
    TeamId activeTeam;
    u16 turnNumber;
    u16 turnsElapsed; /* Finales de turno realmente resueltos, sin duplicados. */
    u16 missionProgress;
    u8 objectiveHoldTurns;
    TeamId objectiveHoldTeam;
    GameOutcome outcome;
    /* Evita duplicar TURN_END; durante la evaluación inicial también impide
       cerrar un turno que todavía no se ha jugado si la misión ya está resuelta. */
    bool activeTurnEndDispatched;
    bool turnLimitReached;
} GameTurn;

bool GameTurn_init(GameTurn *game, MapData *map, UnitController *units,
                   TeamId firstTeam, const MissionObjective *objective);
/* Rechaza el cierre mientras una unidad siga animándose. */
bool GameTurn_endTurn(GameTurn *game);
/* Puede llamarse tras movimiento, combate o cambio de ocupante del objetivo. */
bool GameTurn_checkObjective(GameTurn *game);
TeamId GameTurn_getActiveTeam(const GameTurn *game);
u16 GameTurn_getNumber(const GameTurn *game);
u16 GameTurn_getTurnsElapsed(const GameTurn *game);
u16 GameTurn_getAliveUnitCount(const GameTurn *game, TeamId team);
u16 GameTurn_getMissionProgress(const GameTurn *game);
TeamId GameTurn_getObjectiveHoldTeam(const GameTurn *game);
GameOutcome GameTurn_getOutcome(const GameTurn *game);
const MissionObjective *GameTurn_getObjective(const GameTurn *game);
bool GameTurn_isFinished(const GameTurn *game);
const char *GameTurn_getOutcomeText(const GameTurn *game);
const char *GameTurn_getOutcomeReasonText(const GameTurn *game);
bool GameTurn_canIssueCommands(const GameTurn *game);
bool GameTurn_didCompleteObjective(const GameTurn *game);

#endif
