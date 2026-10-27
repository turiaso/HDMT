#include "game_turn.h"
#include "cell_events.h"

/*
 * Estado de partida: alternancia de equipos, puntos por turno, condiciones de misión
 * y resolución de victoria/derrota. No contiene presentación ni depende del VDP.
 */

static TeamId nextTeam(TeamId team)
{
    return (team == TEAM_BLUE) ? TEAM_RED : TEAM_BLUE;
}

/* Despacha los efectos del final de turno como máximo una vez. El indicador se
   activa antes de ejecutar callbacks para protegerse de reentradas accidentales. */
static void dispatchActiveTurnEnd(GameTurn *game)
{
    if ((game == NULL) || game->activeTurnEndDispatched) return;

    game->activeTurnEndDispatched = TRUE;
    /* El resumen cuenta finales de turno reales, nunca consultas ni reintentos. */
    if (game->turnsElapsed < 0xFFFFu) game->turnsElapsed++;
    CellEvents_dispatchTurn(game->map, CELL_EVENT_TURN_END,
                            (u8)game->activeTeam, game->turnNumber);
}

static void finishWithOutcome(GameTurn *game, GameOutcome outcome)
{
    if ((game == NULL) || (game->outcome != GAME_OUTCOME_ACTIVE)) return;

    /* Los efectos de fin de turno siempre se resuelven antes de cerrar la partida. */
    dispatchActiveTurnEnd(game);
    UnitController_haltMovement(game->units);
    game->outcome = outcome;
}

static void finishForTeam(GameTurn *game, TeamId winner)
{
    if ((u8)winner >= TEAM_COUNT) return;
    finishWithOutcome(game, (winner == TEAM_BLUE)
        ? GAME_OUTCOME_BLUE_VICTORY : GAME_OUTCOME_RED_VICTORY);
}

static void finishAsDraw(GameTurn *game)
{
    finishWithOutcome(game, GAME_OUTCOME_DRAW);
}

static void countAliveUnitsByTeam(const GameTurn *game, u16 aliveByTeam[TEAM_COUNT]);
static TeamId objectiveOwner(const GameTurn *game);

/*
 * Al agotarse el limite, prioriza el control del objetivo y despues el numero
 * de unidades supervivientes. El limite se evalua al cerrar la ronda roja,
 * cuando ambos equipos han tenido su turno.
 */
static void resolveTurnLimit(GameTurn *game)
{
    u16 aliveByTeam[TEAM_COUNT];
    TeamId owner;

    if ((game == NULL) || (game->objective.turnLimit == 0) ||
        (game->activeTeam != TEAM_RED) ||
        ((game->turnNumber / 2u) < game->objective.turnLimit))
        return;

    countAliveUnitsByTeam(game, aliveByTeam);
    owner = objectiveOwner(game);
    game->turnLimitReached = TRUE;

    if ((game->objective.type == MISSION_HOLD_POINT) && (owner != TEAM_NONE))
    {
        finishForTeam(game, owner);
        return;
    }

    if (aliveByTeam[TEAM_BLUE] > aliveByTeam[TEAM_RED])
        finishForTeam(game, TEAM_BLUE);
    else if (aliveByTeam[TEAM_RED] > aliveByTeam[TEAM_BLUE])
        finishForTeam(game, TEAM_RED);
    else
        finishAsDraw(game);
}

/* Cuenta ambos bandos en una sola pasada cuando una misión necesita compararlos. */
static void countAliveUnitsByTeam(const GameTurn *game, u16 aliveByTeam[TEAM_COUNT])
{
    u8 i;
    u8 teamIndex;
    const u8 unitCount = UnitController_getCount(game->units);

    for (teamIndex = 0; teamIndex < TEAM_COUNT; teamIndex++)
        aliveByTeam[teamIndex] = 0;

    for (i = 0; i < unitCount; i++)
    {
        const Unit *unit = UnitController_getUnit(game->units, i);
        if ((unit == NULL) || !unit->alive || ((u8)unit->team >= TEAM_COUNT))
            continue;

        aliveByTeam[unit->team]++;
    }
}

/* Cero en los datos de misión conserva el comportamiento de una ronda. */
static u8 requiredObjectiveTurns(const GameTurn *game)
{
    return game->objective.requiredTurns ? game->objective.requiredTurns : 1u;
}

