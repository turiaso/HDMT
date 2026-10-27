/*
 * Presentación de unidades: sprites, carga/descarga y sincronización con cámara.
 * No modifica la lógica de ocupación del mapa; esa autoridad pertenece a UnitController.
 */
#include "unit_view.h"
#include <string.h>
#include "game_config.h"
#include "performance.h"
#include "resources.h"
#include "team.h"

/*
 * Proyección de las unidades lógicas a sprites SGDK. Gestiona el presupuesto SAT, la
 * caché visual, el índice espacial y los candidatos que entran por el borde del viewport.
 * Las unidades sin sprite asignado siguen existiendo y actuando en la simulación lógica.
 */

#define UNIT_VIEW_MAX_MAP_CELLS GAMEPLAY_MAP_MAX_CELLS
#define UNIT_VIEW_DIRTY_BYTES   ((UNIT_VIEW_MAX_MAP_CELLS + 7u) / 8u)

/* Bitset temporal pequeño: evita buscar cada unidad en toda la lista dirty. */
static u8 fogDirtyCellBits[UNIT_VIEW_DIRTY_BYTES];

/* Recursos gráficos, viewport y snapshots del estado lógico. */

/*
 * El viewport lógico se ajusta a celdas completas (312x216), pero el VDP
 * muestra 320x224 píxeles. No recortamos sprites en esas franjas parciales:
 * el terreno visible allí también debe poder mostrar sus unidades.
 */
static bool intersectsViewport(s32 x, s32 y)
{
    return (x < SCREEN_PIXELS_W) &&
           ((x + UNIT_SPRITE_SIZE_PIXELS) > 0) &&
           (y < SCREEN_PIXELS_H) &&
           ((y + UNIT_SPRITE_SIZE_PIXELS) > 0);
}

/* El sprite se crea fuera de pantalla, pero solo se muestra al entrar en ella. */
static bool setSpriteViewportVisibility(Sprite *sprite, s32 x, s32 y)
{
    const SpriteVisibility desired = intersectsViewport(x, y) ? VISIBLE : HIDDEN;

    if (SPR_getVisibility(sprite) != desired)
    {
        SPR_setVisibility(sprite, desired);
        return TRUE;
    }
    return FALSE;
}

/* Precarga una celda más allá de los límites físicos de la pantalla. */
static bool intersectsSpritePrefetchArea(s32 x, s32 y)
{
    const s32 margin = CELL_PIXELS;

    return (x < (SCREEN_PIXELS_W + margin)) &&
           ((x + UNIT_SPRITE_SIZE_PIXELS) > -margin) &&
           (y < (SCREEN_PIXELS_H + margin)) &&
           ((y + UNIT_SPRITE_SIZE_PIXELS) > -margin);
}

static const SpriteDefinition *getUnitSpriteDefinition(UnitType type)
{
    switch (type)
    {
        case UNIT_TYPE_INFANTRY: return &unit_soldier;
        case UNIT_TYPE_SCOUT:    return &unit_scout;
        case UNIT_TYPE_ENGINEER: return &unit_engineer;
        case UNIT_TYPE_TANK:     return &unit_tank;
        default:                 return NULL;
    }
}

/*
 * Los gráficos de unidad usan posiciones fijas de VRAM compartidas por tipo.
 * Cárgalos una sola vez por inicialización; cada Sprite no debe volver a
 * subir los mismos cuatro tiles al entrar/salir del viewport.
 */
static bool loadUnitTiles(const SpriteDefinition *definition, u16 tileIndex)
{
    Animation *animation;
    AnimationFrame *frame;
    TileSet *tileset;

    if ((definition == NULL) || (definition->numAnimation == 0) ||
        (definition->animations == NULL) ||
        (definition->animations[0] == NULL) ||
        (definition->maxNumTile == 0) ||
        (definition->maxNumTile > UNIT_SPRITE_TILE_COUNT))
        return FALSE;

    animation = definition->animations[0];
    if ((animation->numFrame == 0) || (animation->frames == NULL) ||
        (animation->frames[0] == NULL))
        return FALSE;

    frame = animation->frames[0];
    tileset = frame->tileset;
    if ((tileset == NULL) || (tileset->numTile == 0) ||
        (tileset->numTile > UNIT_SPRITE_TILE_COUNT))
        return FALSE;

    /* DMA_QUEUE difiere la transferencia hasta VBlank; el recurso es estático. */
    VDP_loadTileSet(tileset, tileIndex, DMA_QUEUE);
    return TRUE;
}

static bool loadAllUnitTiles(void)
{
    return loadUnitTiles(&unit_soldier,
                         UNIT_VRAM_TILE_INDEX +
                         UNIT_TYPE_INFANTRY * UNIT_SPRITE_TILE_COUNT) &&
           loadUnitTiles(&unit_scout,
                         UNIT_VRAM_TILE_INDEX +
                         UNIT_TYPE_SCOUT * UNIT_SPRITE_TILE_COUNT) &&
           loadUnitTiles(&unit_engineer,
                         UNIT_VRAM_TILE_INDEX +
                         UNIT_TYPE_ENGINEER * UNIT_SPRITE_TILE_COUNT) &&
           loadUnitTiles(&unit_tank,
                         UNIT_VRAM_TILE_INDEX +
                         UNIT_TYPE_TANK * UNIT_SPRITE_TILE_COUNT);
}

static bool unitStateChanged(const UnitView *view, const Unit *unit, u8 index)
{
    if (!view->stateCacheValid[index]) return TRUE;
    if (unit == NULL) return TRUE;

    return (view->lastUnitAlive[index] != unit->alive) ||
           (view->lastUnitX[index] != unit->x) ||
           (view->lastUnitY[index] != unit->y) ||
           (view->lastUnitTeam[index] != (u8)unit->team);
}

/* Índice enlazado por fila y columna para unidades del mapa. */

static bool spatialCellIsValid(const UnitView *view, u16 x, u16 y)
{
    return view->spatialIndexValid && (x < view->spatialMapWidth) &&
           (y < view->spatialMapHeight);
}

static void unlinkSpatialUnit(UnitView *view, u8 index, u16 x, u16 y)
{
    u8 previous;
    u8 next;

    if (!spatialCellIsValid(view, x, y)) return;

    previous = view->unitPrevColumn[index];
    next = view->unitNextColumn[index];
    if (previous != UNIT_VIEW_NO_INDEX) view->unitNextColumn[previous] = next;
    else if (view->unitColumnHead[x] == index) view->unitColumnHead[x] = next;
    if (next != UNIT_VIEW_NO_INDEX) view->unitPrevColumn[next] = previous;

    previous = view->unitPrevRow[index];
    next = view->unitNextRow[index];
    if (previous != UNIT_VIEW_NO_INDEX) view->unitNextRow[previous] = next;
    else if (view->unitRowHead[y] == index) view->unitRowHead[y] = next;
    if (next != UNIT_VIEW_NO_INDEX) view->unitPrevRow[next] = previous;

    view->unitNextColumn[index] = UNIT_VIEW_NO_INDEX;
    view->unitPrevColumn[index] = UNIT_VIEW_NO_INDEX;
    view->unitNextRow[index] = UNIT_VIEW_NO_INDEX;
    view->unitPrevRow[index] = UNIT_VIEW_NO_INDEX;
}

static void linkSpatialUnit(UnitView *view, u8 index, u16 x, u16 y)
{
    u8 head;

    if (!spatialCellIsValid(view, x, y)) return;

    head = view->unitColumnHead[x];
    view->unitPrevColumn[index] = UNIT_VIEW_NO_INDEX;
    view->unitNextColumn[index] = head;
    if (head != UNIT_VIEW_NO_INDEX) view->unitPrevColumn[head] = index;
    view->unitColumnHead[x] = index;

    head = view->unitRowHead[y];
    view->unitPrevRow[index] = UNIT_VIEW_NO_INDEX;
    view->unitNextRow[index] = head;
    if (head != UNIT_VIEW_NO_INDEX) view->unitPrevRow[head] = index;
    view->unitRowHead[y] = index;
}

