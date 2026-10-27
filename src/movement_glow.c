/*
 * Presentación del área alcanzable. Consume los resultados de pathfinding y solo cambia
 * la paleta de las celdas resaltadas; no calcula rutas al mover el cursor.
 */
#include <genesis.h>
#include <string.h>
#include "movement_glow.h"
#include "combat.h"
#include "game_config.h"
#include "fog_of_war.h"

/*
 * Adaptador visual entre estado lógico y tilemap de SGDK: alcance móvil, objetivos de ataque
 * y reparación, niebla y paleta animada. Agrupa escrituras por tramos y mantiene una caché
 * por celda; las reglas de acción se deciden fuera de este módulo.
 */

#define PALETTE_SIZE             16
#define GLOW_FRAME_INTERVAL      4
#define GLOW_PHASE_COUNT         6
#define GLOW_STYLE_NORMAL        0u
#define GLOW_STYLE_REACHABLE     1u
#define GLOW_STYLE_UNSEEN        2u
#define GLOW_STYLE_ATTACKABLE    3u
#define GLOW_STYLE_REPAIRABLE    4u

static const u8 glowLevels[GLOW_PHASE_COUNT] = { 0, 1, 2, 3, 2, 1 };
static Map *glowMap;
static const u8 *reachableMask;
static u16 reachableWidth;
static u16 reachableHeight;
static const u8 *attackableMask;
static u16 attackableWidth;
static u16 attackableHeight;
static u16 terrainColors[PALETTE_SIZE];
static u16 glowColors[PALETTE_SIZE];
static u8 glowPhase;
static u8 glowFrameCount;
static bool hasReachableCells;
static const MapData *logicalMap;
static TeamId fogViewerTeam = TEAM_BLUE;
static u16 fogTileIndex;
static bool fogEnabled;
static bool fogSharedVision;
/* Precalcula el estilo de cada casilla lógica; el scroll consulta esta tabla. */
static u8 cellStyles[FOG_MAX_MAP_CELLS];
static bool cellStyleCacheValid;

/* Caché de estilos de celda y cálculo de paleta. */

static u16 brightenColor(u16 color, u8 amount)
{
    u8 red = (u8)((color >> 1) & 7);
    u8 green = (u8)((color >> 5) & 7);
    u8 blue = (u8)((color >> 9) & 7);

    red = (red + amount > 7) ? 7 : (u8)(red + amount);
    green = (green + amount > 7) ? 7 : (u8)(green + amount);
    blue = (blue + amount > 7) ? 7 : (u8)(blue + amount);

    return (u16)(((u16)red << 1) | ((u16)green << 5) |
                 ((u16)blue << 9));
}

static bool isCellReachable(u16 tileX, u16 tileY)
{
    const u16 cellX = tileX / CELL_TILES;
    const u16 cellY = tileY / CELL_TILES;
    u32 index;

    if ((reachableMask == NULL) || (cellX >= reachableWidth) ||
        (cellY >= reachableHeight))
        return FALSE;

    index = (u32)cellY * reachableWidth + cellX;
    return reachableMask[index] != 0;
}

static u8 getDisplayVisionState(u16 cellX, u16 cellY)
{
    if (logicalMap == NULL) return MAP_VISION_UNSEEN;
    if (fogSharedVision)
        return MapData_getCombinedVisionState(logicalMap, cellX, cellY);
    return MapData_getVisionState(logicalMap, (u8)fogViewerTeam,
                                  cellX, cellY);
}

static bool isCellAttackable(u16 cellX, u16 cellY)
{
    u32 index;

    if ((attackableMask == NULL) || (cellX >= attackableWidth) ||
        (cellY >= attackableHeight))
        return FALSE;

    index = (u32)cellY * attackableWidth + cellX;
    return attackableMask[index] == COMBAT_TARGET_ATTACKABLE;
}