static TeamId objectiveOwner(const GameTurn *game)
{
    const CellData *cell = MapData_getCellConst(
        game->map, game->objective.x, game->objective.y);
    const Unit *unit;

    if ((cell == NULL) || (cell->occupantId == CELL_NO_OCCUPANT))
        return TEAM_NONE;
    unit = UnitController_getMutableUnitById(game->units,
                                              (u8)cell->occupantId);
    if ((unit == NULL) || !unit->alive) return TEAM_NONE;
    return unit->team;
}

static bool updateHoldProgress(GameTurn *game)
{
    const TeamId owner = objectiveOwner(game);
    const u8 required = requiredObjectiveTurns(game);

    if (owner == TEAM_NONE)
    {
        game->objectiveHoldTeam = TEAM_NONE;
        game->objectiveHoldTurns = 0;
        return FALSE;
    }
    if (owner != game->objectiveHoldTeam)
    {
        game->objectiveHoldTeam = owner;
        game->objectiveHoldTurns = 0;
    }

    /* Solo cuenta al final del turno del equipo que realmente ocupa el punto. */
    if (owner == game->activeTeam)
    {
        if (game->objectiveHoldTurns < 255u) game->objectiveHoldTurns++;
        if (game->objectiveHoldTurns >= required)
        {
            finishForTeam(game, owner);
            return TRUE;
        }
    }
    return FALSE;
}

bool GameTurn_init(GameTurn *game, MapData *map, UnitController *units,
                   TeamId firstTeam, const MissionObjective *objective)
{
    const CellData *objectiveCell;

    if ((game == NULL) || (map == NULL) || (units == NULL) ||
        (objective == NULL) || ((u8)firstTeam >= TEAM_COUNT) ||
        ((u8)objective->type > (u8)MISSION_SURVIVE_TURNS) ||
        ((u8)objective->targetTeam >= TEAM_COUNT))
        return FALSE;

    objectiveCell = MapData_getCellConst(map, objective->x, objective->y);
    if (objectiveCell == NULL) return FALSE;
    if ((objective->type == MISSION_CAPTURE_POINT) ||
        (objective->type == MISSION_HOLD_POINT))
    {
        if (!MapData_isPassable(map, objective->x, objective->y) ||
            (objectiveCell->occupantId != CELL_NO_OCCUPANT))
            return FALSE;
    }

    game->map = map;
    game->units = units;
    game->objective = *objective;
    game->activeTeam = firstTeam;
    game->turnNumber = 1;
    game->turnsElapsed = 0;
    game->missionProgress = 0;
    game->objectiveHoldTurns = 0;
    game->objectiveHoldTeam = TEAM_NONE;
    game->outcome = GAME_OUTCOME_ACTIVE;
    game->activeTurnEndDispatched = FALSE;
    game->turnLimitReached = FALSE;

    if (!UnitController_beginTeamTurn(units, firstTeam, game->turnNumber))
        return FALSE;

    /* Resolver un resultado inmediato al comenzar no debe emitir TURN_END:
       todavía no se ha jugado ningún turno. El centinela también protege una
       misión que empiece ya resuelta (por ejemplo, con un equipo vacío). */
    game->activeTurnEndDispatched = TRUE;
    CellEvents_dispatchTurn(map, CELL_EVENT_TURN_START, (u8)firstTeam,
                            game->turnNumber);
    (void)GameTurn_checkObjective(game);

    /* Si la misión sigue activa, el turno queda abierto y puede cerrarse normal. */
    if (game->outcome == GAME_OUTCOME_ACTIVE)
        game->activeTurnEndDispatched = FALSE;
    return TRUE;
}

