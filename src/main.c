/*
 * Bucle principal y composición de subsistemas. Mantener aquí solo la coordinación
 * de estados; las reglas de mapa, movimiento, combate e IA viven en sus módulos.
 * El orden de liberación de recursos se comprueba en session_lifecycle_test.sh.
 */
#include <genesis.h>
#include <string.h>
#include <sprite_eng.h>
#include "resources.h"
#include "camera_cursor.h"
#include "cell_events.h"
#include "cell_events_selftest.h"
#include "cell_hud.h"
#include "game_config.h"
#include "map_data.h"
#include "unit_controller.h"
#include "unit_view.h"
#include "unit_types.h"
#include "team.h"
#include "game_turn.h"
#include "movement_glow.h"
#include "combat.h"
#include "fog_of_war.h"
#include "scenario.h"
#include "scenario_menu.h"
#include "game_screens.h"
#include "enemy_ai.h"
#include "performance.h"
#include "mission_result.h"

/*
 * Composición de la aplicación: inicialización, fases de turno y presentación.
 * Las reglas puras de mapa, rutas, combate, niebla e IA pertenecen a sus módulos.
 * updateGameFrame() coordina esos sistemas; no debe contener búsquedas pesadas por frame.
 */

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define FPS_OVERLAY_REFRESH_FRAMES 8u
#if ACTIVE_MAP_INDEX >= SCENARIO_COUNT
#error "ACTIVE_MAP_INDEX debe apuntar a un escenario registrado"
#endif

#if SCENARIO_MAX_UNIT_COUNT > GAME_MAX_UNITS
#error "El mayor despliegue supera la capacidad logica del motor"
#endif

#if MAX_UNIT_SPRITES + FIXED_GAME_SPRITE_COUNT > MD_VDP_SPRITE_LIMIT
#error "La reserva de sprites excede el limite del VDP"
#endif

/* Recursos y estado autoritativo de la partida activa. */
static Sprite *unitSprites[SCENARIO_MAX_UNIT_COUNT];
static const ScenarioDefinition *activeScenario;
static UnitController unitController;
static GameTurn gameTurn;
static EnemyAI enemyAI;
static Sprite *objectiveMarker;
static Sprite *fogTileReservation;
static u16 fogTileIndex;
static FogOfWar fogOfWar;
/* Caché de objetivos: solo se reconstruye al cambiar selección/acción. */
static u8 actionTargetMask[FOG_MAX_MAP_CELLS];
/* Banderas de sincronización: las tareas costosas se difieren hasta un frame seguro. */
static bool fogUpdatePending;
/* La visión se consolida una sola vez al terminar la animación de una ruta. */
static bool fogRefreshPendingAfterMovement;
static bool movementCellHudRefreshPending;
static TeamId fogRenderedTeam = TEAM_BLUE;
static bool fogRenderedSpectatorMode;
static bool aiSpectatorView;
/* Aplaza el repintado del HUD hasta acabar las rutas de la IA. */
static bool aiSpectatorHudRefreshPending;
static bool cameraReturnPending;
static bool cursorHiddenByAI;
static bool cursorVisibilityDirty;
static bool aiReturnCameraValid;
static u8 aiPresentationHoldFrames;
static s16 aiReturnCameraX;
static s16 aiReturnCameraY;

/* Estado de presentación: sprites, scroll y posiciones cacheadas. */
static Map *worldMap;
static Sprite *cellCursor;
static s16 lastCursorSpriteX = UNIT_VIEW_POSITION_INVALID;
static s16 lastCursorSpriteY = UNIT_VIEW_POSITION_INVALID;
static s16 lastObjectiveMarkerX = UNIT_VIEW_POSITION_HIDDEN;
static s16 lastObjectiveMarkerY = UNIT_VIEW_POSITION_HIDDEN;
static UnitView unitView;
static s16 lastMapScrollX;
static s16 lastMapScrollY;
static bool mapScrollInitialized;
static u8 fpsOverlayFrameCounter;

/* Permite omitir el recálculo visual si selección y unidad no han cambiado. */
typedef struct
{
    bool initialized;
    bool enabled;
    s16 selectedIndex;
    u16 unitX;
    u16 unitY;
    u8 movementPoints;
    bool hasUsedAction;
} MovementRangeCache;

static MovementRangeCache movementRangeCache;

/* Evita escribir las mismas filas de información del turno cada frame. */
typedef struct
{
    bool initialized;
    TeamId activeTeam;
    u16 turnNumber;
    bool hasSelectedUnit;
    u8 movementPoints;
    u8 maxMovementPoints;
    u16 objectiveX;
    u16 objectiveY;
    const char *outcomeText;
    u16 missionProgress;
    MissionType missionType;
    TeamId missionHolder;
} TurnHudCache;

static TurnHudCache turnHudCache;
/* Fuerza una nueva lectura contextual cuando expira un feedback visible. */
static bool hudContextRefreshPending;

static void onCellClick(const CellEventContext *context)
{
    KLog_U2("Click celda X:", context->x, "Y:", context->y);
}

/* Perfil 0: sin callbacks. Perfil 1: click de ejemplo, compartido por el mapa. */
static const CellEventSet cellEventSets[] =
{
    { { 0 } },
    { { [CELL_EVENT_CLICK] = onCellClick } }
};

/* -------------------------------------------------------------------------
 * Inicio y ciclo de sesión
 * ------------------------------------------------------------------------- */

static void fatalError(const char *message)
{
    KLog(message);
    while (TRUE) SYS_doVBlankProcess();
}

/* -------------------------------------------------------------------------
 * Datos de partida e inicialización de vídeo
 * ------------------------------------------------------------------------- */

static bool initGameData(void)
{
    /* Invalida caches de la partida anterior antes de reutilizar los buffers. */
    movementRangeCache.initialized = FALSE;
    turnHudCache.initialized = FALSE;
    memset(actionTargetMask, 0, sizeof(actionTargetMask));
    fpsOverlayFrameCounter = 0;
    hudContextRefreshPending = FALSE;
    fogUpdatePending = FALSE;
    fogRefreshPendingAfterMovement = FALSE;
    movementCellHudRefreshPending = FALSE;
    aiSpectatorView = FALSE;
    aiSpectatorHudRefreshPending = FALSE;
    cameraReturnPending = FALSE;
    aiReturnCameraValid = FALSE;
    cursorHiddenByAI = FALSE;
    cursorVisibilityDirty = FALSE;
    aiPresentationHoldFrames = 0;

    /* activeScenario lo establece el selector antes de inicializar la partida. */
    if (activeScenario == NULL) return FALSE;

    CellEvents_init(cellEventSets,
                    (u8)(sizeof(cellEventSets) / sizeof(cellEventSets[0])));
    if (!MapData_init(activeScenario->logic)) return FALSE;

    CameraCursor_init(activeScenario->logic->width,
                      activeScenario->logic->height);
    if (!UnitController_init(&unitController, activeScenario->logic,
                             activeScenario->unitStorage,
                             activeScenario->unitCapacity,
                             activeScenario->unitStarts,
                             activeScenario->unitCount))
        return FALSE;

    if (!GameTurn_init(&gameTurn, activeScenario->logic, &unitController,
                       TEAM_BLUE, &activeScenario->objective))
        return FALSE;

    if (!FogOfWar_init(&fogOfWar, activeScenario->logic, &unitController,
                       GameTurn_getActiveTeam(&gameTurn)))
        return FALSE;

    UnitController_setFogEnabled(&unitController, TRUE);
    EnemyAI_init(&enemyAI);
    return TRUE;
}

static void initCellCursor(void)
{
    const CameraCursorState state = CameraCursor_getState();
    u16 availableTiles;
    u16 attributes;

    PAL_setPalette(PAL2, cell_cursor.palette->data, DMA_QUEUE_COPY);

    if ((CURSOR_VRAM_TILE_INDEX >= VRAM_TILE_COUNT) ||
        (cell_cursor.maxNumTile == 0))
    {
        fatalError("ERROR: metadatos de cell_cursor no validos");
    }

    availableTiles = VRAM_TILE_COUNT - CURSOR_VRAM_TILE_INDEX;
    if (cell_cursor.maxNumTile > availableTiles)
    {
        fatalError("ERROR: el cursor supera el final de la VRAM");
    }

    attributes = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE,
                                CURSOR_VRAM_TILE_INDEX);
    cellCursor = SPR_addSpriteEx(&cell_cursor, state.cursorX, state.cursorY,
                                 attributes,
                                 SPR_FLAG_AUTO_TILE_UPLOAD | SPR_FLAG_INSERT_HEAD);
    if (cellCursor == NULL)
    {
        KLog_U2("VRAM libre:", SPR_getFreeVRAM(),
                "Mayor bloque libre:", SPR_getLargestFreeVRAMBlock());
        KLog_U2("Sprites activos:", SPR_getNumActiveSprite(),
                "Sprites VDP usados:", SPR_getUsedVDPSprite());
        fatalError("ERROR: no se pudo crear cell_cursor");
    }

    SPR_setPosition(cellCursor, state.cursorX, state.cursorY);
    lastCursorSpriteX = state.cursorX;
    lastCursorSpriteY = state.cursorY;
    SPR_setPriority(cellCursor, TRUE);
    SPR_setDepth(cellCursor, SPR_MIN_DEPTH);
    SPR_setVisibility(cellCursor, VISIBLE);
}