static bool isCellRepairable(u16 cellX, u16 cellY)
{
    u32 index;

    if ((attackableMask == NULL) || (cellX >= attackableWidth) ||
        (cellY >= attackableHeight))
        return FALSE;

    index = (u32)cellY * attackableWidth + cellX;
    return attackableMask[index] == COMBAT_TARGET_REPAIRABLE;
}

static bool isCellUnseen(u16 tileX, u16 tileY)
{
    const u16 cellX = tileX / CELL_TILES;
    const u16 cellY = tileY / CELL_TILES;

    if (!fogEnabled || (logicalMap == NULL)) return FALSE;
    return getDisplayVisionState(cellX, cellY) == MAP_VISION_UNSEEN;
}

static u8 computeCellStyle(u16 cellX, u16 cellY)
{
    u32 index;

    if ((logicalMap == NULL) || (cellX >= logicalMap->width) ||
        (cellY >= logicalMap->height))
        return GLOW_STYLE_NORMAL;

    if (fogEnabled &&
        (getDisplayVisionState(cellX, cellY) == MAP_VISION_UNSEEN))
        return GLOW_STYLE_UNSEEN;

    /* Los blancos legalmente atacables tienen prioridad sobre el glow de
       movimiento; la niebla desconocida siempre conserva la prioridad máxima. */
    if (isCellAttackable(cellX, cellY)) return GLOW_STYLE_ATTACKABLE;
    if (isCellRepairable(cellX, cellY)) return GLOW_STYLE_REPAIRABLE;

    if ((reachableMask == NULL) || (cellX >= reachableWidth) ||
        (cellY >= reachableHeight))
        return GLOW_STYLE_NORMAL;

    index = (u32)cellY * reachableWidth + cellX;
    return reachableMask[index] ? GLOW_STYLE_REACHABLE : GLOW_STYLE_NORMAL;
}

static void refreshCellStyle(u16 cellX, u16 cellY)
{
    u32 index;

    if ((logicalMap == NULL) || !cellStyleCacheValid ||
        (cellX >= logicalMap->width) || (cellY >= logicalMap->height))
        return;

    index = (u32)cellY * logicalMap->width + cellX;
    if (index < FOG_MAX_MAP_CELLS)
        cellStyles[index] = computeCellStyle(cellX, cellY);
}

static void rebuildCellStyleCache(void)
{
    u32 cellCount;
    u32 index;
    u16 cellY;

    cellStyleCacheValid = FALSE;
    if ((logicalMap == NULL) || (logicalMap->cells == NULL) ||
        (logicalMap->width == 0) || (logicalMap->height == 0))
        return;

    cellCount = (u32)logicalMap->width * logicalMap->height;
    if (cellCount > FOG_MAX_MAP_CELLS) return;

    index = 0;
    for (cellY = 0; cellY < logicalMap->height; cellY++)
    {
        u16 cellX;
        for (cellX = 0; cellX < logicalMap->width; cellX++)
            cellStyles[index++] = computeCellStyle(cellX, cellY);
    }
    cellStyleCacheValid = TRUE;
}

static u8 getCellStyle(u16 cellX, u16 cellY)
{
    u32 index;

    if (logicalMap != NULL && (cellX >= logicalMap->width ||
                               cellY >= logicalMap->height))
        return GLOW_STYLE_NORMAL;

    if (cellStyleCacheValid && (logicalMap != NULL))
    {
        index = (u32)cellY * logicalMap->width + cellX;
        return (index < FOG_MAX_MAP_CELLS)
            ? cellStyles[index] : GLOW_STYLE_NORMAL;
    }

    if ((logicalMap != NULL) && isCellUnseen(
            (u16)(cellX * CELL_TILES), (u16)(cellY * CELL_TILES)))
        return GLOW_STYLE_UNSEEN;
    if (isCellAttackable(cellX, cellY)) return GLOW_STYLE_ATTACKABLE;
    if (isCellRepairable(cellX, cellY)) return GLOW_STYLE_REPAIRABLE;
    if (isCellReachable((u16)(cellX * CELL_TILES),
                        (u16)(cellY * CELL_TILES)))
        return GLOW_STYLE_REACHABLE;
    return GLOW_STYLE_NORMAL;
}

