#ifndef SCENARIO_H
#define SCENARIO_H

#include <genesis.h>
#include "resources.h"
#include "unit_controller.h"
#include "game_turn.h"

/* Escenario 1: punto de control con 60 unidades por bando. */
#define SCENARIO_UNIT_COUNT 120u
#define SCENARIO_OBJECTIVE_X 20u
#define SCENARIO_OBJECTIVE_Y 20u

/* Escenario 2: escaramuza de puente 20x20; 6 unidades por bando. */
#define BRIDGE_SKIRMISH_UNIT_COUNT 12u
/* Escenario 1: defensa del puente 40x40; 12 unidades por bando. */
#define BRIDGE_SCENARIO_UNIT_COUNT 24u
#define SCENARIO_MAX_UNIT_COUNT SCENARIO_UNIT_COUNT
#define SCENARIO_COUNT 4u

typedef struct
{
    const char *name;
    const char *menuDescription;
    const char *menuSummary;
    const MapDefinition *graphics;
    MapData *logic;
    const UnitStart *unitStarts;
    u8 unitCount;
    Unit *unitStorage;
    u8 unitCapacity;
    MissionObjective objective;
} ScenarioDefinition;

extern const UnitStart worldUnitStarts[SCENARIO_UNIT_COUNT];
extern const UnitStart bridgeUnitStarts[BRIDGE_SCENARIO_UNIT_COUNT];
extern const UnitStart bridgeSkirmishUnitStarts[BRIDGE_SKIRMISH_UNIT_COUNT];

/* Devuelve NULL si el índice no existe. Solo se juega un escenario a la vez. */
const ScenarioDefinition *Scenario_get(u8 index);
u8 Scenario_getCount(void);
/* Avanza o retrocede con vuelta circular; current invalido se normaliza a 0. */
u8 Scenario_cycleIndex(u8 current, s8 direction);

#endif