static void initObjectiveMarker(void)
{
    const u16 attributes = TILE_ATTR_FULL(
        PAL2, TRUE, FALSE, FALSE, CURSOR_VRAM_TILE_INDEX);

    objectiveMarker = SPR_addSpriteEx(
        &cell_cursor, -CELL_PIXELS, -CELL_PIXELS, attributes,
        SPR_FLAG_AUTO_TILE_UPLOAD);
    if (objectiveMarker == NULL)
        fatalError("ERROR: no se pudo crear el marcador de objetivo");

    SPR_setDepth(objectiveMarker, SPR_MIN_DEPTH + 2);
    SPR_setPosition(objectiveMarker, -CELL_PIXELS, -CELL_PIXELS);
    SPR_setVisibility(objectiveMarker, HIDDEN);
    lastObjectiveMarkerX = UNIT_VIEW_POSITION_HIDDEN;
    lastObjectiveMarkerY = UNIT_VIEW_POSITION_HIDDEN;
}

/* -------------------------------------------------------------------------
 * Objetivo, HUD de turno y coordenadas
 * ------------------------------------------------------------------------- */

static bool updateObjectiveMarker(const CameraCursorState *state)
{
    const MissionObjective *objective = GameTurn_getObjective(&gameTurn);
    const s32 x = (s32)objective->x * CELL_PIXELS - state->cameraX;
    const s32 y = (s32)objective->y * CELL_PIXELS - state->cameraY;
    const bool hasPointMarker = (objective->type == MISSION_CAPTURE_POINT) ||
        (objective->type == MISSION_HOLD_POINT);
    const bool visible = hasPointMarker && (x < VIEW_PIXELS_W) &&
        (x + CELL_PIXELS > 0) &&
        (y < VIEW_PIXELS_H) && (y + CELL_PIXELS > 0) &&
        ((x + CELL_PIXELS) <= VIEW_PIXELS_W) &&
        ((y + CELL_PIXELS) <= VIEW_PIXELS_H);
    bool changed = FALSE;

    if (visible)
    {
        const bool wasHidden = lastObjectiveMarkerX == UNIT_VIEW_POSITION_HIDDEN;
        if ((lastObjectiveMarkerX != (s16)x) ||
            (lastObjectiveMarkerY != (s16)y))
        {
            SPR_setPosition(objectiveMarker, (s16)x, (s16)y);
            lastObjectiveMarkerX = (s16)x;
            lastObjectiveMarkerY = (s16)y;
            changed = TRUE;
        }
        if (wasHidden)
        {
            SPR_setVisibility(objectiveMarker, VISIBLE);
            changed = TRUE;
        }
    }
    else if (lastObjectiveMarkerX != UNIT_VIEW_POSITION_HIDDEN)
    {
        SPR_setPosition(objectiveMarker, -CELL_PIXELS, -CELL_PIXELS);
        if (SPR_getVisibility(objectiveMarker) != HIDDEN)
            SPR_setVisibility(objectiveMarker, HIDDEN);
        lastObjectiveMarkerX = UNIT_VIEW_POSITION_HIDDEN;
        lastObjectiveMarkerY = UNIT_VIEW_POSITION_HIDDEN;
        changed = TRUE;
    }
    return changed;
}

static void updateTurnHud(void)
{
    const TeamId activeTeam = GameTurn_getActiveTeam(&gameTurn);
    const TeamInfo *team = Team_getInfo(activeTeam);
    const Unit *selected = UnitController_getSelected(&unitController);
    const MissionObjective *objective = GameTurn_getObjective(&gameTurn);
    const u16 turnNumber = GameTurn_getNumber(&gameTurn);
    const u16 missionProgress = GameTurn_getMissionProgress(&gameTurn);
    const TeamId missionHolder = GameTurn_getObjectiveHoldTeam(&gameTurn);
    const bool hasSelectedUnit = selected != NULL;
    const u8 movementPoints = hasSelectedUnit ? selected->movementPoints : 0;
    const u8 maxMovementPoints = hasSelectedUnit ? selected->maxMovementPoints : 0;
    const char *outcomeText = GameTurn_getOutcomeText(&gameTurn);

    CellHud_setMovementTerrainProfile(
        hasSelectedUnit ? selected->type : UNIT_TYPE_INFANTRY,
        hasSelectedUnit);

    /* Evita formatear y comparar seis líneas en cada fotograma sin cambios. */
    if (turnHudCache.initialized &&
        (turnHudCache.activeTeam == activeTeam) &&
        (turnHudCache.turnNumber == turnNumber) &&
        (turnHudCache.hasSelectedUnit == hasSelectedUnit) &&
        (turnHudCache.movementPoints == movementPoints) &&
        (turnHudCache.maxMovementPoints == maxMovementPoints) &&
        (turnHudCache.objectiveX == objective->x) &&
        (turnHudCache.objectiveY == objective->y) &&
        (turnHudCache.outcomeText == outcomeText) &&
        (turnHudCache.missionProgress == missionProgress) &&
        (turnHudCache.missionType == objective->type) &&
        (turnHudCache.missionHolder == missionHolder))
        return;

    CellHud_setTurnInfo((team != NULL) ? team->name : "?", turnNumber,
        hasSelectedUnit, movementPoints, maxMovementPoints,
        objective->x, objective->y, outcomeText, missionHolder,
        objective->type, missionProgress, objective->requiredTurns);

    turnHudCache.initialized = TRUE;
    turnHudCache.activeTeam = activeTeam;
    turnHudCache.turnNumber = turnNumber;
    turnHudCache.hasSelectedUnit = hasSelectedUnit;
    turnHudCache.movementPoints = movementPoints;
    turnHudCache.maxMovementPoints = maxMovementPoints;
    turnHudCache.objectiveX = objective->x;
    turnHudCache.objectiveY = objective->y;
    turnHudCache.outcomeText = outcomeText;
    turnHudCache.missionProgress = missionProgress;
    turnHudCache.missionType = objective->type;
    turnHudCache.missionHolder = missionHolder;
}

static void getCursorCell(const CameraCursorState *state, u16 *x, u16 *y)
{
    const MapData *map = activeScenario->logic;
    const u32 worldX = (u32)state->cameraX + state->cursorX;
    const u32 worldY = (u32)state->cameraY + state->cursorY;

    *x = (u16)(worldX / CELL_PIXELS);
    *y = (u16)(worldY / CELL_PIXELS);
    if (*x >= map->width) *x = map->width - 1;
    if (*y >= map->height) *y = map->height - 1;
}

/* Actualiza el plano del mapa solo si ha cambiado el scroll en píxeles. */
static void scrollMapIfNeeded(const CameraCursorState *state)
{
    if (!mapScrollInitialized ||
        (state->cameraX != lastMapScrollX) ||
        (state->cameraY != lastMapScrollY))
    {
#if PERF_DIAGNOSTICS
        const u16 deltaX = mapScrollInitialized
            ? (u16)((state->cameraX >= lastMapScrollX)
                ? (state->cameraX - lastMapScrollX)
                : (lastMapScrollX - state->cameraX)) : 0;
        const u16 deltaY = mapScrollInitialized
            ? (u16)((state->cameraY >= lastMapScrollY)
                ? (state->cameraY - lastMapScrollY)
                : (lastMapScrollY - state->cameraY)) : 0;
        Performance_count(PERF_EVENT_SCROLL, 1);
        Performance_count(PERF_EVENT_SCROLL_PIXELS,
                          (u32)deltaX + (u32)deltaY);
#endif
        MAP_scrollTo(worldMap, (u32)state->cameraX, (u32)state->cameraY);
        lastMapScrollX = state->cameraX;
        lastMapScrollY = state->cameraY;
        mapScrollInitialized = TRUE;
    }
}