/* Parcheo del tilemap: mantener el tile y ajustar solo su paleta. */

static u16 applyTileStyle(u16 attribute, u8 style)
{
    if (style == GLOW_STYLE_UNSEEN)
    {
        /* Sustituye el patrón completo; el atlas original no se modifica. */
        return (u16)((attribute &
            (u16)~(0x07FFu | TILE_ATTR_PALETTE_MASK)) |
            (fogTileIndex & 0x07FFu) |
            ((u16)PAL2 << TILE_ATTR_PALETTE_SFT));
    }

    {
        const u16 palette = ((style == GLOW_STYLE_REACHABLE) ||
                             (style == GLOW_STYLE_REPAIRABLE)) ? PAL1 :
            ((style == GLOW_STYLE_ATTACKABLE) ? PAL3 : PAL0);
        return (u16)((attribute & (u16)~TILE_ATTR_PALETTE_MASK) |
                     ((u16)palette << TILE_ATTR_PALETTE_SFT));
    }
}

static u16 patchTileAttribute(u16 attribute, u16 tileX, u16 tileY)
{
    return applyTileStyle(attribute, getCellStyle(
        (u16)(tileX / CELL_TILES), (u16)(tileY / CELL_TILES)));
}

static void patchMapTileData(Map *map, u16 *buffer, u16 x, u16 y,
                             MapUpdateType updateType, u16 size)
{
    u16 offset = 0;

    (void)map;
    if (buffer == NULL) return;

    /*
     * Cada casilla lógica ocupa tres tiles contiguos por eje. Consulta el
     * estilo una vez por tramo de tres tiles, no una vez por tile; este callback
     * es una ruta caliente de MAP_scrollTo durante el desplazamiento de cámara.
     */
    if (updateType == ROW_UPDATE)
    {
        const u16 cellY = (u16)(y / CELL_TILES);
        while (offset < size)
        {
            const u16 tileX = (u16)(x + offset);
            const u16 cellX = (u16)(tileX / CELL_TILES);
            u16 run = (u16)(CELL_TILES - (tileX % CELL_TILES));
            const u8 style = getCellStyle(cellX, cellY);
            u16 i;

            if (run > (u16)(size - offset)) run = (u16)(size - offset);
            for (i = 0; i < run; i++)
                buffer[offset + i] = applyTileStyle(buffer[offset + i], style);
            offset = (u16)(offset + run);
        }
    }
    else
    {
        const u16 cellX = (u16)(x / CELL_TILES);
        while (offset < size)
        {
            const u16 tileY = (u16)(y + offset);
            const u16 cellY = (u16)(tileY / CELL_TILES);
            u16 run = (u16)(CELL_TILES - (tileY % CELL_TILES));
            const u8 style = getCellStyle(cellX, cellY);
            u16 i;

            if (run > (u16)(size - offset)) run = (u16)(size - offset);
            for (i = 0; i < run; i++)
                buffer[offset + i] = applyTileStyle(buffer[offset + i], style);
            offset = (u16)(offset + run);
        }
    }
}

/* API pública para máscaras de selección y niebla. */

void MovementGlow_release(void)
{
    /* No posee el mapa: solo abandona sus referencias/cachés prestadas. */
    glowMap = NULL;
    reachableMask = NULL;
    reachableWidth = 0;
    reachableHeight = 0;
    attackableMask = NULL;
    attackableWidth = 0;
    attackableHeight = 0;
    hasReachableCells = FALSE;
    logicalMap = NULL;
    fogViewerTeam = TEAM_BLUE;
    fogTileIndex = 0;
    fogEnabled = FALSE;
    fogSharedVision = FALSE;
    cellStyleCacheValid = FALSE;
    glowPhase = 0;
    glowFrameCount = 0;
    memset(cellStyles, 0, sizeof(cellStyles));
    memset(terrainColors, 0, sizeof(terrainColors));
    memset(glowColors, 0, sizeof(glowColors));
}

