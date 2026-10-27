#include "unit_types.h"

/* Perfil autoritativo por clase; el despliegue copia atributos de combate a cada Unit.
   Pathfinder y HUD consultan terrainMoveCosts directamente para no duplicar reglas. */

/* Las reglas de cada clase se editan aquí, no dentro del combate. */
static const UnitTypeInfo unitTypes[UNIT_TYPE_COUNT] =
{
    /*                 agua, bosque, camino, piedras */
    { "INFANTERIA", "INF", "UNIDAD EQUILIBRADA", 6, 10, 4, 2, 1, 3, 0, 0,
      { 0, 2, 1, 2 } },
    { "EXPLORADOR", "EXP", "MOVILIDAD Y VISION ALTAS", 8, 7, 3, 1, 1, 4, 0, 0,
      { 0, 1, 1, 1 } },
    { "INGENIERO",  "ING", "REPARACION Y APOYO", 5, 8, 2, 2, 1, 2, 1, 3,
      { 0, 3, 1, 2 } },
    { "BLINDADO",   "BLD", "ATAQUE FUERTE A DISTANCIA", 7, 14, 6, 3, 2, 3, 0, 0,
      { 0, 3, 1, 4 } }
};

const UnitTypeInfo *UnitType_getInfo(UnitType type)
{
    if ((u8)type >= UNIT_TYPE_COUNT) return NULL;
    return &unitTypes[type];
}


u8 UnitType_getTerrainMoveCost(UnitType type, TerrainType terrain)
{
    const UnitTypeInfo *info = UnitType_getInfo(type);

    if ((info == NULL) || ((u8)terrain >= TERRAIN_COUNT)) return 0;
    return info->terrainMoveCosts[terrain];
}