static void initWorldMap(void)
{
    mapScrollInitialized = FALSE;
    if ((TERRAIN_VRAM_TILE_INDEX + terrain_tiles.numTile) > UNIT_VRAM_TILE_INDEX)
        fatalError("ERROR: el atlas de terreno invade la VRAM de las unidades");
    if ((CURSOR_VRAM_TILE_INDEX + cell_cursor.maxNumTile) > VRAM_TILE_COUNT)
        fatalError("ERROR: el cursor supera el final de la VRAM");

    PAL_setPalette(PAL0, terrain_palette.data, DMA_QUEUE_COPY);
    VDP_loadTileSet(&terrain_tiles, TERRAIN_VRAM_TILE_INDEX, DMA);

    /* Reserva un tile fuera del atlas para ocultar casillas no exploradas. */
    fogTileIndex = (u16)(TERRAIN_VRAM_TILE_INDEX + terrain_tiles.numTile);
    if (fogTileIndex >= UNIT_VRAM_TILE_INDEX)
        fatalError("ERROR: no queda un tile libre para la niebla");
    fogTileReservation = SPR_addSpriteEx(
        &fog_tile_reservation, -CELL_PIXELS, -CELL_PIXELS,
        TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, fogTileIndex),
        SPR_FLAG_AUTO_TILE_UPLOAD | SPR_FLAG_INSERT_HEAD);
    if (fogTileReservation == NULL)
        fatalError("ERROR: no se pudo reservar el tile de niebla");
    if (fog_tile_reservation.maxNumTile != 1)
        fatalError("ERROR: el tile de niebla debe ocupar un solo patron");
    SPR_setPosition(fogTileReservation, -CELL_PIXELS, -CELL_PIXELS);
    SPR_setVisibility(fogTileReservation, HIDDEN);
    /* Cada nibble vale 1: color azul oscuro de PAL2, opaco sobre BG_A. */
    VDP_fillTileData(0x11, fogTileIndex, 1, TRUE);

    worldMap = MAP_create(activeScenario->graphics, BG_A,
        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TERRAIN_VRAM_TILE_INDEX));
    if (worldMap == NULL) fatalError("ERROR: no se pudo crear el mapa");
    if (!MovementGlow_init(worldMap, terrain_palette.data))
        fatalError("ERROR: no se pudo inicializar el brillo del alcance");

    fogRenderedTeam = GameTurn_getActiveTeam(&gameTurn);
    fogRenderedSpectatorMode = FALSE;
    aiSpectatorView = FALSE;
    aiSpectatorHudRefreshPending = FALSE;
    cameraReturnPending = FALSE;
    cursorHiddenByAI = FALSE;
    cursorVisibilityDirty = FALSE;
    aiPresentationHoldFrames = 0;
    MovementGlow_setFogContext(activeScenario->logic,
        fogRenderedTeam, fogTileIndex, TRUE, FALSE);
}

static void initVideo(void)
{
    CameraCursorState state;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    /* Conserva la geometría de planos inicializada por SGDK y su fuente. */
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    SPR_init();
    initCellCursor();
    if (!UnitView_init(&unitView, &unitController, unitSprites,
                       (u8)ARRAY_COUNT(unitSprites)))
        fatalError("ERROR: no se pudo inicializar la representacion de unidades");
    state = CameraCursor_getState();
    UnitView_setFogContext(&unitView, activeScenario->logic,
                           GameTurn_getActiveTeam(&gameTurn), TRUE, FALSE);
    (void)UnitView_update(&unitView, &unitController, &state);
    initObjectiveMarker();
    SPR_update();
    SYS_doVBlankProcess();

    initWorldMap();
    SYS_doVBlankProcess();

    state = CameraCursor_getState();
    scrollMapIfNeeded(&state);
    SYS_doVBlankProcess();

    CellHud_init();
    CellHud_setUnitController(&unitController);
    Team_initPalettes();
    CellHud_setFogContext(GameTurn_getActiveTeam(&gameTurn), TRUE, FALSE);
    CellHud_setUnitStatus("A: MOVER / ATACAR | START: TURNO");
    updateTurnHud();
    /* No pintar máscaras geométricas sobre el mapa: la niebla se decide por casilla.
       Las celdas parciales del borde muestran el terreno real de BG_A. */
    /* HUD fijo en la esquina superior derecha; el valor se actualiza por frame. */
    VDP_drawTextBGFill(BG_B, "FPS: ", 27, 0, 5);
    updateObjectiveMarker(&state);
    SPR_update();
    SYS_doVBlankProcess();
}

/* -------------------------------------------------------------------------
 * Cursor, vista previa de ruta y selección
 * ------------------------------------------------------------------------- */

static void updateCellHud(const CameraCursorState *state, bool clicked)
{
    CellHud_update(activeScenario->logic, state->cameraX, state->cameraY,
                   state->cursorX, state->cursorY, clicked);
}

static void showSelectedUnitInfo(void);

static void updateRoutePreview(const CameraCursorState *state)
{
    u16 cellX;
    u16 cellY;
    bool reachable;
    u16 steps;
    u8 cost;
    u8 availablePoints;
    const Unit *selected;

    getCursorCell(state, &cellX, &cellY);
    if (!FogOfWar_isExplored(activeScenario->logic,
                             GameTurn_getActiveTeam(&gameTurn), cellX, cellY))
    {
        CellHud_setContextStatus("CASILLA NO EXPLORADA");
        return;
    }
    selected = UnitController_getSelected(&unitController);
    if (selected == NULL)
    {
        /* Invalida una previsualización antigua tras perder la selección. */
        (void)UnitController_updatePreview(&unitController, cellX, cellY);
        CellHud_setContextStatus("A: SELECCIONAR UNIDAD");
        return;
    }
    if ((selected->x == cellX) && (selected->y == cellY))
    {
        /* Sobre su origen, conserva visible la ficha de la unidad seleccionada
           en vez de mostrar una ruta trivial de cero pasos. Esta llamada también
           descarta una ruta pendiente si el cursor ha vuelto al origen. */
        (void)UnitController_updatePreview(&unitController, cellX, cellY);
        showSelectedUnitInfo();
        return;
    }
    if (!UnitController_updatePreview(&unitController, cellX, cellY) &&
        !hudContextRefreshPending)
        return;

    selected = UnitController_getSelected(&unitController);
    if (selected != NULL && FogOfWar_isVisible(activeScenario->logic,
            GameTurn_getActiveTeam(&gameTurn), cellX, cellY))
    {
        const Unit *target = UnitController_getUnitAt(
            &unitController, cellX, cellY);
        if ((target != NULL) && (target->team == selected->team) &&
            (target->id != selected->id) && (selected->repairAmount > 0))
        {
            const RepairResult repair = Combat_predictRepairUnits(
                &unitController, selected, target,
                GameTurn_getActiveTeam(&gameTurn));
            switch (repair.code)
            {
                case REPAIR_RESULT_CAN_REPAIR:
                    CellHud_setRepairPreview(repair.healthRestored,
                        (u8)(target->health + repair.healthRestored),
                        target->maxHealth);
                    return;
                case REPAIR_RESULT_TARGET_FULL_HEALTH:
                    CellHud_setContextStatus("ALIADO SIN DANOS");
                    return;
                case REPAIR_RESULT_OUT_OF_RANGE:
                    CellHud_setContextStatus("ALIADO FUERA DE ALCANCE");
                    return;
                case REPAIR_RESULT_ALREADY_ACTED:
                    CellHud_setContextStatus("INGENIERO YA ACTUO ESTE TURNO");
                    return;
                default:
                    break;
            }
        }
        if (target != NULL && target->team != selected->team)
        {
            const CombatResult preview = Combat_predictUnits(
                &unitController, selected, target,
                GameTurn_getActiveTeam(&gameTurn));
            if (preview.code == COMBAT_RESULT_ATTACKED)
            {
                CellHud_setCombatPreview(preview.damageDealt,
                                         preview.counterDamage,
                                         target->health,
                                         target->maxHealth);
                return;
            }
            if (preview.code == COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT)
            {
                CellHud_setContextStatus("OBSTACULO BLOQUEA TIRO");
                return;
            }
            if (preview.code == COMBAT_RESULT_OUT_OF_RANGE)
            {
                CellHud_setContextStatus("OBJETIVO FUERA DE ALCANCE");
                return;
            }
            if (preview.code == COMBAT_RESULT_ALREADY_ACTED)
            {
                CellHud_setContextStatus("UNIDAD YA HA ACTUADO");
                return;
            }
        }
    }

    if ((selected != NULL) && selected->hasAttackedThisTurn)
    {
        CellHud_setContextStatus("ATAQUE YA REALIZADO ESTE TURNO");
        return;
    }
    if ((selected != NULL) && selected->hasRepairedThisTurn)
    {
        CellHud_setContextStatus("REPARACION YA REALIZADA ESTE TURNO");
        return;
    }
    if ((selected != NULL) && (selected->movementPoints == 0))
    {
        CellHud_setContextStatus("MOVIMIENTO AGOTADO");
        return;
    }

    if (UnitController_getPreviewInfo(&unitController, &reachable,
                                      &steps, &cost, &availablePoints))
        CellHud_setRouteStatus(reachable, steps, cost, availablePoints);
    else if (UnitController_getSelected(&unitController) != NULL)
        CellHud_setContextStatus("UNIDAD SELECCIONADA: MUEVE CURSOR");
}

