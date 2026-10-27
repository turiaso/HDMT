/*
 * Búsqueda de rutas y costes de movimiento. Este módulo calcula resultados sin mover
 * unidades; UnitController aplica después la ruta y actualiza la ocupación del mapa.
 */
#include "pathfinder.h" /* Define los tipos de SGDK antes de incluir string.h. */
#include <string.h>
#include "performance.h"

/*
 * Búsqueda ponderada sobre MapData. Se reutiliza un workspace estático del mapa activo:
 * no iniciar otra búsqueda mientras UnitController esté consumiendo la ruta resultante.
 * El cálculo de alcance y la ruta confirmada son operaciones distintas a propósito.
 */

#define COST_UNREACHABLE 0xFFu
#define NODE_QUEUED      0x40u
#define DIRECTION_MASK   0x07u
#define PATHFINDER_MAX_CELLS GAMEPLAY_MAP_MAX_CELLS

/*
 * El buscador usa una cola circular reutilizable. Solo se ejecuta una búsqueda
 * síncrona a la vez sobre el workspace compartido del mapa; no necesitamos
 * reservar una cola de nodos por unidad.
 */
static u16 searchQueue[PATHFINDER_MAX_CELLS];


/* Geometría de búsqueda y expansión ponderada de vecinos. */

static void getSearchBounds(const MapData *map, u16 startX, u16 startY,
                            u8 maxCost, u16 *minX, u16 *maxX,
                            u16 *minY, u16 *maxY)
{
    const u32 candidateMaxX = (u32)startX + maxCost;
    const u32 candidateMaxY = (u32)startY + maxCost;

    *minX = (startX > maxCost) ? (u16)(startX - maxCost) : 0;
    *minY = (startY > maxCost) ? (u16)(startY - maxCost) : 0;
    *maxX = (candidateMaxX < map->width)
        ? (u16)candidateMaxX : (u16)(map->width - 1u);
    *maxY = (candidateMaxY < map->height)
        ? (u16)candidateMaxY : (u16)(map->height - 1u);
}

static PathDirection opposite(PathDirection direction)
{
    switch (direction)
    {
        case PATH_DIR_UP:    return PATH_DIR_DOWN;
        case PATH_DIR_RIGHT: return PATH_DIR_LEFT;
        case PATH_DIR_DOWN:  return PATH_DIR_UP;
        case PATH_DIR_LEFT:  return PATH_DIR_RIGHT;
        default:             return PATH_DIR_NONE;
    }
}

static u32 neighbourIndex(u32 index, u16 width, u16 height,
                          PathDirection direction)
{
    const u16 x = (u16)(index % width);
    const u16 y = (u16)(index / width);

    switch (direction)
    {
        case PATH_DIR_UP:    return (y > 0) ? index - width : 0xFFFFFFFFu;
        case PATH_DIR_RIGHT: return (x + 1 < width) ? index + 1 : 0xFFFFFFFFu;
        case PATH_DIR_DOWN:  return (y + 1 < height) ? index + width : 0xFFFFFFFFu;
        case PATH_DIR_LEFT:  return (x > 0) ? index - 1 : 0xFFFFFFFFu;
        default:             return 0xFFFFFFFFu;
    }
}

static bool canTraverse(const MapData *map, u32 index, u32 startIndex,
                        s8 movingUnitId, UnitType movingUnitType,
                        u8 *moveCost)
{
    const u16 x = (u16)(index % map->width);
    const u16 y = (u16)(index / map->width);
    const CellData *cell = MapData_getCellConst(map, x, y);
    const TerrainInfo *terrain;

    if (cell == NULL) return FALSE;
    if ((index != startIndex) && (cell->occupantId != CELL_NO_OCCUPANT))
        return FALSE;
    if ((index == startIndex) && (cell->occupantId != CELL_NO_OCCUPANT) &&
        (cell->occupantId != movingUnitId))
        return FALSE;

    terrain = MapData_getTerrainInfo((TerrainType)cell->terrain);
    if ((terrain == NULL) || !terrain->passable) return FALSE;

    *moveCost = UnitType_getTerrainMoveCost(movingUnitType,
                                             (TerrainType)cell->terrain);
    if (*moveCost == 0) return FALSE;
    return TRUE;
}