bool MovementGlow_init(Map *map, const u16 *terrainPalette)
{
    u8 i;

    if ((map == NULL) || (terrainPalette == NULL)) return FALSE;

    glowMap = map;
    reachableMask = NULL;
    reachableWidth = 0;
    reachableHeight = 0;
    attackableMask = NULL;
    attackableWidth = 0;
    attackableHeight = 0;
    hasReachableCells = FALSE;
    logicalMap = NULL;
    fogViewerTeam = TEAM_BLUE;
    fogTileIndex = 0;
    fogEnabled = FALSE;
    fogSharedVision = FALSE;
    cellStyleCacheValid = FALSE;
    glowPhase = 0;
    glowFrameCount = 0;

    for (i = 0; i < PALETTE_SIZE; i++)
    {
        terrainColors[i] = terrainPalette[i];
        glowColors[i] = terrainPalette[i];
    }

    /* PAL1 empieza como copia exacta de PAL0; luego solo cambia su intensidad. */
    /* Las escrituras CRAM se encolan para VBlank; DMA inmediato produce CRAM dots. */
    PAL_setPalette(PAL1, glowColors, DMA_QUEUE_COPY);
    MAP_setDataPatchCallback(glowMap, patchMapTileData);
    return TRUE;
}

void MovementGlow_setReachableMask(const u8 *mask, u16 mapWidth,
                                   u16 mapHeight)
{
    u32 cellCount;
    u32 i;

    reachableMask = mask;
    reachableWidth = (mask != NULL) ? mapWidth : 0;
    reachableHeight = (mask != NULL) ? mapHeight : 0;
    hasReachableCells = FALSE;

    if ((mask == NULL) || (mapWidth == 0) || (mapHeight == 0))
    {
        rebuildCellStyleCache();
        return;
    }

    cellCount = (u32)mapWidth * mapHeight;
    for (i = 0; i < cellCount; i++)
    {
        if (mask[i] != 0)
        {
            hasReachableCells = TRUE;
            break;
        }
    }

    rebuildCellStyleCache();
}

void MovementGlow_setAttackableMask(const u8 *mask, u16 mapWidth,
                                    u16 mapHeight)
{
    const u32 cellCount = (u32)mapWidth * mapHeight;

    attackableMask = mask;
    attackableWidth = (mask != NULL) ? mapWidth : 0;
    attackableHeight = (mask != NULL) ? mapHeight : 0;

    /* Las máscaras solo cambian al seleccionar/actuar: una reconstrucción
       compacta de la caché aquí no añade trabajo al camino caliente del scroll. */
    if ((mask == NULL) || (mapWidth == 0) || (mapHeight == 0) ||
        (logicalMap == NULL) || (cellCount > FOG_MAX_MAP_CELLS))
    {
        rebuildCellStyleCache();
        return;
    }

    rebuildCellStyleCache();
}