static bool sameMovementRangeCache(const MovementRangeCache *a,
                                   const MovementRangeCache *b)
{
    return a->initialized && (a->enabled == b->enabled) &&
        (a->selectedIndex == b->selectedIndex) && (a->unitX == b->unitX) &&
        (a->unitY == b->unitY) &&
        (a->movementPoints == b->movementPoints) &&
        (a->hasUsedAction == b->hasUsedAction);
}

/* Recalcula solo cuando cambia la unidad o el presupuesto; no cada fotograma. */
static void updateMovementRangeVisuals(const CameraCursorState *state)
{
    const Unit *selected = UnitController_getSelected(&unitController);
    const bool enabled = (selected != NULL) &&
        (selected->team == GameTurn_getActiveTeam(&gameTurn)) &&
        !UnitController_isMoving(&unitController) &&
        !GameTurn_isFinished(&gameTurn);
    MovementRangeCache next;
    const u8 *mask;
    const MapData *map = activeScenario->logic;

    next.initialized = TRUE;
    next.enabled = enabled;
    next.selectedIndex = enabled
        ? UnitController_getSelectedIndex(&unitController) : -1;
    next.unitX = enabled ? selected->x : 0;
    next.unitY = enabled ? selected->y : 0;
    next.movementPoints = enabled ? selected->movementPoints : 0;
    next.hasUsedAction = enabled
        ? (selected->hasAttackedThisTurn || selected->hasRepairedThisTurn)
        : FALSE;

    /*
     * Al terminar una orden el controlador deselecciona la unidad. Como el
     * alcance ya se oculto al empezar a mover, ese estado no cambia al llegar
     * y evitamos forzar un redibujado completo del mapa en el ultimo fotograma.
     */
    if (sameMovementRangeCache(&movementRangeCache, &next)) return;

    /* No construyas el alcance cuando lo estamos ocultando. En la versión
       anterior se ejecutaba otro pathfinding justo al comenzar cada ruta. */
    mask = NULL;
    if (enabled)
    {
        if (!UnitController_refreshMovementRange(&unitController))
            fatalError("ERROR: no se pudo calcular el alcance de movimiento");
        mask = UnitController_getMovementRange(&unitController);
        if (!Combat_buildActionTargetMask(&unitController, selected,
                GameTurn_getActiveTeam(&gameTurn), actionTargetMask,
                (u16)sizeof(actionTargetMask)))
            fatalError("ERROR: no se pudieron calcular los objetivos de accion");
    }
    else
    {
        memset(actionTargetMask, 0, sizeof(actionTargetMask));
    }
    CameraCursor_setMovementMask(mask, map->width, map->height,
        enabled ? selected->x : 0, enabled ? selected->y : 0);
    if (!MovementGlow_setMasks(mask, enabled ? actionTargetMask : NULL,
                               map->width, map->height,
                               (u32)state->cameraX, (u32)state->cameraY))
    {
        /* Recuperación excepcional si aún no hay contexto/caché visual válido.
           El camino normal usa el parcheo incremental, sin redraw completo. */
        MovementGlow_setReachableMask(mask, map->width, map->height);
        MovementGlow_setAttackableMask(
            enabled ? actionTargetMask : NULL, map->width, map->height);
        MovementGlow_redraw((u32)state->cameraX, (u32)state->cameraY);
    }

    /* El parcheo incremental usa la posición de cámara actual. */
    lastMapScrollX = state->cameraX;
    lastMapScrollY = state->cameraY;
    mapScrollInitialized = TRUE;
    movementRangeCache = next;
}

static void showSelectedUnitInfo(void)
{
    const Unit *unit = UnitController_getSelected(&unitController);
    const UnitTypeInfo *info;

    if (unit == NULL) return;
    info = UnitType_getInfo(unit->type);
    CellHud_setUnitCombatInfo((info != NULL) ? info->shortName : "UNIDAD",
        unit->health, unit->maxHealth, unit->attack, unit->defense,
        unit->attackRange, unit->visionRange, unit->repairRange,
        unit->repairAmount);
}

static void showCombatResult(const CombatResult *result, const Unit *defender)
{
    char buffer[HUD_TEXT_WIDTH + 1];

    if ((result == NULL) || (result->code != COMBAT_RESULT_ATTACKED))
    {
        if (result == NULL) CellHud_setActionFeedback("ATAQUE NO DISPONIBLE");
        else
        {
            switch (result->code)
            {
                case COMBAT_RESULT_NOT_ENEMY:
                    CellHud_setActionFeedback("NO PUEDES ATACAR A TU EQUIPO");
                    break;
                case COMBAT_RESULT_WRONG_TEAM:
                    CellHud_setActionFeedback("TURNO DEL OTRO EQUIPO");
                    break;
                case COMBAT_RESULT_OUT_OF_RANGE:
                    CellHud_setActionFeedback("OBJETIVO FUERA DE ALCANCE");
                    break;
                case COMBAT_RESULT_ALREADY_ACTED:
                    CellHud_setActionFeedback("ESTA UNIDAD YA HA ACTUADO");
                    break;
                case COMBAT_RESULT_NOT_VISIBLE:
                    CellHud_setActionFeedback("OBJETIVO NO VISIBLE");
                    break;
                case COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT:
                    CellHud_setActionFeedback("OBSTACULO BLOQUEA EL DISPARO");
                    break;
                default:
                    CellHud_setActionFeedback("OBJETIVO NO DISPONIBLE");
                    break;
            }
        }
        return;
    }

    if (result->attackerDefeated)
        CellHud_setActionFeedback("ATACANTE ELIMINADO");
    else if (result->defenderDefeated)
    {
        uintToStr(result->damageDealt, buffer, 1);
        {
            char message[HUD_TEXT_WIDTH + 1];
            u16 i = 0;
            const char *prefix = "ENEMIGO ELIMINADO (-";
            while (*prefix && i < HUD_TEXT_WIDTH) message[i++] = *prefix++;
            prefix = buffer;
            while (*prefix && i < HUD_TEXT_WIDTH) message[i++] = *prefix++;
            prefix = " HP)";
            while (*prefix && i < HUD_TEXT_WIDTH) message[i++] = *prefix++;
            message[i] = '\0';
            CellHud_setActionFeedback(message);
        }
    }
    else
    {
        char damage[11];
        char health[11];
        char maxHealth[11];
        char retaliation[11];
        char message[HUD_TEXT_WIDTH + 1];
        u16 i = 0;
        const char *part;

        uintToStr(result->damageDealt, damage, 1);
        uintToStr((defender != NULL) ? defender->health : 0, health, 1);
        uintToStr((defender != NULL) ? defender->maxHealth : 0, maxHealth, 1);
        uintToStr(result->counterDamage, retaliation, 1);
        part = "ATQ -";
        while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
        part = damage;
        while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
        if (defender != NULL)
        {
            part = " / ENE HP ";
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
            part = health;
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
            part = "/";
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
            part = maxHealth;
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
        }
        if (result->counterDamage > 0)
        {
            part = " / REP -";
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
            part = retaliation;
            while (*part && i < HUD_TEXT_WIDTH) message[i++] = *part++;
        }
        message[i] = '\0';
        CellHud_setActionFeedback(message);
    }
}

static void setCellCursorHidden(bool hidden)
{
    if ((cellCursor == NULL) || (cursorHiddenByAI == hidden)) return;

    cursorHiddenByAI = hidden;
    SPR_setVisibility(cellCursor, hidden ? HIDDEN : VISIBLE);
    cursorVisibilityDirty = TRUE;
}

/* -------------------------------------------------------------------------
 * Cámara espectadora de la IA y foco del HUD
 * ------------------------------------------------------------------------- */