/*
 * SPFA acotado al presupuesto de movimiento. Antes se recorría todo el
 * rectángulo alcanzable por cada nivel de coste, incluso si casi ninguna
 * casilla seguía pendiente. Esta cola visita únicamente casillas que han
 * recibido una mejora y evita repetir barridos de hasta 17x17 casillas.
 *
 * Los costes de terreno son positivos. NODE_QUEUED impide que un índice esté
 * duplicado simultáneamente en la cola; si encuentra un coste mejor después
 * de salir, vuelve a entrar. costs[] guarda coste mínimo y parentDirections[]
 * guarda la dirección hacia el padre más la marca temporal de cola.
 */
/* Núcleo compartido de búsqueda ponderada. */

static bool runCostSearch(const MapData *map, PathfinderWorkspace *workspace,
                          u32 startIndex, s8 movingUnitId,
                          UnitType movingUnitType, u8 maxCost, bool trackSteps)
{
    const u32 cellCount = (u32)map->width * map->height;
    const u16 startX = (u16)(startIndex % map->width);
    const u16 startY = (u16)(startIndex / map->width);
    u16 minX;
    u16 maxX;
    u16 minY;
    u16 maxY;
    u16 y;
    u16 head = 0;
    u16 tail = 0;
    u16 queueCount = 0;

    if ((cellCount == 0) || (cellCount > PATHFINDER_MAX_CELLS))
        return FALSE;

    /* Todo paso cuesta al menos 1: fuera de este rectángulo ninguna casilla
       puede alcanzarse con maxCost. Limita los clears al área de búsqueda,
       en vez de escribir los 1.600 costes y padres por cada decisión de IA. */
    getSearchBounds(map, startX, startY, maxCost,
                    &minX, &maxX, &minY, &maxY);
    for (y = minY; y <= maxY; y++)
    {
        const u32 rowStart = (u32)y * map->width + minX;
        const u16 rowWidth = (u16)(maxX - minX + 1u);
        memset(&workspace->costs[rowStart], COST_UNREACHABLE, rowWidth);
        memset(&workspace->parentDirections[rowStart], 0, rowWidth);
    }
    if (trackSteps && (workspace->reachable != NULL))
        memset(workspace->reachable, 0, cellCount);

    workspace->costs[startIndex] = 0;
    if (trackSteps && (workspace->reachable != NULL))
        workspace->reachable[startIndex] = 1; /* pasos + 1 */

    searchQueue[tail++] = (u16)startIndex;
    if (tail == cellCount) tail = 0;
    queueCount++;
    workspace->parentDirections[startIndex] |= NODE_QUEUED;

    while (queueCount != 0)
    {
        const u32 current = searchQueue[head++];
        PathDirection direction;

        if (head == cellCount) head = 0;
        queueCount--;
        Performance_count(PERF_EVENT_PATH_NODES, 1);
        workspace->parentDirections[current] &= (u8)~NODE_QUEUED;

        /* Para rutas no se puede parar al descubrir el destino: una ruta de
           menor coste puede llegar a él desde otra rama posteriormente. */
        for (direction = PATH_DIR_UP; direction <= PATH_DIR_LEFT; direction++)
        {
            const u32 next = neighbourIndex(current, map->width,
                                            map->height, direction);
            u8 moveCost;
            u16 nextCost;
            u8 queuedFlag;

            if (next == 0xFFFFFFFFu) continue;
            if (!canTraverse(map, next, startIndex, movingUnitId, movingUnitType, &moveCost))
                continue;

            nextCost = (u16)workspace->costs[current] + moveCost;
            if ((nextCost > maxCost) || (nextCost >= COST_UNREACHABLE) ||
                (nextCost >= workspace->costs[next]))
                continue;

            queuedFlag = workspace->parentDirections[next] & NODE_QUEUED;
            workspace->costs[next] = (u8)nextCost;
            workspace->parentDirections[next] = (u8)(queuedFlag |
                (u8)opposite(direction));

            if (trackSteps && (workspace->reachable != NULL))
            {
                const u16 nextSteps = (u16)workspace->reachable[current] + 1u;
                workspace->reachable[next] = (nextSteps < 256u)
                    ? (u8)nextSteps : 0xFFu;
            }

            if ((workspace->parentDirections[next] & NODE_QUEUED) == 0)
            {
                /* Como cada índice está como máximo una vez en la cola, la
                   cola de cellCount elementos nunca puede desbordarse. */
                searchQueue[tail++] = (u16)next;
                if (tail == cellCount) tail = 0;
                queueCount++;
                workspace->parentDirections[next] |= NODE_QUEUED;
            }
        }
    }

    /* Las marcas de cola se han quitado al extraer todos los nodos. */
    return TRUE;
}