bool GameTurn_endTurn(GameTurn *game)
{
    TeamId next;
    u16 nextNumber;

    if ((game == NULL) || (game->outcome != GAME_OUTCOME_ACTIVE) ||
        UnitController_isMoving(game->units) || game->activeTurnEndDispatched)
        return FALSE;

    /* Primero se aplican todos los efectos TURN_END; la victoria se calcula con
       el estado que queda después de esos efectos, no con el estado anterior. */
    dispatchActiveTurnEnd(game);
    if (GameTurn_checkObjective(game)) return TRUE;

    /* Los objetivos temporizados solo avanzan en el cierre del turno correcto. */
    if (game->objective.type == MISSION_HOLD_POINT)
    {
        if (updateHoldProgress(game)) return TRUE;
    }
    else if (game->objective.type == MISSION_SURVIVE_TURNS &&
             game->activeTeam == game->objective.targetTeam)
    {
        const u8 required = requiredObjectiveTurns(game);
        game->missionProgress++;
        if (game->missionProgress >= required)
        {
            finishForTeam(game, game->objective.targetTeam);
            return TRUE;
        }
    }

    if (game->outcome != GAME_OUTCOME_ACTIVE) return TRUE;

    resolveTurnLimit(game);
    if (game->outcome != GAME_OUTCOME_ACTIVE) return TRUE;

    next = nextTeam(game->activeTeam);
    /* El número de turno no puede envolver a cero: se declara empate al agotar
       el rango representable en vez de dejar una partida activa e inavanzable. */
    if (game->turnNumber == 0xFFFFu)
    {
        finishAsDraw(game);
        return TRUE;
    }
    nextNumber = (u16)(game->turnNumber + 1);
    if (!UnitController_beginTeamTurn(game->units, next, nextNumber))
    {
        /* Ya se consumió el final del turno; no aceptar reintentos que dupliquen
           eventos o progreso. El estado es irrecuperable, por lo que se cierra. */
        finishAsDraw(game);
        return TRUE;
    }

    game->activeTeam = next;
    game->turnNumber = nextNumber;
    game->activeTurnEndDispatched = FALSE;
    CellEvents_dispatchTurn(game->map, CELL_EVENT_TURN_START,
                            (u8)game->activeTeam, game->turnNumber);
    (void)GameTurn_checkObjective(game);
    return TRUE;
}

bool GameTurn_checkObjective(GameTurn *game)
{
    TeamId owner;
    u16 aliveByTeam[TEAM_COUNT];

    if ((game == NULL) || (game->outcome != GAME_OUTCOME_ACTIVE))
        return FALSE;

    /* Sin unidades en ambos bandos ningún objetivo puede resolverse. Empate
       explícito y uniforme para todos los tipos de misión. */
    countAliveUnitsByTeam(game, aliveByTeam);
    if ((aliveByTeam[TEAM_BLUE] == 0) && (aliveByTeam[TEAM_RED] == 0))
    {
        finishAsDraw(game);
        return TRUE;
    }

    owner = objectiveOwner(game);

    switch (game->objective.type)
    {
        case MISSION_CAPTURE_POINT:
            if (owner != TEAM_NONE)
            {
                finishForTeam(game, owner);
                return TRUE;
            }
            return FALSE;

        case MISSION_HOLD_POINT:
            /* Cambiar de ocupante interrumpe la racha, incluso antes del turno. */
            if (owner == TEAM_NONE)
            {
                game->objectiveHoldTeam = TEAM_NONE;
                game->objectiveHoldTurns = 0;
            }
            else if (owner != game->objectiveHoldTeam)
            {
                game->objectiveHoldTeam = owner;
                game->objectiveHoldTurns = 0;
            }
            return FALSE;

        case MISSION_ELIMINATE_ALL:
        {
            u16 blue;
            u16 red;

            blue = aliveByTeam[TEAM_BLUE];
            red = aliveByTeam[TEAM_RED];
            if (red == 0)
            {
                finishForTeam(game, TEAM_BLUE);
                return TRUE;
            }
            if (blue == 0)
            {
                finishForTeam(game, TEAM_RED);
                return TRUE;
            }
            return FALSE;
        }

        case MISSION_SURVIVE_TURNS:
        {
            const TeamId target = game->objective.targetTeam;
            const TeamId enemy = (target == TEAM_BLUE) ? TEAM_RED : TEAM_BLUE;

            if (aliveByTeam[target] == 0)
            {
                finishForTeam(game, enemy);
                return TRUE;
            }
            if (aliveByTeam[enemy] == 0)
            {
                finishForTeam(game, target);
                return TRUE;
            }
            return FALSE;
        }

        default:
            return FALSE;
    }
}

TeamId GameTurn_getActiveTeam(const GameTurn *game)
{
    return (game != NULL) ? game->activeTeam : TEAM_BLUE;
}

u16 GameTurn_getNumber(const GameTurn *game)
{
    return (game != NULL) ? game->turnNumber : 0;
}

u16 GameTurn_getTurnsElapsed(const GameTurn *game)
{
    return (game != NULL) ? game->turnsElapsed : 0;
}

u16 GameTurn_getAliveUnitCount(const GameTurn *game, TeamId team)
{
    u16 aliveByTeam[TEAM_COUNT];

    if ((game == NULL) || ((u8)team >= TEAM_COUNT)) return 0;
    countAliveUnitsByTeam(game, aliveByTeam);
    return aliveByTeam[team];
}