static void beginAISpectatorView(const CameraCursorState *state)
{
    if ((state == NULL) || aiSpectatorView) return;

    aiReturnCameraX = state->cameraX;
    aiReturnCameraY = state->cameraY;
    aiReturnCameraValid = TRUE;
    aiSpectatorView = TRUE;
    aiSpectatorHudRefreshPending = FALSE;
    cameraReturnPending = FALSE;
    aiPresentationHoldFrames = 0;

    /* El sprite de selección no debe quedarse flotando sobre otra zona. */
    setCellCursorHidden(TRUE);
    CameraCursor_setAutoCameraTarget(state->cameraX, state->cameraY);
    CellHud_setUnitStatus("IA ROJA: TURNO EN CURSO");
    fogUpdatePending = TRUE;
}

static void endAISpectatorView(void)
{
    if (!aiSpectatorView) return;

    aiSpectatorView = FALSE;
    aiSpectatorHudRefreshPending = FALSE;
    aiPresentationHoldFrames = 0;
    if (aiReturnCameraValid)
    {
        CameraCursor_setAutoCameraTarget(aiReturnCameraX, aiReturnCameraY);
        cameraReturnPending = CameraCursor_isAutoCameraActive();
    }
    else cameraReturnPending = FALSE;

    /* No enseñes el cursor hasta haber recuperado el encuadre del jugador. */
    if (!cameraReturnPending) setCellCursorHidden(FALSE);
    CellHud_setUnitStatus(cameraReturnPending
        ? "VOLVIENDO A TURNO AZUL" : "TURNO AZUL");
    fogUpdatePending = TRUE;
}

static bool getAISpectatorFocus(s32 *worldX, s32 *worldY)
{
    s16 unitIndex;
    const Unit *unit;

    if ((worldX == NULL) || (worldY == NULL)) return FALSE;

    if (UnitController_isMoving(&unitController))
        unitIndex = unitController.movingIndex;
    else if (aiPresentationHoldFrames > 0)
        unitIndex = EnemyAI_getFocusUnitIndex(&enemyAI);
    else
    {
        unitIndex = EnemyAI_getNextActorIndex(&enemyAI, &unitController,
            GameTurn_getNumber(&gameTurn));
        if (unitIndex < 0)
            unitIndex = EnemyAI_getFocusUnitIndex(&enemyAI);
    }
    if (unitIndex < 0) return FALSE;

    unit = UnitController_getUnit(&unitController, (u8)unitIndex);
    if ((unit == NULL) || !unit->alive || (unit->team != TEAM_RED))
        return FALSE;

    UnitController_getRenderPosition(&unitController, (u8)unitIndex,
                                     worldX, worldY);
    return TRUE;
}

/* La IA puede actuar en cuanto el siguiente actor está encuadrado con margen;
   no hace falta colocarlo exactamente en el centro del viewport. */
static bool isAINextActorReadyForAction(void)
{
    const s16 unitIndex = EnemyAI_getNextActorIndex(&enemyAI, &unitController,
        GameTurn_getNumber(&gameTurn));
    s32 worldX;
    s32 worldY;

    if (unitIndex < 0) return TRUE;
    UnitController_getRenderPosition(&unitController, (u8)unitIndex,
                                     &worldX, &worldY);
    return CameraCursor_isWorldPointInsideSafeZone(
        worldX + CELL_PIXELS / 2, worldY + CELL_PIXELS / 2,
        CELL_PIXELS * AI_CAMERA_FOLLOW_START_CELLS,
        CELL_PIXELS * AI_CAMERA_FOLLOW_START_CELLS);
}

/* Seguimiento cinematográfico de la unidad roja que está tomando decisiones.
   Durante ese turno la presentación combina las áreas visibles de ambos
   equipos, sin revelar las celdas que siguen desconocidas para los dos. */
static void updateAISpectatorCamera(CameraCursorState *state)
{
    s32 worldX;
    s32 worldY;
    const bool haveFocus = getAISpectatorFocus(&worldX, &worldY);

    if (!aiSpectatorView || (state == NULL)) return;

    if (haveFocus)
    {
        const bool moving = UnitController_isMoving(&unitController);
        const bool autoCameraActive = CameraCursor_isAutoCameraActive();
        /*
         * Para una unidad quieta, solo desplaza la cámara si queda fuera de la
         * zona segura. Durante un movimiento, aplica histéresis: empieza a
         * seguir al cruzar el margen de una celda y deja de seguir al regresar
         * a un margen de tres celdas. Así no se mantiene el scroll activo durante
         * todo el recorrido, ni se recentra a cada nuevo actor visible.
         */
        const u16 marginCells = (moving && autoCameraActive)
            ? AI_CAMERA_FOLLOW_STOP_CELLS : AI_CAMERA_FOLLOW_START_CELLS;
        const bool mustFollow = !CameraCursor_isWorldPointInsideSafeZone(
            worldX + CELL_PIXELS / 2, worldY + CELL_PIXELS / 2,
            CELL_PIXELS * marginCells, CELL_PIXELS * marginCells);

        if (mustFollow)
            CameraCursor_focusWorldPixel(worldX + CELL_PIXELS / 2,
                                         worldY + CELL_PIXELS / 2);
        else if (autoCameraActive)
        {
            /* Cancela el destino antiguo cuando el actor ya está bien visible. */
            CameraCursor_setAutoCameraTarget(state->cameraX, state->cameraY);
        }
    }
    (void)CameraCursor_updateAutoCamera();
    *state = CameraCursor_getState();

    /*
     * No repintes el HUD en cada celda intermedia del movimiento. Escribir sus
     * cuatro líneas por cada paso provocaba tirones periódicos en la cámara de
     * la IA. Cámara y sprites siguen actualizándose a cada fotograma; el HUD se
     * refresca una sola vez al terminar la ruta.
     */
    if (UnitController_isMoving(&unitController))
    {
        aiSpectatorHudRefreshPending = TRUE;
    }
    else if (aiSpectatorHudRefreshPending)
    {
        /* El foco puede cambiar al siguiente actor; dibuja solo ese destino. */
        if (haveFocus)
            CellHud_inspectCell(activeScenario->logic,
                (u16)(worldX / CELL_PIXELS), (u16)(worldY / CELL_PIXELS));
        aiSpectatorHudRefreshPending = FALSE;
    }
    else if (haveFocus)
    {
        /* Al cambiar de actor enfocado solo se repinta la celda una vez. */
        CellHud_inspectCell(activeScenario->logic,
            (u16)(worldX / CELL_PIXELS), (u16)(worldY / CELL_PIXELS));
    }
}

static void updateCameraReturn(CameraCursorState *state)
{
    if (!cameraReturnPending || (state == NULL)) return;

    (void)CameraCursor_updateAutoCamera();
    *state = CameraCursor_getState();
    if (!CameraCursor_isAutoCameraActive())
    {
        cameraReturnPending = FALSE;
        setCellCursorHidden(FALSE);
        CellHud_update(activeScenario->logic, state->cameraX, state->cameraY,
                       state->cursorX, state->cursorY, FALSE);
        CellHud_setUnitStatus("TURNO AZUL");
    }
}

/* -------------------------------------------------------------------------
 * Aplicación de la niebla lógica al mapa gráfico
 * ------------------------------------------------------------------------- */