/* API pública de rutas: calcular ruta concreta solo al confirmar. */

bool Pathfinder_findRouteForType(const MapData *map,
                                 PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY,
                                 s8 movingUnitId, UnitType movingUnitType,
                                 u8 maxCost, u16 *stepCount, u8 *totalCost)
{
    const u32 cellCount = (map != NULL)
        ? (u32)map->width * map->height : 0;
    u32 startIndex;
    u32 targetIndex;
    u32 i;
    u8 ignoredCost;
    u16 steps = 0;

    if ((stepCount == NULL) || (totalCost == NULL)) return FALSE;
    *stepCount = 0;
    *totalCost = 0;

    if ((map == NULL) || (workspace == NULL) || (workspace->costs == NULL) ||
        (workspace->parentDirections == NULL) || (map->width == 0) ||
        (map->height == 0) || (maxCost == 0) || (maxCost == COST_UNREACHABLE) ||
        (startX >= map->width) || (startY >= map->height) ||
        (targetX >= map->width) || (targetY >= map->height) ||
        (cellCount > workspace->capacity) || (cellCount > map->cellCapacity) ||
        (cellCount > PATHFINDER_MAX_CELLS) ||
        (UnitType_getInfo(movingUnitType) == NULL))
        return FALSE;

    startIndex = (u32)startY * map->width + startX;
    targetIndex = (u32)targetY * map->width + targetX;
    if (!canTraverse(map, startIndex, startIndex, movingUnitId, movingUnitType,
                     &ignoredCost) ||
        !canTraverse(map, targetIndex, startIndex, movingUnitId, movingUnitType,
                     &ignoredCost))
        return FALSE;
    if (startIndex == targetIndex) return TRUE;
    /* Con costes de terreno positivos, una distancia Manhattan mayor al
       presupuesto es imposible: evita leer costes antiguos fuera del área. */
    {
        const u16 dx = (startX > targetX)
            ? (u16)(startX - targetX) : (u16)(targetX - startX);
        const u16 dy = (startY > targetY)
            ? (u16)(startY - targetY) : (u16)(targetY - startY);
        if ((u16)(dx + dy) > maxCost) return FALSE;
    }
    Performance_count(PERF_EVENT_ROUTE_SEARCH, 1);

    if (!runCostSearch(map, workspace, startIndex, movingUnitId, movingUnitType,
                       maxCost, FALSE))
        return FALSE;
    if (workspace->costs[targetIndex] == COST_UNREACHABLE) return FALSE;

    *totalCost = workspace->costs[targetIndex];

    /* Reconstruye desde el destino. costs[] se convierte en la secuencia de
       pasos invertida, preservando el contrato de Pathfinder_getStep(). */
    i = targetIndex;
    while (i != startIndex)
    {
        const PathDirection parent = (PathDirection)
            (workspace->parentDirections[i] & DIRECTION_MASK);
        u32 previous;

        if ((parent < PATH_DIR_UP) || (parent > PATH_DIR_LEFT) ||
            (steps >= cellCount) || (steps >= maxCost))
            return FALSE;

        workspace->costs[steps++] = (u8)opposite(parent);
        previous = neighbourIndex(i, map->width, map->height, parent);
        if (previous == 0xFFFFFFFFu) return FALSE;
        i = previous;
    }

    *stepCount = steps;
    return TRUE;
}