bool MovementGlow_setMasks(const u8 *reachable, const u8 *actionTargets,
                           u16 mapWidth, u16 mapHeight,
                           u32 cameraX, u32 cameraY)
{
    const u32 cellCount = (u32)mapWidth * mapHeight;
    u16 y;

    if ((logicalMap == NULL) || (logicalMap->cells == NULL) ||
        (mapWidth == 0) || (mapHeight == 0) ||
        (mapWidth != logicalMap->width) ||
        (mapHeight != logicalMap->height) ||
        (cellCount > FOG_MAX_MAP_CELLS))
        return FALSE;

    reachableMask = reachable;
    reachableWidth = (reachable != NULL) ? mapWidth : 0;
    reachableHeight = (reachable != NULL) ? mapHeight : 0;
    attackableMask = actionTargets;
    attackableWidth = (actionTargets != NULL) ? mapWidth : 0;
    attackableHeight = (actionTargets != NULL) ? mapHeight : 0;
    hasReachableCells = FALSE;

    /* Si la caché aún no existe (inicialización/cambio de escenario), usar
       la ruta completa una sola vez. En una selección normal ya está válida. */
    if (!cellStyleCacheValid)
    {
        rebuildCellStyleCache();
        if (!cellStyleCacheValid) return FALSE;
        if (reachable != NULL)
        {
            u32 i;
            for (i = 0; i < cellCount; i++)
                if (reachable[i] != 0)
                {
                    hasReachableCells = TRUE;
                    break;
                }
        }
        MovementGlow_redraw(cameraX, cameraY);
        return TRUE;
    }

    /* El mapa lógico tiene 1.600 celdas, pero una selección solo afecta a las
       casillas del rango viejo/nuevo y a los objetivos atacables. Se conserva
       el estilo del resto, sin recalcular visión/niebla para todo el mapa.
       Las escrituras gráficas se agrupan por tramos horizontales modificados. */
    for (y = 0; y < mapHeight; y++)
    {
        u16 x;
        u16 runStart = 0xFFFFu;

        for (x = 0; x < mapWidth; x++)
        {
            const u32 index = (u32)y * mapWidth + x;
            const u8 oldStyle = cellStyles[index];
            const bool wasHighlighted =
                (oldStyle == GLOW_STYLE_REACHABLE) ||
                (oldStyle == GLOW_STYLE_ATTACKABLE) ||
                (oldStyle == GLOW_STYLE_REPAIRABLE);
            const bool isReachable = (reachable != NULL) &&
                (reachable[index] != 0);
            const bool isAttackable = (actionTargets != NULL) &&
                (actionTargets[index] == COMBAT_TARGET_ATTACKABLE);
            const bool isRepairable = (actionTargets != NULL) &&
                (actionTargets[index] == COMBAT_TARGET_REPAIRABLE);
            bool changed = FALSE;

            if (isReachable) hasReachableCells = TRUE;
            if (wasHighlighted || isReachable || isAttackable || isRepairable)
            {
                const u8 newStyle = computeCellStyle(x, y);
                cellStyles[index] = newStyle;
                changed = (newStyle != oldStyle);
            }

            if (changed)
            {
                if (runStart == 0xFFFFu) runStart = x;
            }
            else if (runStart != 0xFFFFu)
            {
                (void)MovementGlow_refreshFogCellRun(
                    runStart, (u16)(x - 1u), y, cameraX, cameraY);
                runStart = 0xFFFFu;
            }
        }

        if (runStart != 0xFFFFu)
            (void)MovementGlow_refreshFogCellRun(
                runStart, (u16)(mapWidth - 1u), y, cameraX, cameraY);
    }

    return TRUE;
}

/* Restauración incremental de los overlays de selección. */

bool MovementGlow_clearAttackableMask(u32 cameraX, u32 cameraY)
{
    u16 y;

    attackableMask = NULL;
    attackableWidth = 0;
    attackableHeight = 0;

    if ((logicalMap == NULL) || (logicalMap->cells == NULL) ||
        !cellStyleCacheValid || (logicalMap->width == 0) ||
        (logicalMap->height == 0) ||
        (((u32)logicalMap->width * logicalMap->height) > FOG_MAX_MAP_CELLS))
        return FALSE;

    for (y = 0; y < logicalMap->height; y++)
    {
        u16 x = 0;
        while (x < logicalMap->width)
        {
            const u32 index = (u32)y * logicalMap->width + x;
            u16 firstX;

            if ((cellStyles[index] != GLOW_STYLE_ATTACKABLE) &&
                (cellStyles[index] != GLOW_STYLE_REPAIRABLE))
            {
                x++;
                continue;
            }

            firstX = x;
            do
            {
                cellStyles[(u32)y * logicalMap->width + x] =
                    computeCellStyle(x, y);
                x++;
            }
            while ((x < logicalMap->width) &&
                   ((cellStyles[(u32)y * logicalMap->width + x] ==
                     GLOW_STYLE_ATTACKABLE) ||
                    (cellStyles[(u32)y * logicalMap->width + x] ==
                     GLOW_STYLE_REPAIRABLE)));

            (void)MovementGlow_refreshFogCellRun(firstX, (u16)(x - 1), y,
                                                  cameraX, cameraY);
        }
    }

    return TRUE;
}