static void refreshFogForActiveTeam(const CameraCursorState *state)
{
    const TeamId activeTeam = GameTurn_getActiveTeam(&gameTurn);
    const bool viewContextChanged = (activeTeam != fogRenderedTeam) ||
        (aiSpectatorView != fogRenderedSpectatorMode);
    const bool changed = FogOfWar_update(&fogOfWar, activeTeam);
    /* Las áreas móviles azules pendientes se drenan durante el turno azul,
       no en cada fotograma del turno rojo/espectador. Así el trabajo aplazado
       no se ejecuta ni se vuelve a solicitar 60 veces mientras actúa la IA. */
    fogRefreshPendingAfterMovement = (activeTeam == TEAM_BLUE) &&
        FogOfWar_hasPendingMovementExploration(&fogOfWar);
    Performance_count(PERF_EVENT_FOG_REFRESH, 1);
    Performance_count(PERF_EVENT_FOG_CELLS,
                       FogOfWar_getLastProcessedCellCount(&fogOfWar));

    if (!changed && !viewContextChanged) return;

    /* El modo espectador usa la unión de visión azul/roja. Los cambios de
       visión del actor actualizan solo las filas de niebla afectadas. */
    if (viewContextChanged)
    {
        /* Turno rojo: la vista une las áreas actualmente visibles por ambos
           equipos; las celdas desconocidas para los dos siguen cubiertas. */
        MovementGlow_setFogContext(activeScenario->logic, activeTeam,
                                   fogTileIndex, TRUE, aiSpectatorView);
        UnitView_setFogContext(&unitView, activeScenario->logic, activeTeam,
                               TRUE, aiSpectatorView);
        CellHud_setFogContext(activeTeam, TRUE, aiSpectatorView);
        MovementGlow_redraw((u32)state->cameraX, (u32)state->cameraY);
        lastMapScrollX = state->cameraX;
        lastMapScrollY = state->cameraY;
        mapScrollInitialized = TRUE;
        fogRenderedTeam = activeTeam;
        fogRenderedSpectatorMode = aiSpectatorView;
        CellHud_refresh();
        return;
    }

    MovementGlow_setFogContext(activeScenario->logic, activeTeam,
                               fogTileIndex, TRUE, aiSpectatorView);
    UnitView_setFogContext(&unitView, activeScenario->logic, activeTeam, TRUE,
                           aiSpectatorView);
    CellHud_setFogContext(activeTeam, TRUE, aiSpectatorView);

    {
        u16 cursorCellX;
        u16 cursorCellY;
        u16 dirtyIndex;
        bool cursorCellChanged = FALSE;
        const u16 dirtyCount = FogOfWar_getDirtyCellCount(&fogOfWar);

        UnitView_setFogDirtyCells(&unitView, fogOfWar.dirtyCellIndices,
                                  dirtyCount, activeScenario->logic->width);
        getCursorCell(state, &cursorCellX, &cursorCellY);
        for (dirtyIndex = 0; dirtyIndex < dirtyCount; dirtyIndex++)
        {
            u16 cellX;
            u16 cellY;

            if (!FogOfWar_getDirtyCell(&fogOfWar, dirtyIndex, &cellX, &cellY))
                continue;
            if ((cellX == cursorCellX) && (cellY == cursorCellY))
                cursorCellChanged = TRUE;
        }

        /*
         * FogOfWar registra las celdas en orden de fila (Y, después X).
         * Agrupamos las que necesitan cambiar tiles para hacer una transferencia
         * por fila de tiles visuales, no tres transferencias por cada celda.
         */
        dirtyIndex = 0;
        while (dirtyIndex < dirtyCount)
        {
            u16 runFirstX;
            u16 runLastX;
            u16 runY;
            u16 runLastIndex;
            u16 cellX;
            u16 cellY;

            if (!FogOfWar_getDirtyCell(&fogOfWar, dirtyIndex, &cellX, &cellY))
            {
                dirtyIndex++;
                continue;
            }
            if (!FogOfWar_didTileChange(&fogOfWar, dirtyIndex))
            {
                dirtyIndex++;
                continue;
            }

            runFirstX = cellX;
            runLastX = cellX;
            runY = cellY;
            runLastIndex = dirtyIndex;

            while ((runLastIndex + 1) < dirtyCount)
            {
                u16 nextX;
                u16 nextY;
                const u16 nextIndex = (u16)(runLastIndex + 1);

                if (!FogOfWar_didTileChange(&fogOfWar, nextIndex) ||
                    !FogOfWar_getDirtyCell(&fogOfWar, nextIndex,
                                           &nextX, &nextY) ||
                    (nextY != runY) || (nextX != (u16)(runLastX + 1)))
                    break;

                runLastX = nextX;
                runLastIndex = nextIndex;
            }

            if (MovementGlow_refreshFogCellRun(runFirstX, runLastX, runY,
                (u32)state->cameraX, (u32)state->cameraY))
                Performance_count(PERF_EVENT_FOG_ROW_RUNS, 1);
            dirtyIndex = (u16)(runLastIndex + 1);
        }

        /* No reescribas el HUD si la casilla inspeccionada no cambió de visión. */
        if (cursorCellChanged) CellHud_refresh();
    }
}

/* -------------------------------------------------------------------------
 * Órdenes del jugador y activación del combate
 * ------------------------------------------------------------------------- */

static void showRepairResult(const RepairResult *result, const Unit *ally)
{
    if (result == NULL)
    {
        CellHud_setActionFeedback("REPARACION NO DISPONIBLE");
        return;
    }

    switch (result->code)
    {
        case REPAIR_RESULT_REPAIRED:
            if (ally != NULL)
                CellHud_setRepairResult(result->healthRestored,
                                        ally->health, ally->maxHealth);
            else
                CellHud_setActionFeedback("REPARACION COMPLETADA");
            break;
        case REPAIR_RESULT_WRONG_TEAM:
            CellHud_setActionFeedback("TURNO DEL OTRO EQUIPO");
            break;
        case REPAIR_RESULT_NOT_ENGINEER:
            CellHud_setActionFeedback("SOLO INGENIEROS PUEDEN REPARAR");
            break;
        case REPAIR_RESULT_NOT_ALLY:
            CellHud_setActionFeedback("SOLO SE REPARAN ALIADOS");
            break;
        case REPAIR_RESULT_OUT_OF_RANGE:
            CellHud_setActionFeedback("ALIADO FUERA DE ALCANCE");
            break;
        case REPAIR_RESULT_TARGET_FULL_HEALTH:
            CellHud_setActionFeedback("ALIADO SIN DANOS");
            break;
        case REPAIR_RESULT_ALREADY_ACTED:
            CellHud_setActionFeedback("INGENIERO YA ACTUO ESTE TURNO");
            break;
        default:
            CellHud_setActionFeedback("REPARACION NO DISPONIBLE");
            break;
    }
}

static void activateCursorCell(const CameraCursorState *state)
{
    u16 cellX;
    u16 cellY;
    const TeamId activeTeam = GameTurn_getActiveTeam(&gameTurn);
    const Unit *attacker;
    const Unit *target;

    if (!GameTurn_canIssueCommands(&gameTurn)) return;
    getCursorCell(state, &cellX, &cellY);
    if (!FogOfWar_isExplored(activeScenario->logic, activeTeam, cellX, cellY))
    {
        CellHud_setActionFeedback("CASILLA NO EXPLORADA");
        return;
    }

    attacker = UnitController_getSelected(&unitController);
    target = FogOfWar_isVisible(activeScenario->logic, activeTeam, cellX, cellY)
        ? UnitController_getUnitAt(&unitController, cellX, cellY) : NULL;
    if ((attacker != NULL) && (target != NULL) &&
        (attacker->repairAmount > 0) && (target->team == attacker->team) &&
        (target->id != attacker->id))
    {
        const RepairResult repair = Combat_repair(
            &unitController, attacker->id, target->id, activeTeam);
        showRepairResult(&repair, target);
        /* La casilla inspeccionada puede haber cambiado de PV. */
        CellHud_refresh();
        if (repair.code == REPAIR_RESULT_REPAIRED)
            movementRangeCache.initialized = FALSE;
        return;
    }
    if ((attacker != NULL) && (target != NULL) &&
        (target->team != attacker->team))
    {
        CombatResult combatResult;
        combatResult = Combat_attack(&unitController, attacker->id,
                                     target->id, activeTeam);
        showCombatResult(&combatResult, target);
        /* Refleja inmediatamente los PV nuevos o la casilla ya liberada. */
        CellHud_refresh();
        (void)GameTurn_checkObjective(&gameTurn);
        /* Una baja puede no cambiar la niebla por visión solapada; la vista sí cambia. */
        UnitView_invalidate(&unitView);
        UnitController_invalidateMovementRange(&unitController);
        movementRangeCache.initialized = FALSE;
        fogUpdatePending = TRUE;
        return;
    }

    {
        const UnitActionResult action = UnitController_activate(
            &unitController, cellX, cellY);
        if (action == UNIT_ACTION_SELECTED)
        {
            CellHud_setUnitStatus(UnitController_getActionText(action));
            showSelectedUnitInfo();
        }
        else CellHud_setActionFeedback(UnitController_getActionText(action));
    }
}

/*
 * Una ruta solo puede alterar la propiedad de un objetivo de casilla al salir
 * de él o entrar en él. Evita escanear todo el ejército en cada paso normal.
 */
static bool movementCanChangeCellObjective(const MissionObjective *objective,
                                           u16 oldX, u16 oldY,
                                           u16 newX, u16 newY)
{
    if (objective == NULL) return FALSE;
    if ((objective->type != MISSION_CAPTURE_POINT) &&
        (objective->type != MISSION_HOLD_POINT))
        return FALSE;

    return ((oldX == objective->x) && (oldY == objective->y)) ||
           ((newX == objective->x) && (newY == objective->y));
}