/* La API original conserva el coste clásico de infantería para pruebas y herramientas. */
bool Pathfinder_findRoute(const MapData *map, PathfinderWorkspace *workspace,
                          u16 startX, u16 startY, u16 targetX, u16 targetY,
                          s8 movingUnitId, u8 maxCost,
                          u16 *stepCount, u8 *totalCost)
{
    return Pathfinder_findRouteForType(map, workspace, startX, startY,
        targetX, targetY, movingUnitId, UNIT_TYPE_INFANTRY, maxCost,
        stepCount, totalCost);
}

/* Caché de alcanzabilidad y consultas O(1) de destino. */

bool Pathfinder_buildReachabilityForType(const MapData *map,
                                         PathfinderWorkspace *workspace,
                                         u16 startX, u16 startY,
                                         s8 movingUnitId,
                                         UnitType movingUnitType,
                                         u8 maxCost)
{
    const u32 cellCount = (map != NULL)
        ? (u32)map->width * map->height : 0;
    const u32 startIndex = (map != NULL)
        ? (u32)startY * map->width + startX : 0;
    u8 ignoredCost;

    if ((workspace == NULL) || (workspace->reachable == NULL) ||
        (workspace->costs == NULL) || (workspace->parentDirections == NULL) ||
        (map == NULL) || (map->cells == NULL) || (map->width == 0) ||
        (map->height == 0) || (startX >= map->width) ||
        (startY >= map->height) || (cellCount > workspace->capacity) ||
        (cellCount > map->cellCapacity) ||
        (UnitType_getInfo(movingUnitType) == NULL))
        return FALSE;

    if (maxCost == 0)
    {
        memset(workspace->reachable, 0, cellCount);
        return TRUE;
    }
    if ((maxCost == COST_UNREACHABLE) ||
        !canTraverse(map, startIndex, startIndex, movingUnitId, movingUnitType,
                     &ignoredCost))
        return FALSE;

    /* El mapa de juego cabe en la cola de 1600 índices. Para mapas de prueba
       o escenarios de herramientas mayores, conserva una ruta sin cola y
       acotada por el presupuesto, en vez de reservar más RAM global al juego. */
    if (cellCount > PATHFINDER_MAX_CELLS)
    {
        u16 minX, maxX, minY, maxY;
        u16 costLevel;
        getSearchBounds(map, startX, startY, maxCost,
                        &minX, &maxX, &minY, &maxY);
        memset(workspace->costs, COST_UNREACHABLE, cellCount);
        memset(workspace->parentDirections, 0, cellCount);
        workspace->costs[startIndex] = 0;
        workspace->reachable[startIndex] = 1;
        for (costLevel = 0; costLevel <= maxCost; costLevel++)
        {
            u16 y;
            for (y = minY; y <= maxY; y++)
            {
                u16 x;
                for (x = minX; x <= maxX; x++)
                {
                    const u32 current = (u32)y * map->width + x;
                    PathDirection direction;
                    if (workspace->costs[current] != costLevel) continue;
                    for (direction = PATH_DIR_UP; direction <= PATH_DIR_LEFT;
                         direction++)
                    {
                        const u32 next = neighbourIndex(current, map->width,
                                                        map->height, direction);
                        u8 moveCost;
                        if ((next == 0xFFFFFFFFu) ||
                            !canTraverse(map, next, startIndex, movingUnitId,
                                         movingUnitType, &moveCost)) continue;
                        if (((u16)costLevel + moveCost > maxCost) ||
                            ((u16)costLevel + moveCost >= COST_UNREACHABLE) ||
                            ((u16)costLevel + moveCost >= workspace->costs[next]))
                            continue;
                        workspace->costs[next] = (u8)(costLevel + moveCost);
                        workspace->parentDirections[next] = (u8)opposite(direction);
                        workspace->reachable[next] = (u8)(workspace->reachable[current] + 1u);
                    }
                }
            }
        }
        workspace->reachable[startIndex] = 0;
        return TRUE;
    }

    Performance_count(PERF_EVENT_REACHABILITY_BUILD, 1);
    if (!runCostSearch(map, workspace, startIndex, movingUnitId, movingUnitType,
                       maxCost, TRUE))
        return FALSE;

    /* El origen permite colocar el cursor, pero no se resalta como destino. */
    workspace->reachable[startIndex] = 0;
    return TRUE;
}