bool MovementGlow_clearReachableMask(u32 cameraX, u32 cameraY)
{
    u16 y;

    /*
     * setReachableMask(NULL) antes reconstruía los estilos de las 1600 celdas
     * y el llamador forzaba un scroll/redibujado completo. Al empezar cada
     * movimiento eso provocaba un pico visible de trabajo. La caché conserva
     * qué celdas se estaban iluminando, así que actualizamos solo esas bandas.
     */
    reachableMask = NULL;
    reachableWidth = 0;
    reachableHeight = 0;
    hasReachableCells = FALSE;

    if ((logicalMap == NULL) || (logicalMap->cells == NULL) ||
        !cellStyleCacheValid || (logicalMap->width == 0) ||
        (logicalMap->height == 0) ||
        (((u32)logicalMap->width * logicalMap->height) > FOG_MAX_MAP_CELLS))
        return FALSE;

    for (y = 0; y < logicalMap->height; y++)
    {
        u16 x = 0;
        while (x < logicalMap->width)
        {
            const u32 index = (u32)y * logicalMap->width + x;
            u16 firstX;

            if (cellStyles[index] != GLOW_STYLE_REACHABLE)
            {
                x++;
                continue;
            }

            firstX = x;
            do
            {
                /* reachableMask ya es NULL: solo se conserva niebla/terreno. */
                cellStyles[(u32)y * logicalMap->width + x] =
                    computeCellStyle(x, y);
                x++;
            }
            while ((x < logicalMap->width) &&
                   (cellStyles[(u32)y * logicalMap->width + x] ==
                    GLOW_STYLE_REACHABLE));

            /* Redibuja solo la fila de celdas que ha perdido el resplandor. */
            (void)MovementGlow_refreshFogCellRun(firstX, (u16)(x - 1), y,
                                                  cameraX, cameraY);
        }
    }

    return TRUE;
}

/* Refresco de niebla y animación de paleta por frames. */

void MovementGlow_setFogContext(const MapData *map, TeamId viewerTeam,
                                u16 tileIndex, bool enabled,
                                bool sharedVision)
{
    const bool nextEnabled = enabled && (map != NULL);
    const bool nextSharedVision = nextEnabled && sharedVision;
    const bool contextChanged = (logicalMap != map) ||
        (fogViewerTeam != viewerTeam) || (fogTileIndex != tileIndex) ||
        (fogEnabled != nextEnabled) || (fogSharedVision != nextSharedVision);

    logicalMap = map;
    fogViewerTeam = viewerTeam;
    fogTileIndex = tileIndex;
    fogEnabled = nextEnabled;
    fogSharedVision = nextSharedVision;

    /* La actualización por movimiento refresca las celdas concretas. */
    if (contextChanged) rebuildCellStyleCache();
}

void MovementGlow_redraw(u32 cameraX, u32 cameraY)
{
    if (glowMap != NULL)
        MAP_scrollToEx(glowMap, cameraX, cameraY, TRUE);
}

/*
 * Refresca una fila horizontal de celdas lógicas. Cada celda ocupa tres tiles,
 * pero una fila contigua se escribe con una sola operación DMA por fila
 * visual (o dos si atraviesa el final circular del plano).
 */