static void rebuildSpatialIndex(UnitView *view)
{
    u8 i;

    if (view == NULL) return;
    view->spatialIndexValid = FALSE;
    if ((view->visibilityMap == NULL) ||
        (view->visibilityMap->width == 0) ||
        (view->visibilityMap->height == 0) ||
        (view->visibilityMap->width > UNIT_VIEW_MAX_MAP_SIDE) ||
        (view->visibilityMap->height > UNIT_VIEW_MAX_MAP_SIDE))
        return;

    view->spatialMapWidth = view->visibilityMap->width;
    view->spatialMapHeight = view->visibilityMap->height;
    for (i = 0; i < UNIT_VIEW_MAX_MAP_SIDE; i++)
    {
        view->unitColumnHead[i] = UNIT_VIEW_NO_INDEX;
        view->unitRowHead[i] = UNIT_VIEW_NO_INDEX;
    }
    for (i = 0; i < view->capacity; i++)
    {
        view->unitNextColumn[i] = UNIT_VIEW_NO_INDEX;
        view->unitPrevColumn[i] = UNIT_VIEW_NO_INDEX;
        view->unitNextRow[i] = UNIT_VIEW_NO_INDEX;
        view->unitPrevRow[i] = UNIT_VIEW_NO_INDEX;
    }

    view->spatialIndexValid = TRUE;
    for (i = 0; i < view->capacity; i++)
        if (view->stateCacheValid[i])
            linkSpatialUnit(view, i, view->lastUnitX[i], view->lastUnitY[i]);
}

static void cacheUnitState(UnitView *view, const Unit *unit, u8 index)
{
    if (unit == NULL)
    {
        if (view->stateCacheValid[index])
            unlinkSpatialUnit(view, index, view->lastUnitX[index],
                              view->lastUnitY[index]);
        view->stateCacheValid[index] = FALSE;
        view->lastFogVisible[index] = FALSE;
        return;
    }

    if (view->spatialIndexValid)
    {
        if (view->stateCacheValid[index])
        {
            if ((view->lastUnitX[index] != unit->x) ||
                (view->lastUnitY[index] != unit->y))
            {
                unlinkSpatialUnit(view, index, view->lastUnitX[index],
                                  view->lastUnitY[index]);
                linkSpatialUnit(view, index, unit->x, unit->y);
            }
        }
        else linkSpatialUnit(view, index, unit->x, unit->y);
    }

    view->lastUnitAlive[index] = unit->alive;
    view->lastUnitX[index] = unit->x;
    view->lastUnitY[index] = unit->y;
    view->lastUnitTeam[index] = (u8)unit->team;
    view->stateCacheValid[index] = TRUE;
}

static bool computeFogVisibility(const UnitView *view, const Unit *unit)
{
    if ((unit == NULL) || !unit->alive) return FALSE;
    if (!view->fogEnabled) return TRUE;

    /* En el turno espectador, ambas facciones comparten solo la presentación:
       una unidad se ve si su casilla está actualmente dentro de algún alcance. */
    if (view->sharedFogVision)
        return (view->visibilityMap != NULL) &&
            (MapData_getCombinedVisionState(view->visibilityMap,
                unit->x, unit->y) == MAP_VISION_VISIBLE);

    if (unit->team == view->viewerTeam) return TRUE;
    return (view->visibilityMap != NULL) &&
        (MapData_getVisionState(view->visibilityMap, (u8)view->viewerTeam,
                                unit->x, unit->y) == MAP_VISION_VISIBLE);
}

/* Mantiene una lista compacta de los sprites que realmente tienen instancia
   SGDK. La lista contiene como máximo MAX_UNIT_SPRITES elementos. */
/* Presupuesto de sprites y registro de instancias activas. */

static void registerActiveSprite(UnitView *view, u8 index)
{
    u8 i;

    for (i = 0; i < view->activeSpriteIndexCount; i++)
        if (view->activeSpriteIndices[i] == index) return;

    if (view->activeSpriteIndexCount < UNIT_VIEW_MAX_UNITS)
        view->activeSpriteIndices[view->activeSpriteIndexCount++] = index;
}

static void unregisterActiveSprite(UnitView *view, u8 index)
{
    u8 i;

    for (i = 0; i < view->activeSpriteIndexCount; i++)
    {
        if (view->activeSpriteIndices[i] == index)
        {
            const u8 last = (u8)(view->activeSpriteIndexCount - 1u);
            view->activeSpriteIndices[i] = view->activeSpriteIndices[last];
            view->activeSpriteIndexCount--;
            return;
        }
    }
}

static void releaseUnitSprite(UnitView *view, u8 index)
{
    Sprite *sprite;

    if ((view == NULL) || (index >= view->capacity)) return;
    sprite = view->sprites[index];
    if (sprite != NULL)
    {
        const bool spriteBudgetWasFull =
            view->activeSpriteCount >= MAX_UNIT_SPRITES;
        SPR_releaseSprite(sprite);
        view->sprites[index] = NULL;
        unregisterActiveSprite(view, index);
        if (view->activeSpriteCount > 0) view->activeSpriteCount--;
        /* Solo hace falta explorar unidades omitidas si la SAT estaba llena. */
        if (spriteBudgetWasFull) view->spriteBudgetRefreshPending = TRUE;
    }

    view->lastScreenX[index] = UNIT_VIEW_POSITION_HIDDEN;
    view->lastScreenY[index] = UNIT_VIEW_POSITION_HIDDEN;
}

static u8 unitRenderPriority(const UnitView *view,
                             const UnitController *controller, u8 index)
{
    if (controller->moving && (controller->movingIndex == (s16)index))
        return 4u;
    if (controller->selectedIndex == (s16)index) return 3u;
    if (view->lastUnitTeam[index] == (u8)view->viewerTeam) return 2u;
    return 1u;
}

static bool unitHasHigherRenderPriority(const UnitView *view,
                                        const UnitController *controller,
                                        u8 candidate, u8 other)
{
    const u8 candidatePriority = unitRenderPriority(view, controller, candidate);
    const u8 otherPriority = unitRenderPriority(view, controller, other);

    if (candidatePriority != otherPriority)
        return candidatePriority > otherPriority;
    /* Desempate estable: conserva antes los índices menores para evitar parpadeos. */
    return candidate < other;
}

/* Reserva una entrada de sprite sin superar las 80 entradas SAT del VDP,
   contando cursor, marcador y reserva de niebla. Si hace falta, sustituye
   una unidad de menor prioridad visual; todas siguen existiendo lógicamente. */
static bool reserveUnitSpriteSlot(UnitView *view,
                                 const UnitController *controller,
                                 u8 candidateIndex)
{
    u8 i;
    s16 weakestIndex = -1;

    if (view->activeSpriteCount < MAX_UNIT_SPRITES) return TRUE;

    /*
     * Solo se recorren los sprites con instancia activa. Antes se recorrían
     * las 120 unidades lógicas por cada candidato que entraba en pantalla; al
     * cruzar una columna con muchas unidades, eso repetía el mismo trabajo
     * hasta una vez por candidato. El índice compacto limita el barrido a 77.
     */
    for (i = 0; i < view->activeSpriteIndexCount; i++)
    {
        const u8 activeIndex = view->activeSpriteIndices[i];
        if ((activeIndex >= view->capacity) ||
            (view->sprites[activeIndex] == NULL))
            continue;
        if ((weakestIndex < 0) || unitHasHigherRenderPriority(
                view, controller, (u8)weakestIndex, activeIndex))
            weakestIndex = (s16)activeIndex;
    }

    if ((weakestIndex < 0) || !unitHasHigherRenderPriority(
            view, controller, candidateIndex, (u8)weakestIndex))
        return FALSE;

    releaseUnitSprite(view, (u8)weakestIndex);
    return TRUE;
}