bool Pathfinder_buildReachability(const MapData *map,
                                  PathfinderWorkspace *workspace,
                                  u16 startX, u16 startY, s8 movingUnitId,
                                  u8 maxCost)
{
    return Pathfinder_buildReachabilityForType(map, workspace, startX, startY,
        movingUnitId, UNIT_TYPE_INFANTRY, maxCost);
}

bool Pathfinder_getReachableInfo(const MapData *map,
                                 const PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY, u8 maxCost,
                                 u16 *stepCount, u8 *totalCost)
{
    u32 cellCount;
    u32 startIndex;
    u32 targetIndex;
    u8 encodedSteps;
    u8 cost;

    if ((stepCount == NULL) || (totalCost == NULL)) return FALSE;
    *stepCount = 0;
    *totalCost = 0;

    if ((map == NULL) || (workspace == NULL) ||
        (workspace->costs == NULL) || (workspace->parentDirections == NULL) ||
        (workspace->reachable == NULL) || (map->width == 0) ||
        (map->height == 0) || (startX >= map->width) ||
        (startY >= map->height) || (targetX >= map->width) ||
        (targetY >= map->height) || (maxCost == 0) ||
        (maxCost == COST_UNREACHABLE))
        return FALSE;

    cellCount = (u32)map->width * map->height;
    if ((cellCount > workspace->capacity) || (cellCount > map->cellCapacity))
        return FALSE;

    startIndex = (u32)startY * map->width + startX;
    targetIndex = (u32)targetY * map->width + targetX;
    if (startIndex == targetIndex) return TRUE;

    /* reachable guarda pasos+1. El HUD puede consultar cualquier celda en
       O(1), sin reconstruir su cadena de padres cada vez que se mueve el cursor. */
    encodedSteps = workspace->reachable[targetIndex];
    if (encodedSteps == 0) return FALSE;

    cost = workspace->costs[targetIndex];
    if ((cost == COST_UNREACHABLE) || (cost > maxCost)) return FALSE;

    *stepCount = (u16)(encodedSteps - 1);
    *totalCost = cost;
    return TRUE;
}

bool Pathfinder_getReachableStep(const MapData *map,
                                 const PathfinderWorkspace *workspace,
                                 u16 startX, u16 startY,
                                 u16 targetX, u16 targetY, u8 maxCost,
                                 u16 stepIndex, u16 *stepX, u16 *stepY)
{
    u16 stepCount;
    u8 totalCost;
    u32 current;
    u16 stepsBack;

    if ((stepX == NULL) || (stepY == NULL) ||
        !Pathfinder_getReachableInfo(map, workspace, startX, startY,
                                     targetX, targetY, maxCost,
                                     &stepCount, &totalCost) ||
        (stepIndex >= stepCount))
        return FALSE;

    current = (u32)targetY * map->width + targetX;
    stepsBack = (u16)(stepCount - stepIndex - 1);
    while (stepsBack-- > 0)
    {
        const PathDirection parent = (PathDirection)
            (workspace->parentDirections[current] & DIRECTION_MASK);

        if ((parent < PATH_DIR_UP) || (parent > PATH_DIR_LEFT)) return FALSE;
        current = neighbourIndex(current, map->width, map->height, parent);
        if (current == 0xFFFFFFFFu) return FALSE;
    }

    *stepX = (u16)(current % map->width);
    *stepY = (u16)(current / map->width);
    return TRUE;
}

PathDirection Pathfinder_getStep(const PathfinderWorkspace *workspace,
                                 u16 stepCount, u16 stepIndex)
{
    if ((workspace == NULL) || (workspace->costs == NULL) ||
        (stepIndex >= stepCount) || (stepCount > workspace->capacity))
        return PATH_DIR_NONE;

    return (PathDirection)workspace->costs[stepCount - stepIndex - 1];
}
