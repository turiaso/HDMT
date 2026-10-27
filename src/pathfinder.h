#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <genesis.h>
#include "map_data.h"
#include "unit_types.h"

/*
 * Memoria prestada por el escenario. Se comparte entre todas las unidades: una ruta
 * guardada en costs no puede usarse a la vez que otra búsqueda pisa el workspace.
 */
typedef struct PathfinderWorkspace
{
    /* During search: distance and parent direction. After success, costs
       stores the route directions in reverse order, saving one map-sized
       buffer. Do not run another search while a route is being consumed. */
    u8 *costs;
    u8 *parentDirections;
    u32 capacity;
    /* 0 = no alcanzable/origen; valor no cero = pasos mínimos + 1.
       Sigue siendo una máscara booleana por contrato (se consulta != 0), y
       permite al HUD conocer los pasos sin reconstruir la ruta al mover cursor. */
    u8 *reachable;
} PathfinderWorkspace;

typedef enum
{
    PATH_DIR_NONE = 0,
    PATH_DIR_UP,
    PATH_DIR_RIGHT,
    PATH_DIR_DOWN,
    PATH_DIR_LEFT
} PathDirection;

/* Busca una sola ruta concreta; el controlador solo la llama al confirmar A. */
/* API de compatibilidad para herramientas/pruebas: usa el perfil de infantería. */
bool Pathfinder_findRoute(const MapData *map, PathfinderWorkspace *workspace,
                          u16 startX, u16 startY, u16 targetX, u16 targetY,
                          s8 movingUnitId, u8 maxCost,
                          u16 *stepCount, u8 *totalCost);
/* Calcula una ruta con el coste de terreno específico de la clase que se mueve. */
bool Pathfinder_findRouteForType(const MapData *map,
                                 PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY,
                                 s8 movingUnitId, UnitType movingUnitType,
                                 u8 maxCost, u16 *stepCount, u8 *totalCost);
PathDirection Pathfinder_getStep(const PathfinderWorkspace *workspace,
                                 u16 stepCount, u16 stepIndex);
/* Calcula una vez el alcance ponderado al seleccionar unidad, para el glow. */
/* API de compatibilidad para herramientas/pruebas: usa el perfil de infantería. */
bool Pathfinder_buildReachability(const MapData *map,
                                  PathfinderWorkspace *workspace,
                                  u16 startX, u16 startY, s8 movingUnitId,
                                  u8 maxCost);
/* Calcula el alcance con los costes de terreno de una clase concreta. */
bool Pathfinder_buildReachabilityForType(const MapData *map,
                                         PathfinderWorkspace *workspace,
                                         u16 startX, u16 startY,
                                         s8 movingUnitId,
                                         UnitType movingUnitType,
                                         u8 maxCost);
/* Consulta O(1) del alcance en caché; no busca ni reconstruye rutas. */
bool Pathfinder_getReachableInfo(const MapData *map,
                                 const PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY, u8 maxCost,
                                 u16 *stepCount, u8 *totalCost);
bool Pathfinder_getReachableStep(const MapData *map,
                                 const PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY, u8 maxCost,
                                 u16 stepIndex, u16 *stepX, u16 *stepY);

#endif
