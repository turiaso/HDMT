#ifndef UNIT_TYPES_H
#define UNIT_TYPES_H

#include <genesis.h>
#include "map_data.h"

#define UNIT_TYPE_COUNT 4

typedef enum
{
    UNIT_TYPE_INFANTRY = 0,
    UNIT_TYPE_SCOUT,
    UNIT_TYPE_ENGINEER,
    UNIT_TYPE_TANK
} UnitType;

typedef struct
{
    const char *name;
    const char *shortName;
    const char *role;
    u8 defaultMovementPoints;
    u8 maxHealth;
    u8 attack;
    u8 defense;
    u8 attackRange;
    u8 visionRange;
    u8 repairRange;       /* Distancia Manhattan; cero si la clase no repara. */
    u8 repairAmount;      /* PV restaurados por accion, limitados por la vida maxima. */
    /* Coste de entrar en cada terreno; 0 significa que esta clase no puede entrar. */
    u8 terrainMoveCosts[TERRAIN_COUNT];
} UnitTypeInfo;

const UnitTypeInfo *UnitType_getInfo(UnitType type);
/* Coste de entrar en una casilla para esta clase; 0 si el terreno no es transitable. */
u8 UnitType_getTerrainMoveCost(UnitType type, TerrainType terrain);

#endif