#if PERF_DIAGNOSTICS
static PerformancePhase classifyPerformancePhase(u16 joy)
{
    const TeamId activeTeam = GameTurn_getActiveTeam(&gameTurn);
    const bool moving = UnitController_isMoving(&unitController);
    const u16 directions = (u16)(BUTTON_UP | BUTTON_DOWN |
                                  BUTTON_LEFT | BUTTON_RIGHT);

    if (activeTeam == TEAM_RED)
    {
        if (moving)
            return CameraCursor_isAutoCameraActive()
                ? PERF_PHASE_AI_MOVE_CAMERA : PERF_PHASE_AI_MOVE;
        if (CameraCursor_isAutoCameraActive()) return PERF_PHASE_AI_CAMERA;
        return PERF_PHASE_AI_DECISION;
    }

    if (cameraReturnPending || CameraCursor_isAutoCameraActive())
        return PERF_PHASE_CAMERA_RETURN;
    if (moving) return PERF_PHASE_PLAYER_MOVE;
    if ((joy & BUTTON_C) && (joy & directions))
        return PERF_PHASE_PLAYER_SCROLL;
    if (joy & directions) return PERF_PHASE_PLAYER_CURSOR;
    return PERF_PHASE_IDLE;
}
#endif

/* Consolida trabajo aplazado antes de leer nuevas órdenes del jugador. */
/* -------------------------------------------------------------------------
 * Fases de frame: órdenes, trabajo aplazado y presentación
 * ------------------------------------------------------------------------- */

static void processFogPendingBeforeInput(CameraCursorState *state,
                                         bool *fogRefreshedThisFrame)
{
    if (!fogRefreshPendingAfterMovement ||
        UnitController_isMoving(&unitController))
        return;

    *state = CameraCursor_getState();
    refreshFogForActiveTeam(state);
    *fogRefreshedThisFrame = TRUE;

    if (movementCellHudRefreshPending)
    {
        CellHud_refresh();
        movementCellHudRefreshPending = FALSE;
    }
}

/* Actualiza una unidad en movimiento y registra cambios al cruzar una celda. */
static void updateMovingUnitFrame(CameraCursorState *state,
                                  bool *movementEndedThisFrame)
{
    const Unit *movingUnit;

    if (!GameTurn_canIssueCommands(&gameTurn)) return;
    movingUnit = UnitController_getSelected(&unitController);
    const u16 oldX = (movingUnit != NULL) ? movingUnit->x : 0;
    const u16 oldY = (movingUnit != NULL) ? movingUnit->y : 0;
    const UnitActionResult result = UnitController_update(&unitController);

    *movementEndedThisFrame = !UnitController_isMoving(&unitController);
    *state = CameraCursor_getState();

    /* El controlador deselecciona la unidad al acabar; su registro sigue vivo. */
    if ((movingUnit != NULL) &&
        ((movingUnit->x != oldX) || (movingUnit->y != oldY)))
    {
        movementCellHudRefreshPending = TRUE;
        /* La niebla se consolida al terminar la ruta, no en cada casilla. */
        fogUpdatePending = TRUE;

        if (movementCanChangeCellObjective(
                GameTurn_getObjective(&gameTurn), oldX, oldY,
                movingUnit->x, movingUnit->y) &&
            GameTurn_checkObjective(&gameTurn))
            CellHud_setUnitStatus("VICTORIA: OBJETIVO CUMPLIDO");
    }

    if (GameTurn_isFinished(&gameTurn)) return;

    if ((result == UNIT_ACTION_MOVED) || (result == UNIT_ACTION_BLOCKED) ||
        (result == UNIT_ACTION_NO_MOVEMENT_POINTS))
        CellHud_setActionFeedback(UnitController_getActionText(result));
}

/* Ejecuta una sola decisión roja; el movimiento continúa en el bucle de frames. */
static void updateEnemyTeamFrame(void)
{
    const MissionObjective *objective;
    CombatResult combatResult;
    EnemyAIResult result = ENEMY_AI_IDLE;

    if (!GameTurn_canIssueCommands(&gameTurn)) return;
    objective = GameTurn_getObjective(&gameTurn);

    /* Reserva tiempo para que el jugador vea el resultado del ataque. */
    if (aiPresentationHoldFrames > 0)
        aiPresentationHoldFrames--;
    else if (isAINextActorReadyForAction())
        result = EnemyAI_update(&enemyAI, &unitController,
                                activeScenario->logic,
                                GameTurn_getActiveTeam(&gameTurn),
                                GameTurn_getNumber(&gameTurn), objective,
                                &combatResult);
    else
        CellHud_setUnitStatus("IA ROJA: ENFOCANDO UNIDAD");

    if (result == ENEMY_AI_ATTACK_RESOLVED)
    {
        Performance_count(PERF_EVENT_AI_ATTACK, 1);
        showCombatResult(&combatResult, NULL);
        aiPresentationHoldFrames = AI_ATTACK_PRESENTATION_FRAMES;
        UnitView_invalidate(&unitView);
        UnitController_invalidateMovementRange(&unitController);
        movementRangeCache.initialized = FALSE;
        fogUpdatePending = TRUE;

        if (GameTurn_checkObjective(&gameTurn))
            CellHud_setUnitStatus("MISION COMPLETADA");
    }
    else if (result == ENEMY_AI_REPAIR_RESOLVED)
    {
        CellHud_setUnitStatus("IA ROJA: INGENIERO REPARA");
        aiPresentationHoldFrames = 12;
    }
    else if (result == ENEMY_AI_MOVE_STARTED)
    {
        Performance_count(PERF_EVENT_AI_MOVE, 1);
        CellHud_setUnitStatus("IA ROJA: MOVIENDO");
    }
    else if (result == ENEMY_AI_RETREAT_STARTED)
    {
        Performance_count(PERF_EVENT_AI_MOVE, 1);
        CellHud_setUnitStatus("IA ROJA: RETIRADA");
    }
    else if (result == ENEMY_AI_TURN_FINISHED)
    {
        const TeamId previousTeam = GameTurn_getActiveTeam(&gameTurn);

        if (GameTurn_endTurn(&gameTurn))
        {
            if (GameTurn_getActiveTeam(&gameTurn) != previousTeam)
                fogUpdatePending = TRUE;
            CellHud_setUnitStatus("FIN TURNO IA ROJA");
        }
    }
}

/* Resuelve entradas y órdenes exclusivas del turno del jugador azul. */
static void updatePlayerTeamFrame(u16 joy, bool clicked, bool cancelPressed,
                                  bool endTurnPressed,
                                  CameraCursorState *state)
{
    if (!GameTurn_canIssueCommands(&gameTurn)) return;
    CameraCursor_update(joy);
    *state = CameraCursor_getState();
    updateCellHud(state, clicked);

    if (endTurnPressed)
    {
        const TeamId previousTeam = GameTurn_getActiveTeam(&gameTurn);

        if (GameTurn_endTurn(&gameTurn))
        {
            const TeamInfo *endedTeam = Team_getInfo(previousTeam);
            CellHud_setUnitStatus((endedTeam != NULL)
                ? (previousTeam == TEAM_BLUE ? "FIN TURNO AZUL" : "FIN TURNO ROJO")
                : "TURNO FINALIZADO");
        }
        else
            CellHud_setActionFeedback("NO SE PUEDE TERMINAR EL TURNO");

        if (GameTurn_getActiveTeam(&gameTurn) != previousTeam)
            fogUpdatePending = TRUE;
    }
    else if (cancelPressed)
    {
        CellHud_setActionFeedback(UnitController_getActionText(
            UnitController_cancelOrder(&unitController)));
    }
    else if (clicked)
    {
        activateCursorCell(state);
    }
    else
    {
        updateRoutePreview(state);
    }
}

/* Mantiene alineados turno, modo espectador y contexto visual de la niebla. */
static void synchronizeTeamTransition(TeamId previousActiveTeam,
                                      const CameraCursorState *state)
{
    const TeamId activeTeamNow = GameTurn_getActiveTeam(&gameTurn);

    if (activeTeamNow == previousActiveTeam) return;

    if (activeTeamNow == TEAM_RED)
        beginAISpectatorView(state);
    else if (previousActiveTeam == TEAM_RED)
        endAISpectatorView();

    fogUpdatePending = TRUE;
}

