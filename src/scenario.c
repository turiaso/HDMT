#include "scenario.h"
#include "map_data.h"
#include "resources.h"

/*
 * Registro de escenarios y despliegues iniciales. Cada escenario describe sus recursos,
 * mapa lógico, unidades y objetivo; solo una definición está activa por partida para
 * reutilizar los buffers grandes de RAM.
 */

/*
 * Despliegue fijo y editable: 60 unidades por equipo.
 * Azul comienza en el cuadrante noroeste; Rojo, en el sureste.
 * Las posiciones se eligieron sobre terreno transitable y sin solaparse.
 * Los dos equipos usan la misma mezcla: 8 infanterías, 8 exploradores,
 * 7 ingenieros y 7 blindados.
 */
const UnitStart worldUnitStarts[SCENARIO_UNIT_COUNT] =
{
    /* Equipo Azul, IDs 0..59; despliegue repartido por el cuadrante noroeste. */
    {   0,  4,  4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {   1, 15, 17, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {   2, 16,  3, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {   3,  3, 16, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {   4, 10, 10, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {   5, 17, 10, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {   6, 10,  2, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {   7,  2, 10, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {   8,  9, 16, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {   9,  8,  6, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  10,  6, 12, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  11, 13,  6, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  12, 14, 13, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  13,  5,  8, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  14,  7,  3, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  15, 16,  7, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  16,  6, 17, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  17, 13,  2, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  18, 13,  9, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  19,  3, 13, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  20,  9, 13, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  21, 12, 16, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  22,  5,  2, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  23,  9,  4, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  24, 12,  4, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  25,  6,  5, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  26, 15,  5, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  27,  3,  6, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  28, 10,  7, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  29,  7,  9, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },

    /* Refuerzos azules: se amplía el despliegue sin solapar casillas. */
    {  30,  8,  1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  31, 15,  1, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  32,  4, 11, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  33,  8, 11, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  34, 12, 11, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  35, 15, 11, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  36,  5, 14, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  37, 11, 14, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  38, 16, 14, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  39,  7, 15, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  40,  4, 18, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  41,  8, 18, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  42, 11, 18, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  43,  2,  8, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  44, 15,  9, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  45, 14, 15, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  46, 13, 18, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  47,  4,  1, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  48,  6,  1, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  49, 11,  1, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  50, 11,  3, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  51, 14,  3, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  52, 10,  5, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  53,  5,  6, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  54, 11,  6, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  55,  4,  7, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  56,  6,  7, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  57, 12,  7, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  58, 14,  7, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  59,  8,  8, 0, UNIT_TYPE_TANK, TEAM_BLUE },

    /* Equipo Rojo, IDs 60..119; despliegue en el cuadrante sureste. */
    {  60, 24, 24, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  61, 35, 37, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  62, 36, 23, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  63, 23, 36, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  64, 30, 30, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  65, 37, 30, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  66, 30, 22, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  67, 22, 30, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  68, 29, 36, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  69, 28, 26, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  70, 26, 32, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  71, 33, 26, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  72, 34, 33, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  73, 25, 28, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  74, 27, 23, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  75, 36, 27, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  76, 26, 37, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  77, 33, 22, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  78, 33, 29, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  79, 23, 33, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  80, 29, 33, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  81, 32, 36, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  82, 25, 22, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  83, 29, 24, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  84, 32, 24, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  85, 26, 25, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  86, 35, 25, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {  87, 23, 26, 0, UNIT_TYPE_TANK, TEAM_RED },
    {  88, 30, 27, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  89, 27, 29, 0, UNIT_TYPE_SCOUT, TEAM_RED },

    /* Refuerzos rojos: mismas proporciones de tipos que Azul. */
    {  90, 28, 21, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  91, 35, 21, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  92, 24, 31, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  93, 28, 31, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  94, 32, 31, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  95, 35, 31, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  96, 25, 34, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  97, 31, 34, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    {  98, 36, 34, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    {  99, 27, 35, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 100, 24, 38, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 101, 28, 38, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 102, 31, 38, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 103, 22, 28, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 104, 35, 29, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 105, 34, 35, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 106, 33, 38, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 107, 24, 21, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 108, 26, 21, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 109, 31, 21, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 110, 31, 23, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 111, 34, 23, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 112, 30, 25, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 113, 25, 26, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 114, 31, 26, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 115, 24, 27, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 116, 26, 27, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 117, 32, 27, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 118, 34, 27, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 119, 28, 28, 0, UNIT_TYPE_TANK, TEAM_RED }
};

/* Despliegue de la mision 2: fuerzas compactas disputan tres puentes.
 * Cada bando dispone de 12 unidades: 4 infanterias, 3 exploradores,
 * 2 ingenieros y 3 blindados.
 */
const UnitStart bridgeUnitStarts[BRIDGE_SCENARIO_UNIT_COUNT] =
{
    {  0,  4, 12, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  1,  6, 12, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  2,  8, 12, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  3, 10, 12, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  4,  4, 16, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  5,  6, 16, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    {  6,  8, 16, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    {  7, 10, 16, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    {  8,  4, 24, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    {  9,  6, 24, 0, UNIT_TYPE_SCOUT, TEAM_BLUE },
    { 10,  8, 24, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    { 11, 10, 24, 0, UNIT_TYPE_TANK, TEAM_BLUE },
    { 12, 35, 12, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 13, 33, 12, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 14, 31, 12, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 15, 29, 12, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 16, 35, 16, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 17, 33, 16, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 18, 31, 16, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 19, 29, 16, 0, UNIT_TYPE_TANK, TEAM_RED },
    { 20, 35, 24, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 21, 33, 24, 0, UNIT_TYPE_SCOUT, TEAM_RED },
    { 22, 31, 24, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    { 23, 29, 24, 0, UNIT_TYPE_TANK, TEAM_RED }
};

/* Despliegue de la escaramuza 20x20: 6 unidades por equipo.
 * Cada bando combina 2 infanterías, 1 explorador, 1 ingeniero y 2 blindados. */
const UnitStart bridgeSkirmishUnitStarts[BRIDGE_SKIRMISH_UNIT_COUNT] =
{
    { 0,  4,  6, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    { 1,  6,  6, 0, UNIT_TYPE_INFANTRY, TEAM_BLUE },
    { 2,  4, 12, 0, UNIT_TYPE_SCOUT,    TEAM_BLUE },
    { 3,  6, 12, 0, UNIT_TYPE_ENGINEER, TEAM_BLUE },
    { 4,  5,  7, 0, UNIT_TYPE_TANK,     TEAM_BLUE },
    { 5,  7,  7, 0, UNIT_TYPE_TANK,     TEAM_BLUE },
    { 6, 13,  6, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 7, 15,  6, 0, UNIT_TYPE_INFANTRY, TEAM_RED },
    { 8, 13, 12, 0, UNIT_TYPE_SCOUT,    TEAM_RED },
    { 9, 15, 12, 0, UNIT_TYPE_ENGINEER, TEAM_RED },
    {10, 12,  7, 0, UNIT_TYPE_TANK,     TEAM_RED },
    {11, 14,  7, 0, UNIT_TYPE_TANK,     TEAM_RED }
};

/* Almacenamiento de ejecución compartido: solo se inicializa una misión. */
static Unit scenarioUnitStorage[SCENARIO_MAX_UNIT_COUNT];

/*
 * Registro de escenarios. Para añadir uno nuevo se amplía este registro,
 * se define su terreno/despliegue y se declara su recurso MAP en resources.res.
 * No es necesario modificar main.c.
 */
static const ScenarioDefinition scenarioDefinitions[SCENARIO_COUNT] =
{
    {
        "PUNTO DE CONTROL",
        "CAPTURA EL OBJETIVO CENTRAL",
        "120 UNIDADES - 60 POR EQUIPO",
        &world_map,
        &worldMapData,
        worldUnitStarts,
        (u8)SCENARIO_UNIT_COUNT,
        scenarioUnitStorage,
        (u8)SCENARIO_MAX_UNIT_COUNT,
        { "PUNTO DE CONTROL", SCENARIO_OBJECTIVE_X, SCENARIO_OBJECTIVE_Y,
          MISSION_CAPTURE_POINT, 0, TEAM_BLUE, 0 }
    },
    {
        "DEFENSA DEL PUENTE",
        "MANTEN EL PUENTE 3 TURNOS",
        "24 UNIDADES - 12 POR EQUIPO",
        &bridge_map,
        &bridgeMapData,
        bridgeUnitStarts,
        (u8)BRIDGE_SCENARIO_UNIT_COUNT,
        scenarioUnitStorage,
        (u8)SCENARIO_MAX_UNIT_COUNT,
        { "DEFENSA DEL PUENTE", 20, 20,
          MISSION_HOLD_POINT, 3, TEAM_BLUE, 0 }
    },
    {
        "PUENTE DEL DESFILADERO",
        "MANTEN EL PUENTE 3 TURNOS",
        "12 UNIDADES - 6 POR EQUIPO",
        &bridge_skirmish_map,
        &bridgeSkirmishMapData,
        bridgeSkirmishUnitStarts,
        (u8)BRIDGE_SKIRMISH_UNIT_COUNT,
        scenarioUnitStorage,
        (u8)SCENARIO_MAX_UNIT_COUNT,
        { "PUENTE DEL DESFILADERO", 10, 9,
          MISSION_HOLD_POINT, 3, TEAM_BLUE, 0 }
    },
    {
        "CRUCE DE LAS RUINAS",
        "MANTEN EL CRUCE 3 TURNOS",
        "12 UNIDADES - 6 POR EQUIPO",
        &ruins_crossing_map,
        &ruinsCrossingMapData,
        bridgeSkirmishUnitStarts,
        (u8)BRIDGE_SKIRMISH_UNIT_COUNT,
        scenarioUnitStorage,
        (u8)SCENARIO_MAX_UNIT_COUNT,
        { "CRUCE DE LAS RUINAS", 10, 9,
          MISSION_HOLD_POINT, 3, TEAM_BLUE, 0 }
    }
};

const ScenarioDefinition *Scenario_get(u8 index)
{
    if (index >= SCENARIO_COUNT) return NULL;
    return &scenarioDefinitions[index];
}

u8 Scenario_getCount(void)
{
    return (u8)SCENARIO_COUNT;
}


/* Navegación circular del selector de escenarios. */
u8 Scenario_cycleIndex(u8 current, s8 direction)
{
    const u8 count = Scenario_getCount();

    if (count == 0) return 0;
    if (current >= count) current = 0;

    if (direction < 0)
        return (current == 0) ? (u8)(count - 1) : (u8)(current - 1);
    if (direction > 0)
        return ((u8)(current + 1) >= count) ? 0 : (u8)(current + 1);

    return current;
}