static Sprite *createUnitSprite(UnitView *view,
                                const UnitController *controller,
                                const Unit *unit, u8 index,
                                s16 screenX, s16 screenY)
{
    const SpriteDefinition *definition = getUnitSpriteDefinition(unit->type);
    const u16 tileIndex = UNIT_VRAM_TILE_INDEX +
        (u16)unit->type * UNIT_SPRITE_TILE_COUNT;
    const TeamInfo *teamInfo = Team_getInfo(unit->team);
    const u16 attributes = TILE_ATTR_FULL(
        (teamInfo != NULL) ? teamInfo->paletteLine : PAL2,
        TRUE, FALSE, FALSE, tileIndex);
    Sprite *sprite;

    if ((definition == NULL) || (definition->maxNumTile == 0) ||
        (definition->maxNumTile > UNIT_SPRITE_TILE_COUNT) ||
        ((tileIndex + definition->maxNumTile) > CURSOR_VRAM_TILE_INDEX))
        return NULL;

    /* Limita instancias activas a las entradas SAT reservadas a unidades. */
    view->spriteAllocationFailed = FALSE;
    if (!reserveUnitSpriteSlot(view, controller, index)) return NULL;

    /* Los patrones se comparten por tipo; no se reserva VRAM por unidad. */
    /* Los tiles ya están cargados por tipo; no solicitar otra subida por sprite. */
    sprite = SPR_addSpriteEx(definition, screenX, screenY, attributes,
        SPR_FLAG_INSERT_HEAD);
    if (sprite == NULL)
    {
        view->spriteAllocationFailed = TRUE;
        view->spriteBudgetRefreshPending = TRUE;
        return NULL;
    }

    SPR_setDepth(sprite, SPR_MIN_DEPTH + 1);
    (void)setSpriteViewportVisibility(sprite, screenX, screenY);
    view->sprites[index] = sprite;
    view->activeSpriteCount++;
    registerActiveSprite(view, index);
    view->lastScreenX[index] = screenX;
    view->lastScreenY[index] = screenY;
    return sprite;
}

/* Coloca una unidad viva y visible dentro del viewport ampliado de precarga. */
/* Asignación priorizada de unidades visibles a la SAT del VDP. */