/* Ejecuta los cambios aplazados después de presentar la llegada de la unidad. */
static void processMovementCompletionWork(const CameraCursorState *state,
                                          bool movementEndedThisFrame,
                                          bool fogRefreshedThisFrame)
{
    if (fogUpdatePending)
        fogRefreshPendingAfterMovement = TRUE;

    /* El último fotograma de interpolación debe verse antes de refrescar niebla. */
    if (UnitController_isMoving(&unitController) || movementEndedThisFrame)
        return;

    if (fogRefreshPendingAfterMovement && !fogRefreshedThisFrame)
        refreshFogForActiveTeam(state);

    /* El HUD inspecciona la casilla del cursor, no la ruta entera. */
    if (movementCellHudRefreshPending)
    {
        CellHud_refresh();
        movementCellHudRefreshPending = FALSE;
    }
}

/* Aplica cambios visuales con caché y publica un único fotograma VDP. */
static void presentGameFrame(CameraCursorState *state)
{
    bool spriteTableDirty = FALSE;

    if (aiSpectatorView)
        updateAISpectatorCamera(state);
    else if (cameraReturnPending)
        updateCameraReturn(state);

    updateMovementRangeVisuals(state);
    scrollMapIfNeeded(state);

    if ((cellCursor != NULL) &&
        ((state->cursorX != lastCursorSpriteX) ||
         (state->cursorY != lastCursorSpriteY)))
    {
        SPR_setPosition(cellCursor, state->cursorX, state->cursorY);
        lastCursorSpriteX = state->cursorX;
        lastCursorSpriteY = state->cursorY;
        spriteTableDirty = TRUE;
    }

    if (UnitView_update(&unitView, &unitController, state))
        spriteTableDirty = TRUE;
    if (updateObjectiveMarker(state))
        spriteTableDirty = TRUE;
    updateTurnHud();

    if (!GameTurn_isFinished(&gameTurn))
        MovementGlow_update();

    if (cursorVisibilityDirty)
    {
        spriteTableDirty = TRUE;
        cursorVisibilityDirty = FALSE;
    }

    /* No reconstruir la SAT si ningún sprite cambió durante este fotograma. */
    if (spriteTableDirty)
    {
        Performance_count(PERF_EVENT_SAT_UPDATE, 1);
        SPR_update();
    }

    /* SGDK estima los FPS contando llamadas a SYS_getFPS(): debe consultarse
       en cada frame, aunque el texto visible se actualice con menos frecuencia.
       Dibujamos explícitamente en BG_B y reservamos tres columnas (32..34),
       antes de la máscara lateral que empieza en la columna 35. */
    {
        const u32 fps = SYS_getFPS();

        fpsOverlayFrameCounter++;
        if (fpsOverlayFrameCounter >= FPS_OVERLAY_REFRESH_FRAMES)
        {
            char fpsText[4];

            fpsOverlayFrameCounter = 0;
            uintToStr(fps, fpsText, 1);
            VDP_drawTextBGFill(BG_B, fpsText, 32, 0, 3);
        }
    }
    Performance_tick();
    SYS_doVBlankProcess();
}

static void updateGameFrame(u16 joy, bool clicked, bool cancelPressed,
                            bool endTurnPressed)
{
    CameraCursorState state;
    const TeamId previousActiveTeam = GameTurn_getActiveTeam(&gameTurn);
    bool movementEndedThisFrame = FALSE;
    bool fogRefreshedThisFrame = FALSE;

    /* Un resultado terminal congela toda entrada y actualización de partida. */
    if (!GameTurn_canIssueCommands(&gameTurn)) return;

    /* El feedback de acciones expira en frames, o antes si cambia la casilla. */
    if (CellHud_tick()) hudContextRefreshPending = TRUE;

    /* La clasificación separa muestras de reposo, cámara, movimiento e IA. */
    Performance_setPhase(classifyPerformancePhase(joy));

    processFogPendingBeforeInput(&state, &fogRefreshedThisFrame);
    fogUpdatePending = FALSE;

    if (UnitController_isMoving(&unitController))
    {
        updateMovingUnitFrame(&state, &movementEndedThisFrame);
    }
    else if ((GameTurn_getActiveTeam(&gameTurn) == TEAM_BLUE) &&
             cameraReturnPending)
    {
        /* El jugador recupera el control cuando termina el retorno de cámara. */
        state = CameraCursor_getState();
    }
    else if (GameTurn_getActiveTeam(&gameTurn) == TEAM_RED)
    {
        updateEnemyTeamFrame();
        state = CameraCursor_getState();
    }
    else
    {
        updatePlayerTeamFrame(joy, clicked, cancelPressed, endTurnPressed,
                              &state);
    }

    synchronizeTeamTransition(previousActiveTeam, &state);
    processMovementCompletionWork(&state, movementEndedThisFrame,
                                   fogRefreshedThisFrame);
    presentGameFrame(&state);
}

/* -------------------------------------------------------------------------
 * Pantalla de resultado y liberación de recursos
 * ------------------------------------------------------------------------- */

/* Pantalla final: conserva el campo de batalla detrás del texto. */
/* Libera lo que pertenece a esta partida antes de volver al menú o reiniciar. */
static void releaseCurrentGame(void)
{
    /* Desvincula primero las cachés que contienen punteros al mapa/sprites. */
    MovementGlow_release();
    UnitView_release(&unitView);

    if (worldMap != NULL)
    {
        MAP_release(worldMap);
        worldMap = NULL;
    }

    SPR_end();
    cellCursor = NULL;
    objectiveMarker = NULL;
    fogTileReservation = NULL;
    lastCursorSpriteX = UNIT_VIEW_POSITION_INVALID;
    lastCursorSpriteY = UNIT_VIEW_POSITION_INVALID;
    lastObjectiveMarkerX = UNIT_VIEW_POSITION_HIDDEN;
    lastObjectiveMarkerY = UNIT_VIEW_POSITION_HIDDEN;
    lastMapScrollX = 0;
    lastMapScrollY = 0;
    mapScrollInitialized = FALSE;
    movementRangeCache.initialized = FALSE;
    turnHudCache.initialized = FALSE;
    hudContextRefreshPending = FALSE;
    memset(actionTargetMask, 0, sizeof(actionTargetMask));

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    SYS_doVBlankProcess();
}

int main(bool hardReset)
{
    u8 rememberedScenarioIndex = ACTIVE_MAP_INDEX;

    (void)hardReset;

#if CELL_EVENTS_SELF_TEST
    KLog(CellEvents_runSelfTest() ?
         "CELL EVENTS SELF-TEST: PASS" : "CELL EVENTS SELF-TEST: FAIL");
    while (TRUE) SYS_doVBlankProcess();
#endif

    /* Un ciclo de juego completo permite volver al selector sin reiniciar ROM. */
    while (TRUE)
    {
        /* Volver al selector conserva la última misión elegida, no el estado
           de la partida. Replay sigue reiniciando directamente ese escenario. */
        rememberedScenarioIndex = ScenarioMenu_showFrom(rememberedScenarioIndex);
        activeScenario = Scenario_get(rememberedScenarioIndex);
        if (activeScenario == NULL)
            fatalError("ERROR: el selector devolvio un escenario invalido");

        while (TRUE)
        {
            CameraCursorState state;
            bool actionWasPressed = FALSE;
            bool cancelWasPressed = FALSE;
            bool endTurnWasPressed = FALSE;
            MissionResultAction resultAction;

            if (!initGameData())
                fatalError("ERROR: no se pudieron inicializar los datos del mapa");

            initVideo();
            state = CameraCursor_getState();
            updateCellHud(&state, FALSE);

            while (!GameTurn_isFinished(&gameTurn))
            {
                const u16 joy = JOY_readJoypad(JOY_1);
                const bool actionPressed = ((joy & BUTTON_A) != 0) &&
                                           !actionWasPressed;
                const bool cancelPressed = ((joy & BUTTON_B) != 0) &&
                                          !cancelWasPressed;
                const bool endTurnPressed = ((joy & BUTTON_START) != 0) &&
                                            !endTurnWasPressed;

                actionWasPressed = (joy & BUTTON_A) != 0;
                cancelWasPressed = (joy & BUTTON_B) != 0;
                endTurnWasPressed = (joy & BUTTON_START) != 0;
                updateGameFrame(joy, actionPressed, cancelPressed,
                                endTurnPressed);
            }

            resultAction = GameScreens_showMissionResult(&gameTurn, activeScenario,
                                                          fogTileIndex);
            releaseCurrentGame();
            if (resultAction == MISSION_RESULT_RETURN_TO_MENU) break;
            /* REPLAY: se reinicializa el mismo escenario con todos los datos limpios. */
        }
    }
}