bool MovementGlow_refreshFogCellRun(u16 firstCellX, u16 lastCellX, u16 cellY,
                                    u32 cameraX, u32 cameraY)
{
    enum { MAX_VISIBLE_RUN_TILES =
        (SCREEN_PIXELS_W / CELL_PIXELS + 2) * CELL_TILES };
    u16 rowData[MAX_VISIBLE_RUN_TILES];
    u16 cellX;
    u16 visibleFirstX;
    u16 visibleLastX;
    u16 firstTileX;
    u16 tileCount;
    u16 row;
    u32 cellWorldY;

    if ((glowMap == NULL) || (logicalMap == NULL) ||
        (firstCellX > lastCellX) || (cellY >= logicalMap->height) ||
        (lastCellX >= logicalMap->width))
        return FALSE;

    /* Actualiza la caché para todas las celdas, también las fuera de pantalla. */
    for (cellX = firstCellX; cellX <= lastCellX; cellX++)
        refreshCellStyle(cellX, cellY);

    cellWorldY = (u32)cellY * CELL_PIXELS;
    if ((cellWorldY + CELL_PIXELS <= cameraY) ||
        (cellWorldY >= cameraY + SCREEN_PIXELS_H) ||
        (glowMap->planeWidth == 0) || (glowMap->planeHeight == 0))
        return FALSE;

    /* Solo se transfieren celdas que cortan físicamente el viewport actual. */
    visibleFirstX = (u16)(cameraX / CELL_PIXELS);
    visibleLastX = (u16)((cameraX + SCREEN_PIXELS_W - 1) / CELL_PIXELS);
    if (firstCellX < visibleFirstX) firstCellX = visibleFirstX;
    if (lastCellX > visibleLastX) lastCellX = visibleLastX;
    if (firstCellX > lastCellX) return FALSE;

    firstTileX = (u16)(firstCellX * CELL_TILES);
    tileCount = (u16)((lastCellX - firstCellX + 1) * CELL_TILES);

    for (row = 0; row < CELL_TILES; row++)
    {
        const u16 tileY = (u16)(cellY * CELL_TILES + row);
        const u16 planeY = (u16)(tileY & (glowMap->planeHeight - 1));
        u16 offset = 0;

        while (offset < tileCount)
        {
            const u16 worldTileX = (u16)(firstTileX + offset);
            const u16 planeX = (u16)(worldTileX & (glowMap->planeWidth - 1));
            u16 chunk = (u16)(glowMap->planeWidth - planeX);
            u16 i;

            if (chunk > (u16)(tileCount - offset))
                chunk = (u16)(tileCount - offset);
            if (chunk > MAX_VISIBLE_RUN_TILES)
                chunk = MAX_VISIBLE_RUN_TILES;

            for (i = 0; i < chunk; i++)
            {
                const u16 logicalTileX = (u16)(worldTileX + i);
                const u16 attribute = (u16)(MAP_getTile(
                    glowMap, logicalTileX, tileY) + glowMap->baseTile);
                rowData[i] = patchTileAttribute(attribute, logicalTileX, tileY);
            }

            /* COPY permite usar rowData local sin dejar referencias a la pila. */
            VDP_setTileMapDataRow(glowMap->plane, rowData, planeY, planeX,
                                  chunk, DMA_QUEUE_COPY);
            offset = (u16)(offset + chunk);
        }
    }

    return TRUE;
}

bool MovementGlow_refreshFogCell(u16 cellX, u16 cellY,
                                 u32 cameraX, u32 cameraY)
{
    return MovementGlow_refreshFogCellRun(cellX, cellX, cellY,
                                          cameraX, cameraY);
}

void MovementGlow_update(void)
{
    u8 i;
    u8 amount;

    /* No se escribe CRAM si no hay ninguna casilla iluminada. */
    if ((glowMap == NULL) || !hasReachableCells) return;
    if (++glowFrameCount < GLOW_FRAME_INTERVAL) return;
    glowFrameCount = 0;
    glowPhase = (u8)((glowPhase + 1) % GLOW_PHASE_COUNT);
    amount = glowLevels[glowPhase];

    /* El indice 0 se deja intacto; los indices de color se iluminan por fases. */
    glowColors[0] = terrainColors[0];
    for (i = 1; i < PALETTE_SIZE; i++)
        glowColors[i] = brightenColor(terrainColors[i], amount);

    PAL_setPalette(PAL1, glowColors, DMA_QUEUE_COPY);
}