static bool updateUnitSprite(UnitView *view, const UnitController *controller,
                             u8 index, const Unit *unit,
                             const CameraCursorState *camera,
                             bool visibleByFog)
{
    Sprite *sprite;
    s32 worldX;
    s32 worldY;
    s32 screenX;
    s32 screenY;

    if ((unit == NULL) || !unit->alive || !visibleByFog)
    {
        const bool hadSprite = (view->sprites[index] != NULL);
        releaseUnitSprite(view, index);
        return hadSprite;
    }

    /* Solo la unidad que se anima necesita la posición subcelda del controlador. */
    if (controller->moving && (controller->movingIndex == (s16)index))
        UnitController_getRenderPosition(controller, index, &worldX, &worldY);
    else
    {
        worldX = (s32)unit->x * CELL_PIXELS;
        worldY = (s32)unit->y * CELL_PIXELS;
    }

    worldX += (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    worldY += (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    screenX = worldX - camera->cameraX;
    screenY = worldY - camera->cameraY;

    if (!intersectsSpritePrefetchArea(screenX, screenY))
    {
        const bool hadSprite = (view->sprites[index] != NULL);
        releaseUnitSprite(view, index);
        return hadSprite;
    }

    sprite = view->sprites[index];
    if (sprite == NULL)
    {
        sprite = createUnitSprite(view, controller, unit, index,
                                  (s16)screenX, (s16)screenY);
        /* Si falla, se reintentará cuando el sprite vuelva a ser necesario. */
        return sprite != NULL;
    }

    {
        bool changed = setSpriteViewportVisibility(sprite, screenX, screenY);
        if ((view->lastScreenX[index] != (s16)screenX) ||
            (view->lastScreenY[index] != (s16)screenY))
        {
            SPR_setPosition(sprite, (s16)screenX, (s16)screenY);
            view->lastScreenX[index] = (s16)screenX;
            view->lastScreenY[index] = (s16)screenY;
            changed = TRUE;
        }
        return changed;
    }
}

/* Devuelve el mejor candidato dentro del margen de precarga que todavía no
   tiene sprite. Se usa la caché y la prioridad se evalúa por registro. */
/* Candidatos pendientes ordenados mediante bitsets de prioridad. */

static s16 findBestVisibleSpriteCandidate(const UnitView *view,
                                         const UnitController *controller,
                                         const CameraCursorState *camera)
{
    const u8 count = UnitController_getCount(controller);
    const s16 offset = (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    s16 bestIndex = -1;
    u8 bestPriority = 0;
    u8 i;

    for (i = 0; i < count; i++)
    {
        s32 screenX;
        s32 screenY;
        u8 priority;

        if ((view->sprites[i] != NULL) || !view->stateCacheValid[i] ||
            !view->lastUnitAlive[i] || !view->lastFogVisible[i])
            continue;

        screenX = (s32)view->lastUnitX[i] * CELL_PIXELS + offset -
                  camera->cameraX;
        screenY = (s32)view->lastUnitY[i] * CELL_PIXELS + offset -
                  camera->cameraY;
        if (!intersectsSpritePrefetchArea(screenX, screenY)) continue;

        priority = unitRenderPriority(view, controller, i);
        if ((bestIndex < 0) || (priority > bestPriority) ||
            ((priority == bestPriority) && (i < (u8)bestIndex)))
        {
            bestIndex = (s16)i;
            bestPriority = priority;
        }
    }

    return bestIndex;
}

static void removePendingSpriteCandidate(UnitView *view, u8 index)
{
    u8 priority;
    u8 word;
    u16 bit;

    if ((index >= view->capacity) || !view->pendingSpriteCandidateQueued[index])
        return;
    priority = view->pendingSpriteCandidatePriority[index];
    word = (u8)(index >> 4);
    bit = (u16)(1u << (index & 15u));
    if ((priority > 0) && (priority < UNIT_VIEW_PRIORITY_LEVELS))
        view->pendingSpritePriorityBits[priority][word] &= (u16)~bit;
    view->pendingSpriteCandidateQueued[index] = FALSE;
    if (view->pendingSpriteCandidateCount > 0)
        view->pendingSpriteCandidateCount--;
}

static void addPendingSpriteCandidateAtPriority(UnitView *view, u8 index,
                                                u8 priority)
{
    const u8 word = (u8)(index >> 4);
    const u16 bit = (u16)(1u << (index & 15u));

    if ((index >= view->capacity) || (priority == 0) ||
        (priority >= UNIT_VIEW_PRIORITY_LEVELS))
        return;
    view->pendingSpritePriorityBits[priority][word] |= bit;
    view->pendingSpriteCandidatePriority[index] = priority;
    if (!view->pendingSpriteCandidateQueued[index])
    {
        view->pendingSpriteCandidateQueued[index] = TRUE;
        view->pendingSpriteCandidateCount++;
    }
}

static void reclassifyPendingSpriteCandidate(UnitView *view, u8 index,
                                            u8 priority)
{
    u8 oldPriority;
    u8 word;
    u16 bit;

    if ((index >= view->capacity) || !view->pendingSpriteCandidateQueued[index] ||
        (priority == 0) || (priority >= UNIT_VIEW_PRIORITY_LEVELS))
        return;
    oldPriority = view->pendingSpriteCandidatePriority[index];
    if (priority == oldPriority) return;
    word = (u8)(index >> 4);
    bit = (u16)(1u << (index & 15u));
    if ((oldPriority > 0) && (oldPriority < UNIT_VIEW_PRIORITY_LEVELS))
        view->pendingSpritePriorityBits[oldPriority][word] &= (u16)~bit;
    view->pendingSpritePriorityBits[priority][word] |= bit;
    view->pendingSpriteCandidatePriority[index] = priority;
}

static s16 findBestPendingSpriteCandidate(const UnitView *view)
{
    s16 priority;

    for (priority = (s16)UNIT_VIEW_PRIORITY_LEVELS - 1; priority > 0; priority--)
    {
        u8 word;
        for (word = 0; word < UNIT_VIEW_PENDING_WORDS; word++)
        {
            u16 bits = view->pendingSpritePriorityBits[priority][word];
            if (bits != 0)
            {
                u8 bitIndex = 0;
                while ((bits & 1u) == 0)
                {
                    bits >>= 1;
                    bitIndex++;
                }
                return (s16)((u16)word * 16u + bitIndex);
            }
        }
    }
    return -1;
}

static void clearPendingSpriteCandidates(UnitView *view)
{
    memset(view->pendingSpritePriorityBits, 0,
           sizeof(view->pendingSpritePriorityBits));
    memset(view->pendingSpriteCandidateQueued, 0,
           sizeof(view->pendingSpriteCandidateQueued));
    memset(view->pendingSpriteCandidatePriority, 0,
           sizeof(view->pendingSpriteCandidatePriority));
    view->pendingSpriteCandidateCount = 0;
}

static void queueSpriteCandidate(UnitView *view,
                                 const UnitController *controller,
                                 const CameraCursorState *camera,
                                 u8 index)
{
    const s16 offset = (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    s32 screenX;
    s32 screenY;
    u8 priority;

    if ((index >= view->capacity) || view->pendingSpriteCandidateQueued[index] ||
        (view->sprites[index] != NULL) || !view->stateCacheValid[index] ||
        !view->lastUnitAlive[index] || !view->lastFogVisible[index] ||
        (view->pendingSpriteCandidateCount >= view->capacity))
        return;

    screenX = (s32)view->lastUnitX[index] * CELL_PIXELS + offset -
              camera->cameraX;
    screenY = (s32)view->lastUnitY[index] * CELL_PIXELS + offset -
              camera->cameraY;
    if (!intersectsSpritePrefetchArea(screenX, screenY)) return;

    priority = unitRenderPriority(view, controller, index);
    addPendingSpriteCandidateAtPriority(view, index, priority);
    view->spriteBudgetRefreshPending = TRUE;
}

/* Resuelve solo candidatos que se encontraron en una fila/columna que entra
   en pantalla. Nunca recorre el ejército completo durante el scroll. */
static bool processPendingSpriteCandidate(UnitView *view,
                                          const UnitController *controller,
                                          const CameraCursorState *camera)
{
    const s16 offset = (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    const u8 maxStaleCandidatesPerFrame = 8u;
    u8 staleCandidates = 0;

    while (view->pendingSpriteCandidateCount > 0)
    {
        const s16 bestIndex = findBestPendingSpriteCandidate(view);
        u8 index;
        s32 screenX;
        s32 screenY;
        u8 priority;
        const Unit *unit;
        s32 renderX;
        s32 renderY;
        bool changed;

        if (bestIndex < 0)
        {
            clearPendingSpriteCandidates(view);
            break;
        }
        index = (u8)bestIndex;
        screenX = (s32)view->lastUnitX[index] * CELL_PIXELS + offset -
                  camera->cameraX;
        screenY = (s32)view->lastUnitY[index] * CELL_PIXELS + offset -
                  camera->cameraY;

        /* La entrada puede haberse quedado fuera del viewport durante un paneo
           rápido. Se retira en O(1), con un límite fijo para no crear otro pico. */
        if ((view->sprites[index] != NULL) || !view->stateCacheValid[index] ||
            !view->lastUnitAlive[index] || !view->lastFogVisible[index] ||
            !intersectsSpritePrefetchArea(screenX, screenY))
        {
            removePendingSpriteCandidate(view, index);
            staleCandidates++;
            if (staleCandidates >= maxStaleCandidatesPerFrame) break;
            continue;
        }

        /* La prioridad puede cambiar al empezar/terminar un movimiento o cambiar
           selección. Reclasifica esa entrada con coste constante y vuelve a elegir. */
        priority = unitRenderPriority(view, controller, index);
        if (priority != view->pendingSpriteCandidatePriority[index])
        {
            reclassifyPendingSpriteCandidate(view, index, priority);
            staleCandidates++;
            if (staleCandidates >= maxStaleCandidatesPerFrame) break;
            continue;
        }

        removePendingSpriteCandidate(view, index);
        unit = UnitController_getUnit(controller, index);
        if (unitStateChanged(view, unit, index))
            cacheUnitState(view, unit, index);
        view->lastFogVisible[index] = computeFogVisibility(view, unit);
        if ((unit == NULL) || !unit->alive || !view->lastFogVisible[index])
        {
            staleCandidates++;
            if (staleCandidates >= maxStaleCandidatesPerFrame) break;
            continue;
        }

        if (controller->moving && (controller->movingIndex == bestIndex))
            UnitController_getRenderPosition(controller, index, &renderX, &renderY);
        else
        {
            renderX = (s32)unit->x * CELL_PIXELS;
            renderY = (s32)unit->y * CELL_PIXELS;
        }
        renderX += offset;
        renderY += offset;
        if (!intersectsSpritePrefetchArea(renderX - camera->cameraX,
                                renderY - camera->cameraY))
        {
            staleCandidates++;
            if (staleCandidates >= maxStaleCandidatesPerFrame) break;
            continue;
        }

        view->spriteAllocationFailed = FALSE;
        changed = updateUnitSprite(view, controller, index, unit, camera,
                                   view->lastFogVisible[index]);
        if (!changed && !view->spriteAllocationFailed &&
            (view->activeSpriteCount >= MAX_UNIT_SPRITES))
        {
            /* La mejor candidata pendiente no supera la prioridad de la SAT llena;
               por orden, ninguna candidata restante puede ocupar su lugar. */
            clearPendingSpriteCandidates(view);
            view->spriteBudgetRefreshPending = FALSE;
        }
        else
            view->spriteBudgetRefreshPending =
                (view->pendingSpriteCandidateCount != 0) ||
                (view->activeSpriteCount < MAX_UNIT_SPRITES);

        /* Una nueva instancia como máximo por fotograma de scroll. */
        return changed;
    }

    /* No pierdas candidatos pendientes al alcanzar el límite de descartes
       obsoletos: el resto se procesa de forma acotada en el siguiente frame. */
    view->spriteBudgetRefreshPending =
        (view->pendingSpriteCandidateCount != 0) ||
        (view->activeSpriteCount < MAX_UNIT_SPRITES);
    return FALSE;
}

/* El desplazamiento puede hacer entrar muchas unidades en el margen de
   precarga a la vez. Crear y sustituir sprites en un solo frame provocaba picos
   de trabajo; procesamos el mejor candidato por frame y reposicionamos siempre
   los sprites existentes. Las unidades lógicas nunca se pierden. */
static bool refillUnitSpriteBudget(UnitView *view,
                                   const UnitController *controller,
                                   const CameraCursorState *camera)
{
    s16 candidateIndex;
    const Unit *unit;
    bool changed;

    if (view->pendingSpriteCandidateCount > 0)
        return processPendingSpriteCandidate(view, controller, camera);
    if (!view->spriteBudgetRefreshPending) return FALSE;
    candidateIndex = findBestVisibleSpriteCandidate(view, controller, camera);
    if (candidateIndex < 0)
    {
        view->spriteBudgetRefreshPending = FALSE;
        return FALSE;
    }

    unit = UnitController_getUnit(controller, (u8)candidateIndex);
    if (unit == NULL)
    {
        view->spriteBudgetRefreshPending = FALSE;
        return FALSE;
    }

    if (unitStateChanged(view, unit, (u8)candidateIndex))
    {
        cacheUnitState(view, unit, (u8)candidateIndex);
        view->lastFogVisible[(u8)candidateIndex] = computeFogVisibility(view,
                                                                       unit);
    }
    if (!unit->alive || !view->lastFogVisible[(u8)candidateIndex])
    {
        /* El siguiente frame elegirá el siguiente candidato tras actualizar
           la caché del primero que ha cambiado mientras estaba fuera de vista. */
        view->spriteBudgetRefreshPending =
            (view->activeSpriteCount < MAX_UNIT_SPRITES);
        return FALSE;
    }

    view->spriteAllocationFailed = FALSE;
    changed = updateUnitSprite(view, controller, (u8)candidateIndex, unit,
                               camera, view->lastFogVisible[(u8)candidateIndex]);

    /* Un solo alta/sustitución por fotograma. Si se ha realizado, permite
       que el siguiente frame tramite otro candidato; cuando la SAT está llena
       y ninguno puede ganar prioridad, el intento devuelve FALSE y se detiene. */
    if (view->spriteAllocationFailed)
        view->spriteBudgetRefreshPending = FALSE;
    else
        view->spriteBudgetRefreshPending = changed;

    return changed;
}

/*
 * Ancla de forma explícita el sprite al centro de la casilla lógica cuando
 * termina una orden. No depende de que lastScreenX/Y coincidan por casualidad
 * con la última posición subcelda de la animación.
 */
/* Scroll incremental y ajuste exacto al terminar el movimiento. */

static bool snapUnitSpriteToCell(UnitView *view,
                                 const UnitController *controller,
                                 u8 index, const Unit *unit,
                                 const CameraCursorState *camera,
                                 bool visibleByFog)
{
    bool changed = updateUnitSprite(view, controller, index, unit, camera,
                                   visibleByFog);
    Sprite *sprite;
    s32 screenX;
    s32 screenY;

    if ((unit == NULL) || !unit->alive || !visibleByFog ||
        (view->sprites[index] == NULL))
        return changed;

    /* Usa siempre la coordenada lógica, nunca el offset de la animación. */
    screenX = (s32)unit->x * CELL_PIXELS +
        (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2 - camera->cameraX;
    screenY = (s32)unit->y * CELL_PIXELS +
        (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2 - camera->cameraY;
    if (!intersectsSpritePrefetchArea(screenX, screenY)) return changed;

    sprite = view->sprites[index];
    SPR_setPosition(sprite, (s16)screenX, (s16)screenY);
    view->lastScreenX[index] = (s16)screenX;
    view->lastScreenY[index] = (s16)screenY;
    (void)setSpriteViewportVisibility(sprite, screenX, screenY);
    return TRUE;
}

/*
 * En un scroll sin cambios de unidades ni de niebla, la caché lógica ya es
 * suficiente. Reposiciona sprites activos desde ella y consulta al controlador
 * solo para unidades nuevas que entran al viewport.
 */
static bool updateCachedCameraUnit(UnitView *view,
                                   const UnitController *controller,
                                   const CameraCursorState *camera,
                                   u8 index,
                                   bool inspectOffscreenUnits)
{
    s32 worldX;
    s32 worldY;
    s32 screenX;
    s32 screenY;
    Sprite *sprite = view->sprites[index];
    const bool isMovingUnit = controller->moving &&
        (controller->movingIndex == (s16)index);

    /* La unidad móvil puede entrar/salir del viewport entre dos celdas. */
    if (isMovingUnit)
    {
        const Unit *unit = UnitController_getUnit(controller, index);
        if (unitStateChanged(view, unit, index))
            cacheUnitState(view, unit, index);
        return updateUnitSprite(view, controller, index, unit, camera,
                                view->lastFogVisible[index]);
    }

    /* Si el scroll no cruza un umbral de entrada/salida, una unidad sin
       sprite no puede aparecer repentinamente dentro del viewport. */
    if ((sprite == NULL) && !inspectOffscreenUnits) return FALSE;

    if (!view->stateCacheValid[index] || !view->lastUnitAlive[index] ||
        !view->lastFogVisible[index])
    {
        if (sprite != NULL)
        {
            releaseUnitSprite(view, index);
            return TRUE;
        }
        return FALSE;
    }

    worldX = (s32)view->lastUnitX[index] * CELL_PIXELS +
             (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    worldY = (s32)view->lastUnitY[index] * CELL_PIXELS +
             (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    screenX = worldX - camera->cameraX;
    screenY = worldY - camera->cameraY;

    if (!intersectsSpritePrefetchArea(screenX, screenY))
    {
        if (sprite != NULL)
        {
            releaseUnitSprite(view, index);
            return TRUE;
        }
        return FALSE;
    }

    if (sprite == NULL)
    {
        const Unit *unit = UnitController_getUnit(controller, index);
        return (unit != NULL) && updateUnitSprite(view, controller, index,
                                                  unit, camera, TRUE);
    }

    if ((view->lastScreenX[index] != (s16)screenX) ||
        (view->lastScreenY[index] != (s16)screenY))
    {
        SPR_setPosition(sprite, (s16)screenX, (s16)screenY);
        view->lastScreenX[index] = (s16)screenX;
        view->lastScreenY[index] = (s16)screenY;
        (void)setSpriteViewportVisibility(sprite, screenX, screenY);
        return TRUE;
    }

    return FALSE;
}

/* Añade candidatos de celdas que entran en el margen de precarga por el borde.
   La cola deduplica unidades que cruzan una esquina diagonal. */
static void queueCandidatesInEdgeCell(UnitView *view,
                                      const UnitController *controller,
                                      const CameraCursorState *camera,
                                      u16 cell,
                                      bool column)
{
    u8 index;

    if (view->spatialIndexValid)
    {
        if (column)
        {
            if (cell >= view->spatialMapWidth) return;
            index = view->unitColumnHead[cell];
        }
        else
        {
            if (cell >= view->spatialMapHeight) return;
            index = view->unitRowHead[cell];
        }

        while (index != UNIT_VIEW_NO_INDEX)
        {
            const u8 next = column ? view->unitNextColumn[index] :
                                     view->unitNextRow[index];
            Performance_count(PERF_EVENT_UNIT_RECORDS, 1);
            queueSpriteCandidate(view, controller, camera, index);
            index = next;
        }
    }
    else
    {
        /* Fallback para tests/mapas fuera de las dimensiones indexadas. Solo se
           usa al entrar una nueva fila/columna, nunca en cada pixel de scroll. */
        u8 i;
        Performance_count(PERF_EVENT_UNIT_RECORDS, view->capacity);
        for (i = 0; i < view->capacity; i++)
        {
            if (!view->stateCacheValid[i]) continue;
            if ((column ? view->lastUnitX[i] : view->lastUnitY[i]) == cell)
                queueSpriteCandidate(view, controller, camera, i);
        }
    }
}

static void queueIncomingCellsForAxis(UnitView *view,
                                      const UnitController *controller,
                                      const CameraCursorState *camera,
                                      s16 oldPosition, s16 newPosition,
                                      u16 viewportExtent, bool column)
{
    const s32 offset = (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2;
    s32 boundary;
    u16 residue;

    if (newPosition > oldPosition)
    {
        /* Cámara hacia la derecha/abajo: entran celdas por el borde opuesto. */
        residue = (u16)((offset + CELL_PIXELS -
                         (viewportExtent % CELL_PIXELS)) % CELL_PIXELS);
        boundary = ((s32)oldPosition / CELL_PIXELS) * CELL_PIXELS + residue;
        if (boundary < oldPosition) boundary += CELL_PIXELS;
        while (boundary < newPosition)
        {
            const s32 cell = (boundary + viewportExtent - offset) / CELL_PIXELS;
            if ((cell >= 0) && (cell <= 0xFFFF))
            {
                queueCandidatesInEdgeCell(view, controller, camera, (u16)cell, column);
                /* Precalienta también la siguiente celda: una unidad puede
                   crear su sprite mientras aún está fuera de pantalla. */
                if (cell < 0xFFFF)
                    queueCandidatesInEdgeCell(view, controller, camera,
                                              (u16)(cell + 1), column);
            }
            boundary += CELL_PIXELS;
        }
    }
    else if (newPosition < oldPosition)
    {
        /* Cámara hacia la izquierda/arriba: entran celdas por el borde próximo. */
        residue = (u16)((offset + UNIT_SPRITE_SIZE_PIXELS) % CELL_PIXELS);
        boundary = ((s32)oldPosition / CELL_PIXELS) * CELL_PIXELS + residue;
        if (boundary > oldPosition) boundary -= CELL_PIXELS;
        while (boundary > newPosition)
        {
            const s32 cell = (boundary - offset - UNIT_SPRITE_SIZE_PIXELS) /
                             CELL_PIXELS;
            if ((cell >= 0) && (cell <= 0xFFFF))
            {
                queueCandidatesInEdgeCell(view, controller, camera, (u16)cell, column);
                /* Al desplazar la vista en sentido contrario, precarga la
                   celda adyacente por el borde de entrada izquierdo/superior. */
                if (cell > 0)
                    queueCandidatesInEdgeCell(view, controller, camera,
                                              (u16)(cell - 1), column);
            }
            boundary -= CELL_PIXELS;
        }
    }
}

/* En scroll normal solo se visitan sprites activos (<=77). Los nuevos sprites
   se buscan mediante un índice por fila/columna cuando cruzan el viewport; ya
   no se recorren las 128 unidades lógicas cada 2-3 frames al mover la cámara. */
static bool updateCameraFromCache(UnitView *view,
                                  const UnitController *controller,
                                  const CameraCursorState *camera)
{
    const s16 oldCameraX = view->lastCameraX;
    const s16 oldCameraY = view->lastCameraY;
    const s16 deltaX = camera->cameraX - oldCameraX;
    const s16 deltaY = camera->cameraY - oldCameraY;
    const s16 movingIndex = controller->movingIndex;
    const bool movingSpriteWasActive = controller->moving &&
        (movingIndex >= 0) && (movingIndex < view->capacity) &&
        (view->sprites[(u8)movingIndex] != NULL);
    bool spriteChanged = FALSE;
    u8 slot = 0;
    const u16 recordsToVisit = (u16)view->activeSpriteIndexCount +
        ((controller->moving && !movingSpriteWasActive) ? 1u : 0u);

    (void)recordsToVisit;
    Performance_count(PERF_EVENT_UNIT_RECORDS, recordsToVisit);
    while (slot < view->activeSpriteIndexCount)
    {
        const u8 index = view->activeSpriteIndices[slot];
        const bool hadSprite = (view->sprites[index] != NULL);
        const bool isMovingUnit = controller->moving &&
            (controller->movingIndex == (s16)index);

        if (isMovingUnit)
        {
            if (updateCachedCameraUnit(view, controller, camera, index, FALSE))
                spriteChanged = TRUE;
        }
        else
        {
            const s32 screenX = (s32)view->lastScreenX[index] - deltaX;
            const s32 screenY = (s32)view->lastScreenY[index] - deltaY;
            Sprite *sprite = view->sprites[index];

            if ((sprite == NULL) || !intersectsSpritePrefetchArea(screenX, screenY))
            {
                if (sprite != NULL) spriteChanged = TRUE;
                releaseUnitSprite(view, index);
            }
            else if ((view->lastScreenX[index] != (s16)screenX) ||
                     (view->lastScreenY[index] != (s16)screenY))
            {
                SPR_setPosition(sprite, (s16)screenX, (s16)screenY);
                view->lastScreenX[index] = (s16)screenX;
                view->lastScreenY[index] = (s16)screenY;
                if (setSpriteViewportVisibility(sprite, screenX, screenY))
                    spriteChanged = TRUE;
                spriteChanged = TRUE;
            }
        }

        /* La liberación compacta intercambia el último índice con este slot. */
        if (hadSprite && (view->sprites[index] == NULL)) continue;
        slot++;
    }

    if (controller->moving && !movingSpriteWasActive && (movingIndex >= 0) &&
        (movingIndex < view->capacity) &&
        (view->sprites[(u8)movingIndex] == NULL))
    {
        const u8 index = (u8)movingIndex;
        const Unit *unit = UnitController_getUnit(controller, index);
        if (unitStateChanged(view, unit, index)) cacheUnitState(view, unit, index);
        if (updateUnitSprite(view, controller, index, unit, camera,
                             view->lastFogVisible[index]))
            spriteChanged = TRUE;
    }

    /* La indexación hace que el coste dependa de las unidades de las celdas
       entrantes, no de las 128 unidades totales ni de los sprites ya activos. */
    queueIncomingCellsForAxis(view, controller, camera, oldCameraX, camera->cameraX,
                              VIEW_PIXELS_W, TRUE);
    queueIncomingCellsForAxis(view, controller, camera, oldCameraY, camera->cameraY,
                              VIEW_PIXELS_H, FALSE);
    if (view->pendingSpriteCandidateCount > 0)
    {
        if (processPendingSpriteCandidate(view, controller, camera))
            spriteChanged = TRUE;
    }

    view->lastCameraX = camera->cameraX;
    view->lastCameraY = camera->cameraY;
    view->cameraCacheValid = TRUE;
    return spriteChanged;
}

/* Ciclo de vida público y actualización por frame. */

static bool isFogDirtyCell(const UnitView *view, const Unit *unit)
{
    u32 cellIndex;

    if ((view == NULL) || (unit == NULL) ||
        !view->fogDirtyCellsPending || (view->fogDirtyMapWidth == 0))
        return FALSE;

    cellIndex = (u32)unit->y * view->fogDirtyMapWidth + unit->x;
    if (cellIndex >= UNIT_VIEW_MAX_MAP_CELLS) return FALSE;
    return (fogDirtyCellBits[cellIndex >> 3] &
            (u8)(1u << (cellIndex & 7u))) != 0;
}

static bool updateUnitsForDirtyFogCells(UnitView *view,
                                        const UnitController *controller,
                                        const CameraCursorState *camera)
{
    const u8 count = UnitController_getCount(controller);
    const u32 cellCount = (u32)view->fogDirtyMapWidth *
                          (view->visibilityMap != NULL
                              ? view->visibilityMap->height : 0);
    bool spriteChanged = FALSE;
    u16 i;
    u8 unitIndex;

    memset(fogDirtyCellBits, 0, sizeof(fogDirtyCellBits));
    if ((view->fogDirtyCellIndices == NULL) ||
        (view->fogDirtyMapWidth == 0) ||
        (cellCount > UNIT_VIEW_MAX_MAP_CELLS))
        return FALSE;

    for (i = 0; i < view->fogDirtyCellCount; i++)
    {
        const u16 cellIndex = view->fogDirtyCellIndices[i];
        if (cellIndex >= cellCount) continue;
        fogDirtyCellBits[cellIndex >> 3] |=
            (u8)(1u << (cellIndex & 7u));
    }

    /* No cambies posiciones que no hayan variado; para la niebla basta con
       consultar unidades cuyo tile fue afectado o cuyo estado lógico cambió. */
    for (unitIndex = 0; unitIndex < view->capacity; unitIndex++)
    {
        const Unit *unit = (unitIndex < count)
            ? UnitController_getUnit(controller, unitIndex) : NULL;
        const bool stateChanged = unitStateChanged(view, unit, unitIndex);
        const bool movingUnit = controller->moving &&
            (controller->movingIndex == (s16)unitIndex);
        bool visibilityChanged = FALSE;

        if (unit == NULL)
        {
            if (view->sprites[unitIndex] != NULL)
            {
                releaseUnitSprite(view, unitIndex);
                spriteChanged = TRUE;
            }
            cacheUnitState(view, NULL, unitIndex);
            continue;
        }

        if (stateChanged || isFogDirtyCell(view, unit))
        {
            const bool nextVisible = computeFogVisibility(view, unit);
            visibilityChanged = nextVisible != view->lastFogVisible[unitIndex];
            view->lastFogVisible[unitIndex] = nextVisible;
        }

        if (stateChanged || movingUnit || visibilityChanged)
        {
            if (updateUnitSprite(view, controller, unitIndex, unit, camera,
                                 view->lastFogVisible[unitIndex]))
                spriteChanged = TRUE;
        }
        if (stateChanged) cacheUnitState(view, unit, unitIndex);
    }

    return spriteChanged;
}


void UnitView_release(UnitView *view)
{
    u8 i;

    if (view == NULL) return;
    if (view->sprites != NULL)
    {
        for (i = 0; i < view->capacity; i++)
        {
            if (view->sprites[i] != NULL)
            {
                SPR_releaseSprite(view->sprites[i]);
                view->sprites[i] = NULL;
            }
            view->lastScreenX[i] = UNIT_VIEW_POSITION_INVALID;
            view->lastScreenY[i] = UNIT_VIEW_POSITION_INVALID;
            view->stateCacheValid[i] = FALSE;
            view->lastFogVisible[i] = FALSE;
        }
    }
    view->sprites = NULL;
    view->capacity = 0;
    view->activeSpriteCount = 0;
    view->activeSpriteIndexCount = 0;
    view->spatialIndexValid = FALSE;
    view->spatialMapWidth = 0;
    view->spatialMapHeight = 0;
    clearPendingSpriteCandidates(view);
    view->spriteAllocationFailed = FALSE;
    view->spriteBudgetRefreshPending = FALSE;
    view->cameraCacheValid = FALSE;
    view->lastControllerMoving = FALSE;
    view->lastMovingIndex = -1;
    view->forceRefresh = FALSE;
    view->fogRefreshPending = FALSE;
    view->fogDirtyCellIndices = NULL;
    view->fogDirtyCellCount = 0;
    view->fogDirtyMapWidth = 0;
    view->fogDirtyCellsPending = FALSE;
}

bool UnitView_init(UnitView *view, const UnitController *controller,
                   Sprite **spriteStorage, u8 spriteCapacity)
{
    const CameraCursorState camera = CameraCursor_getState();
    u8 count;
    u8 i;

    if ((view == NULL) || (controller == NULL) ||
        ((spriteCapacity > 0) && (spriteStorage == NULL)) ||
        (spriteCapacity > UNIT_VIEW_MAX_UNITS) ||
        (UNIT_VRAM_TILE_INDEX + UNIT_VRAM_TILE_COUNT > CURSOR_VRAM_TILE_INDEX) ||
        (CURSOR_VRAM_TILE_INDEX >= VRAM_TILE_COUNT) ||
        ((CURSOR_VRAM_TILE_INDEX + cell_cursor.maxNumTile) > VRAM_TILE_COUNT))
        return FALSE;

    count = UnitController_getCount(controller);
    if (spriteCapacity < count) return FALSE;
    if (!loadAllUnitTiles()) return FALSE;

    memset(view, 0, sizeof(*view));
    view->sprites = spriteStorage;
    view->capacity = spriteCapacity;
    view->activeSpriteCount = 0;
    view->activeSpriteIndexCount = 0;
    view->spriteAllocationFailed = FALSE;
    view->spriteBudgetRefreshPending = FALSE;
    view->visibilityMap = NULL;
    view->viewerTeam = TEAM_BLUE;
    view->fogEnabled = FALSE;
    view->forceRefresh = FALSE;
    view->fogRefreshPending = FALSE;
    view->fogDirtyCellIndices = NULL;
    view->fogDirtyCellCount = 0;
    view->fogDirtyMapWidth = 0;
    view->fogDirtyCellsPending = FALSE;
    view->lastMovingIndex = -1;
    for (i = 0; i < spriteCapacity; i++)
    {
        spriteStorage[i] = NULL;
        view->lastScreenX[i] = UNIT_VIEW_POSITION_INVALID;
        view->lastScreenY[i] = UNIT_VIEW_POSITION_INVALID;
        view->stateCacheValid[i] = FALSE;
        view->lastFogVisible[i] = FALSE;
    }

    for (i = 0; i < count; i++)
    {
        const Unit *unit = UnitController_getUnit(controller, i);

        cacheUnitState(view, unit, i);
        view->lastFogVisible[i] = computeFogVisibility(view, unit);
        (void)updateUnitSprite(view, controller, i, unit, &camera,
                               view->lastFogVisible[i]);
        /* No tener sprite puede ser correcto si se agotó el presupuesto SAT.
           Solo se aborta por un fallo real de asignación de SGDK. */
        if (view->spriteAllocationFailed) goto allocationFailed;
    }

    view->lastCameraX = camera.cameraX;
    view->lastCameraY = camera.cameraY;
    view->cameraCacheValid = TRUE;
    view->spriteBudgetRefreshPending = FALSE;
    return TRUE;

allocationFailed:
    UnitView_release(view);
    return FALSE;
}

bool UnitView_update(UnitView *view, const UnitController *controller,
                     const CameraCursorState *camera)
{
    const u8 count = (view != NULL && controller != NULL)
        ? UnitController_getCount(controller) : 0;
    const bool cameraChanged = (view != NULL) &&
        (!view->cameraCacheValid || (camera != NULL &&
         ((camera->cameraX != view->lastCameraX) ||
          (camera->cameraY != view->lastCameraY))));
    /*
     * Un final de movimiento también puede significar que un tramo fue
     * rechazado y que el controlador ha vuelto a la casilla lógica anterior.
     * En ambos casos la posición dibujada del último fotograma puede ser
     * intermedia, aunque las coordenadas lógicas no hayan cambiado. Detecta
     * la transición moving -> idle antes de actualizar la caché y fuerza una
     * pasada de sincronización para volver a anclar el sprite a la cuadrícula.
     */
    const bool movementEnded = (view != NULL) && (controller != NULL) &&
        view->lastControllerMoving && !controller->moving;
    const bool explicitForceRefresh = (view != NULL) && view->forceRefresh;
    const bool forceRefresh = explicitForceRefresh || movementEnded;
    const bool fogRefreshPending = (view != NULL) && view->fogRefreshPending;
    const s16 endedIndex = (view != NULL) ? view->lastMovingIndex : -1;
    bool spriteChanged = FALSE;
    u8 i;

    if ((view == NULL) || (controller == NULL) || (camera == NULL)) return FALSE;

    view->lastControllerMoving = controller->moving;
    if (controller->moving && (controller->movingIndex >= 0) &&
        (controller->movingIndex < view->capacity))
        view->lastMovingIndex = controller->movingIndex;
    else if (!movementEnded)
        view->lastMovingIndex = -1;

    /*
     * La niebla incremental proporciona las celdas afectadas. Actualiza solo
     * las unidades situadas en ellas (y cualquier unidad cuyo estado lógico
     * haya cambiado), en vez de volver a calcular la visibilidad de todo el
     * ejército cada vez que una unidad cruza una casilla.
     */
    if (view->fogDirtyCellsPending && !explicitForceRefresh)
    {
        spriteChanged = updateUnitsForDirtyFogCells(view, controller, camera);

        /* Un final de movimiento requiere un snap explícito a la celda lógica. */
        if (movementEnded && (endedIndex >= 0) && (endedIndex < count) &&
            (endedIndex < view->capacity))
        {
            const u8 movingIndex = (u8)endedIndex;
            const Unit *unit = UnitController_getUnit(controller, movingIndex);
            const bool visible = computeFogVisibility(view, unit);
            if (unitStateChanged(view, unit, movingIndex))
                cacheUnitState(view, unit, movingIndex);
            view->lastFogVisible[movingIndex] = visible;
            if (snapUnitSpriteToCell(view, controller, movingIndex, unit,
                                     camera, visible))
                spriteChanged = TRUE;
            view->lastMovingIndex = -1;
        }

        if (cameraChanged)
        {
            spriteChanged |= updateCameraFromCache(view, controller, camera);
        }
        else
        {
            view->lastCameraX = camera->cameraX;
            view->lastCameraY = camera->cameraY;
            view->cameraCacheValid = TRUE;
        }
        view->forceRefresh = FALSE;
        view->fogRefreshPending = FALSE;
        view->fogDirtyCellIndices = NULL;
        view->fogDirtyCellCount = 0;
        view->fogDirtyMapWidth = 0;
        view->fogDirtyCellsPending = FALSE;
        if (!cameraChanged)
            spriteChanged |= refillUnitSpriteBudget(view, controller, camera);
        return spriteChanged;
    }

    /* Al terminar sin cambios de cámara/niebla, basta con ajustar el sprite
       que se movía. No hagas un barrido de todas las unidades en ese fotograma. */
    if (movementEnded && !cameraChanged && !explicitForceRefresh &&
        !fogRefreshPending && (endedIndex >= 0) && (endedIndex < count) &&
        (endedIndex < view->capacity))
    {
        const u8 movingIndex = (u8)endedIndex;
        const Unit *unit = UnitController_getUnit(controller, movingIndex);
        const bool visible = computeFogVisibility(view, unit);
        if (unitStateChanged(view, unit, movingIndex))
            cacheUnitState(view, unit, movingIndex);
        view->lastFogVisible[movingIndex] = visible;
        spriteChanged = updateUnitSprite(view, controller, movingIndex,
            unit, camera, visible);
        view->lastMovingIndex = -1;
        view->forceRefresh = FALSE;
        view->fogRefreshPending = FALSE;
        spriteChanged |= refillUnitSpriteBudget(view, controller, camera);
        return spriteChanged;
    }

    /* En reposo no hace falta volver a consultar todos los registros cada frame. */
    if (!cameraChanged && !forceRefresh && !fogRefreshPending &&
        !controller->moving)
        return refillUnitSpriteBudget(view, controller, camera);

    /* El scroll usa la caché lógica; evita releer y validar todas las unidades. */
    if (cameraChanged && !forceRefresh && !fogRefreshPending)
    {
        spriteChanged = updateCameraFromCache(view, controller, camera);
        return spriteChanged;
    }

    /*
     * Durante una animación sin cambio de cámara ni niebla, solo puede cambiar
     * la representación de la unidad en movimiento. No recorras el resto de los
     * registros lógicos en cada fotograma subcelda.
     */
    if (!cameraChanged && !forceRefresh && !fogRefreshPending &&
        controller->moving && (controller->movingIndex >= 0) &&
        (controller->movingIndex < count) &&
        (controller->movingIndex < view->capacity))
    {
        const u8 movingIndex = (u8)controller->movingIndex;
        const Unit *movingUnit = UnitController_getUnit(controller, movingIndex);
        bool changed = FALSE;

        if (movingUnit == NULL)
        {
            if (view->sprites[movingIndex] != NULL)
            {
                releaseUnitSprite(view, movingIndex);
                changed = TRUE;
            }
            cacheUnitState(view, NULL, movingIndex);
        }
        else
        {
            if (unitStateChanged(view, movingUnit, movingIndex))
                cacheUnitState(view, movingUnit, movingIndex);
            changed = updateUnitSprite(view, controller, movingIndex,
                movingUnit, camera, view->lastFogVisible[movingIndex]);
        }
        changed |= refillUnitSpriteBudget(view, controller, camera);
        return changed;
    }

    Performance_count(PERF_EVENT_UNIT_RECORDS, view->capacity);
    for (i = 0; i < view->capacity; i++)
    {
        const Unit *unit = (i < count) ? UnitController_getUnit(controller, i) : NULL;
        const bool stateChanged = unitStateChanged(view, unit, i);
        const bool refreshFog = forceRefresh || fogRefreshPending || stateChanged;
        bool visibleByFog;
        bool fogVisibilityChanged = FALSE;
        const bool isMovingUnit = controller->moving &&
            (controller->movingIndex == (s16)i);

        if (unit == NULL)
        {
            if (view->sprites[i] != NULL)
            {
                releaseUnitSprite(view, i);
                spriteChanged = TRUE;
            }
            cacheUnitState(view, NULL, i);
            continue;
        }

        if (refreshFog)
        {
            const bool nextFogVisible = computeFogVisibility(view, unit);
            fogVisibilityChanged = nextFogVisible != view->lastFogVisible[i];
            view->lastFogVisible[i] = nextFogVisible;
        }
        visibleByFog = view->lastFogVisible[i];

        /*
         * Un cambio incremental de niebla solo vuelve a consultar la visión;
         * no obliga a calcular de nuevo la posición de todas las unidades.
         * Solo se actualiza un sprite si cambió su posición o su visibilidad.
         */
        if (cameraChanged || forceRefresh || stateChanged || isMovingUnit ||
            fogVisibilityChanged)
        {
            if (updateUnitSprite(view, controller, i, unit, camera,
                                 visibleByFog))
                spriteChanged = TRUE;
        }

        if (stateChanged) cacheUnitState(view, unit, i);
    }

    view->lastCameraX = camera->cameraX;
    view->lastCameraY = camera->cameraY;
    view->cameraCacheValid = TRUE;
    view->forceRefresh = FALSE;
    view->fogRefreshPending = FALSE;
    view->lastMovingIndex = (controller->moving &&
        (controller->movingIndex >= 0) &&
        (controller->movingIndex < view->capacity))
        ? controller->movingIndex : -1;

    /* También cubre el final coincidente con un scroll o una invalidación. */
    if (movementEnded && (endedIndex >= 0) && (endedIndex < count) &&
        (endedIndex < view->capacity))
    {
        const u8 movingIndex = (u8)endedIndex;
        const Unit *unit = UnitController_getUnit(controller, movingIndex);
        const bool visible = computeFogVisibility(view, unit);
        if (unitStateChanged(view, unit, movingIndex))
            cacheUnitState(view, unit, movingIndex);
        view->lastFogVisible[movingIndex] = visible;
        if (snapUnitSpriteToCell(view, controller, movingIndex, unit, camera,
                                 visible))
            spriteChanged = TRUE;
    }
    spriteChanged |= refillUnitSpriteBudget(view, controller, camera);
    return spriteChanged;
}

void UnitView_invalidate(UnitView *view)
{
    if (view != NULL) view->forceRefresh = TRUE;
}

/* Integración de niebla: refrescar solo unidades en celdas afectadas. */

void UnitView_setFogContext(UnitView *view, const MapData *map,
                            TeamId viewerTeam, bool enabled,
                            bool sharedVision)
{
    const bool nextEnabled = enabled && (map != NULL);
    const bool nextSharedVision = nextEnabled && sharedVision;
    bool contextChanged;
    bool mapChanged;

    if (view == NULL) return;
    mapChanged = (view->visibilityMap != map);
    contextChanged = mapChanged ||
        (view->viewerTeam != viewerTeam) || (view->fogEnabled != nextEnabled) ||
        (view->sharedFogVision != nextSharedVision);

    view->visibilityMap = map;
    view->viewerTeam = viewerTeam;
    view->fogEnabled = nextEnabled;
    view->sharedFogVision = nextSharedVision;

    if (contextChanged)
    {
        clearPendingSpriteCandidates(view);
        if ((map != NULL) && (mapChanged || !view->spatialIndexValid ||
            (view->spatialMapWidth != map->width) ||
            (view->spatialMapHeight != map->height)))
            rebuildSpatialIndex(view);
        else if (map == NULL)
            view->spatialIndexValid = FALSE;
        view->forceRefresh = TRUE;
    }
    else view->fogRefreshPending = TRUE;
}

void UnitView_setFogDirtyCells(UnitView *view, const u16 *cellIndices,
                               u16 cellCount, u16 mapWidth)
{
    if (view == NULL) return;
    view->fogDirtyCellIndices = cellIndices;
    view->fogDirtyCellCount = cellCount;
    view->fogDirtyMapWidth = mapWidth;
    view->fogDirtyCellsPending = (cellIndices != NULL) &&
        (cellCount > 0) && (mapWidth > 0) && (view->visibilityMap != NULL) &&
        (((u32)mapWidth * view->visibilityMap->height) <=
            UNIT_VIEW_MAX_MAP_CELLS) && !view->forceRefresh;
    if (view->fogDirtyCellsPending)
        view->fogRefreshPending = FALSE;
}