u16 GameTurn_getMissionProgress(const GameTurn *game)
{
    if (game == NULL) return 0;
    return (game->objective.type == MISSION_HOLD_POINT)
        ? game->objectiveHoldTurns : game->missionProgress;
}


TeamId GameTurn_getObjectiveHoldTeam(const GameTurn *game)
{
    return (game != NULL) ? game->objectiveHoldTeam : TEAM_NONE;
}

GameOutcome GameTurn_getOutcome(const GameTurn *game)
{
    return (game != NULL) ? game->outcome : GAME_OUTCOME_ACTIVE;
}

const MissionObjective *GameTurn_getObjective(const GameTurn *game)
{
    return (game != NULL) ? &game->objective : NULL;
}

bool GameTurn_isFinished(const GameTurn *game)
{
    return (game != NULL) && (game->outcome != GAME_OUTCOME_ACTIVE);
}

const char *GameTurn_getOutcomeText(const GameTurn *game)
{
    if (game == NULL) return "";
    switch (game->outcome)
    {
        case GAME_OUTCOME_BLUE_VICTORY:
            return "VICTORIA AZUL / DERROTA ROJA";
        case GAME_OUTCOME_RED_VICTORY:
            return "VICTORIA ROJA / DERROTA AZUL";
        case GAME_OUTCOME_DRAW:
            return "EMPATE";
        default:
            return "";
    }
}


/* Describe la condición concreta que llevó al resultado final. */
const char *GameTurn_getOutcomeReasonText(const GameTurn *game)
{
    u16 aliveByTeam[TEAM_COUNT];
    TeamId winner;
    TeamId target;
    TeamId enemy;

    if ((game == NULL) || (game->outcome == GAME_OUTCOME_ACTIVE)) return "";

    countAliveUnitsByTeam(game, aliveByTeam);
    if (game->turnLimitReached) return "LIMITE DE TURNOS";
    if (game->outcome == GAME_OUTCOME_DRAW)
    {
        if ((aliveByTeam[TEAM_BLUE] == 0) && (aliveByTeam[TEAM_RED] == 0))
            return "AMBOS EQUIPOS ELIMINADOS";
        return "EMPATE TECNICO";
    }

    winner = (game->outcome == GAME_OUTCOME_BLUE_VICTORY)
        ? TEAM_BLUE : TEAM_RED;

    switch (game->objective.type)
    {
        case MISSION_CAPTURE_POINT:
            return "OBJETIVO CAPTURADO";

        case MISSION_HOLD_POINT:
            return "OBJETIVO MANTENIDO";

        case MISSION_ELIMINATE_ALL:
            return "EJERCITO RIVAL ELIMINADO";

        case MISSION_SURVIVE_TURNS:
            target = game->objective.targetTeam;
            enemy = (target == TEAM_BLUE) ? TEAM_RED : TEAM_BLUE;
            if ((winner == target) && (aliveByTeam[enemy] == 0))
                return "EJERCITO RIVAL ELIMINADO";
            if (winner == target) return "SUPERVIVENCIA COMPLETADA";
            if (aliveByTeam[target] == 0)
                return "EQUIPO OBJETIVO ELIMINADO";
            return "SUPERVIVENCIA FALLIDA";

        default:
            return "MISION RESUELTA";
    }
}

bool GameTurn_canIssueCommands(const GameTurn *game)
{
    return (game != NULL) && (game->outcome == GAME_OUTCOME_ACTIVE);
}


/* El resultado de la misión no implica siempre que se haya cumplido el
   objetivo fijado: en MISSION_SURVIVE_TURNS puede ganar el rival al eliminar
   al equipo que debía sobrevivir. */
bool GameTurn_didCompleteObjective(const GameTurn *game)
{
    TeamId winner;

    if ((game == NULL) || (game->outcome == GAME_OUTCOME_ACTIVE) ||
        (game->outcome == GAME_OUTCOME_DRAW))
        return FALSE;

    if (game->turnLimitReached) return FALSE;
    if (game->objective.type != MISSION_SURVIVE_TURNS)
        return TRUE;

    winner = (game->outcome == GAME_OUTCOME_BLUE_VICTORY)
        ? TEAM_BLUE : TEAM_RED;
    return winner == game->objective.targetTeam;
}
