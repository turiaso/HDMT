#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "unit_view.h"
#include "game_config.h"
#include "resources.h"

#define TEST_SPRITE_CAPACITY UNIT_VIEW_MAX_UNITS

struct Sprite
{
    const SpriteDefinition *definition;
    s16 x;
    s16 y;
    SpriteVisibility visibility;
    bool released;
    s16 depth;
    u16 attributes;
};

static struct Sprite createdSprites[TEST_SPRITE_CAPACITY];
static u8 createdCount;
static u16 visibilityWrites;
static u16 positionWrites;
static u8 allocationCalls;
static u16 unitReadCount;
static s32 fakeRenderOffsetX;
static s32 fakeRenderOffsetY;
static s16 failAllocationAt;
static u8 releasedCount;
static Unit fakeUnits[UNIT_VIEW_MAX_UNITS];
static UnitController fakeController;
static Palette testPalette = { NULL };
static TileSet unitTileSets[UNIT_TYPE_COUNT] =
{
    { NULL, UNIT_SPRITE_TILE_COUNT }, { NULL, UNIT_SPRITE_TILE_COUNT },
    { NULL, UNIT_SPRITE_TILE_COUNT }, { NULL, UNIT_SPRITE_TILE_COUNT }
};
static AnimationFrame unitFrames[UNIT_TYPE_COUNT];
static AnimationFrame *unitFramePointers[UNIT_TYPE_COUNT];
static Animation unitAnimations[UNIT_TYPE_COUNT];
static Animation *unitAnimationPointers[UNIT_TYPE_COUNT];
static u8 tileLoadCount;
static u16 tileLoadIndices[UNIT_TYPE_COUNT];
static u16 spriteFlagsAtCreate[TEST_SPRITE_CAPACITY];

void PAL_setPalette(u16 palette, const u16 *data, u16 transfer)
{ (void)palette; (void)data; (void)transfer; }

const SpriteDefinition cell_cursor =
    { .w = 24, .h = 24, .palette = &testPalette,
      .maxNumTile = 9, .maxNumSprite = 1 };
const SpriteDefinition unit_soldier =
    { .w = 16, .h = 16, .palette = &testPalette, .numAnimation = 1,
      .animations = &unitAnimationPointers[0], .maxNumTile = 4,
      .maxNumSprite = 1 };
const SpriteDefinition unit_scout =
    { .w = 16, .h = 16, .palette = &testPalette, .numAnimation = 1,
      .animations = &unitAnimationPointers[1], .maxNumTile = 4,
      .maxNumSprite = 1 };
const SpriteDefinition unit_engineer =
    { .w = 16, .h = 16, .palette = &testPalette, .numAnimation = 1,
      .animations = &unitAnimationPointers[2], .maxNumTile = 4,
      .maxNumSprite = 1 };
const SpriteDefinition unit_tank =
    { .w = 16, .h = 16, .palette = &testPalette, .numAnimation = 1,
      .animations = &unitAnimationPointers[3], .maxNumTile = 4,
      .maxNumSprite = 1 };

void VDP_loadTileSet(const TileSet *tiles, u16 index, u16 transfer)
{
    assert(tiles != NULL);
    assert(tiles->numTile == UNIT_SPRITE_TILE_COUNT);
    assert(transfer == DMA_QUEUE);
    assert(tileLoadCount < UNIT_TYPE_COUNT);
    tileLoadIndices[tileLoadCount++] = index;
}

Sprite *SPR_addSpriteEx(const SpriteDefinition *def, s16 x, s16 y,
                        u16 attr, u16 flags)
{
    struct Sprite *sprite;
    if ((failAllocationAt >= 0) && (allocationCalls == (u8)failAllocationAt))
    {
        allocationCalls++;
        return NULL;
    }
    allocationCalls++;
    assert(createdCount < TEST_SPRITE_CAPACITY);
    sprite = &createdSprites[createdCount++];
    memset(sprite, 0, sizeof(*sprite));
    sprite->definition = def;
    sprite->attributes = attr;
    spriteFlagsAtCreate[createdCount - 1] = flags;
    sprite->x = x;
    sprite->y = y;
    sprite->visibility = HIDDEN;
    return sprite;
}

void SPR_setPosition(Sprite *sprite, s16 x, s16 y)
{
    positionWrites++;
    sprite->x = x;
    sprite->y = y;
}

void SPR_setDepth(Sprite *sprite, s16 depth) { sprite->depth = depth; }

void SPR_setVisibility(Sprite *sprite, SpriteVisibility visibility)
{
    visibilityWrites++;
    sprite->visibility = visibility;
}

SpriteVisibility SPR_getVisibility(Sprite *sprite) { return sprite->visibility; }

void SPR_releaseSprite(Sprite *sprite)
{
    if (sprite == NULL) return;
    sprite->released = TRUE;
    sprite->visibility = HIDDEN;
    releasedCount++;
}

u8 UnitController_getCount(const UnitController *controller)
{ return (controller != NULL) ? controller->count : 0; }

const Unit *UnitController_getUnit(const UnitController *controller, u8 index)
{
    unitReadCount++;
    if ((controller == NULL) || (index >= controller->count)) return NULL;
    return &controller->units[index];
}

void UnitController_getRenderPosition(const UnitController *controller,
                                      u8 index, s32 *pixelX, s32 *pixelY)
{
    const Unit *unit = UnitController_getUnit(controller, index);
    assert(unit != NULL);
    *pixelX = (s32)unit->x * CELL_PIXELS;
    *pixelY = (s32)unit->y * CELL_PIXELS;
    if (controller->moving && (controller->movingIndex == (s16)index))
    {
        *pixelX += fakeRenderOffsetX;
        *pixelY += fakeRenderOffsetY;
    }
}

static void resetFixture(UnitView *view, Sprite **storage, u8 capacity,
                         u8 unitCount)
{
    u8 i;

    memset(view, 0, sizeof(*view));
    memset(createdSprites, 0, sizeof(createdSprites));
    memset(storage, 0, capacity * sizeof(*storage));
    memset(fakeUnits, 0, sizeof(fakeUnits));
    createdCount = 0;
    visibilityWrites = 0;
    positionWrites = 0;
    allocationCalls = 0;
    unitReadCount = 0;
    fakeRenderOffsetX = 0;
    fakeRenderOffsetY = 0;
    failAllocationAt = -1;
    releasedCount = 0;
    tileLoadCount = 0;
    memset(tileLoadIndices, 0, sizeof(tileLoadIndices));
    memset(spriteFlagsAtCreate, 0, sizeof(spriteFlagsAtCreate));
    for (i = 0; i < UNIT_TYPE_COUNT; i++)
    {
        unitFrames[i].tileset = &unitTileSets[i];
        unitFramePointers[i] = &unitFrames[i];
        unitAnimations[i].numFrame = 1;
        unitAnimations[i].loop = 0;
        unitAnimations[i].frames = &unitFramePointers[i];
        unitAnimationPointers[i] = &unitAnimations[i];
    }

    for (i = 0; i < unitCount; i++)
    {
        fakeUnits[i].id = i;
        fakeUnits[i].x = (u16)i;
        fakeUnits[i].y = 1;
        fakeUnits[i].maxMovementPoints = 6;
        fakeUnits[i].movementPoints = 6;
        fakeUnits[i].type = (UnitType)(i % UNIT_TYPE_COUNT);
        fakeUnits[i].team = (i & 1) ? TEAM_RED : TEAM_BLUE;
        fakeUnits[i].alive = TRUE;
        fakeUnits[i].health = fakeUnits[i].maxHealth = 10;
    }

    memset(&fakeController, 0, sizeof(fakeController));
    fakeController.units = fakeUnits;
    fakeController.count = unitCount;
    fakeController.selectedIndex = -1;
    fakeController.movingIndex = -1;
}

static void assertActiveSpriteIndexConsistency(const UnitView *view);

static void testViewContainsUnitsButNoPathMarkers(void)
{
    UnitView view;
    Sprite *units[UNIT_TYPE_COUNT];
    CameraCursorState camera = { 0, 0, 0, 0 };
    u8 i;

    resetFixture(&view, units, UNIT_TYPE_COUNT, UNIT_TYPE_COUNT);
    CameraCursor_init(20, 20);
    assert(UnitView_init(&view, &fakeController, units, UNIT_TYPE_COUNT));
    assert(tileLoadCount == UNIT_TYPE_COUNT);
    assert(tileLoadIndices[0] == UNIT_VRAM_TILE_INDEX + 0 * UNIT_SPRITE_TILE_COUNT);
    assert(tileLoadIndices[1] == UNIT_VRAM_TILE_INDEX + 1 * UNIT_SPRITE_TILE_COUNT);
    assert(tileLoadIndices[2] == UNIT_VRAM_TILE_INDEX + 2 * UNIT_SPRITE_TILE_COUNT);
    assert(tileLoadIndices[3] == UNIT_VRAM_TILE_INDEX + 3 * UNIT_SPRITE_TILE_COUNT);
    for (i = 0; i < UNIT_TYPE_COUNT; i++)
        assert((spriteFlagsAtCreate[i] & SPR_FLAG_AUTO_TILE_UPLOAD) == 0);

    /* La vista solo crea sprites de unidades; no pinta una ruta con marcadores. */
    assert(createdCount == UNIT_TYPE_COUNT);
    for (i = 0; i < UNIT_TYPE_COUNT; i++)
    {
        assert(createdSprites[i].definition != &cell_cursor);
        assert(createdSprites[i].definition == (i == 0 ? &unit_soldier :
            i == 1 ? &unit_scout : i == 2 ? &unit_engineer : &unit_tank));
        assert(createdSprites[i].depth == SPR_MIN_DEPTH + 1);
    }

    UnitView_update(&view, &fakeController, &camera);
    assert(units[0]->x == 4 && units[0]->y == CELL_PIXELS + 4);
    assert(units[1]->x == CELL_PIXELS + 4 && units[1]->y == CELL_PIXELS + 4);
    {
        const u16 writes = visibilityWrites;
        const u16 positionWritesBefore = positionWrites;
        UnitView_update(&view, &fakeController, &camera);
        assert(visibilityWrites == writes);
        assert(positionWrites == positionWritesBefore);
    }

    UnitView_release(&view);
    assert(releasedCount == UNIT_TYPE_COUNT);
    UnitView_release(&view);
    assert(releasedCount == UNIT_TYPE_COUNT);
    assert(view.sprites == NULL && view.capacity == 0);
    puts("PASS: la vista centra sprites 16x16 sin dibujar la ruta y libera recursos");
}

static void testHardwareSpriteBudgetWithMoreLogicalUnits(void)
{
    UnitView view;
    Sprite *units[120];
    u8 i;
    u8 visibleSprites = 0;
    u8 blueSprites = 0;
    u8 redSprites = 0;

    resetFixture(&view, units, 120, 120);
    /* Hay 120 unidades lógicas, 100 dentro del viewport de esta prueba, pero
       solo 77 entradas SAT reservadas a unidades (80 menos tres sprites fijos). */
    for (i = 0; i < 120; i++)
    {
        if (i < 100u)
        {
            fakeUnits[i].x = (u16)(i % 13u);
            fakeUnits[i].y = (u16)(i / 13u);
        }
        else
        {
            fakeUnits[i].x = (u16)(30u + (i % 10u));
            fakeUnits[i].y = (u16)(30u + (i / 10u));
        }
        fakeUnits[i].team = (i & 1u) ? TEAM_RED : TEAM_BLUE;
    }

    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 120));
    assert(view.capacity == 120);
    assert(view.activeSpriteCount == MAX_UNIT_SPRITES);
    assertActiveSpriteIndexConsistency(&view);
    assert(createdCount >= MAX_UNIT_SPRITES && createdCount <= 120);
    assert(view.activeSpriteCount + FIXED_GAME_SPRITE_COUNT <=
           MD_VDP_SPRITE_LIMIT);

    for (i = 0; i < 100; i++)
    {
        if (units[i] != NULL)
        {
            visibleSprites++;
            if (fakeUnits[i].team == TEAM_BLUE) blueSprites++;
            else redSprites++;
        }
    }
    assert(visibleSprites == MAX_UNIT_SPRITES);
    /* La prioridad de equipo hace que las 50 azules visibles no sean
       expulsadas por las unidades rojas cuando la SAT se llena. */
    assert(blueSprites == 50 && redSprites == 27);

    /* Un píxel de scroll sin cruzar un umbral recorre la lista de sprites
       activos, no las 120 unidades lógicas ni consulta el controlador. */
    {
        CameraCursorState camera = { 1, 0, 0, 0 };
        unitReadCount = 0;
        positionWrites = 0;
        assert(UnitView_update(&view, &fakeController, &camera));
        assert(unitReadCount == 0);
        assert(positionWrites <= MAX_UNIT_SPRITES);
        assertActiveSpriteIndexConsistency(&view);
    }

    /* Al cambiar el equipo observador, también cambia la prioridad visual. */
    UnitView_setFogContext(&view, NULL, TEAM_RED, FALSE, FALSE);
    (void)UnitView_update(&view, &fakeController,
                          &(CameraCursorState){ 0, 0, 0, 0 });
    blueSprites = 0;
    redSprites = 0;
    for (i = 0; i < 120; i++)
    {
        if (units[i] == NULL) continue;
        if (fakeUnits[i].team == TEAM_BLUE) blueSprites++;
        else redSprites++;
    }
    assert(blueSprites == 27 && redSprites == 50);
    assertActiveSpriteIndexConsistency(&view);

    UnitView_release(&view);
    assert(view.activeSpriteCount == 0 && view.activeSpriteIndexCount == 0);
    assert(releasedCount == createdCount);
    puts("PASS: 120 unidades lógicas, SAT limitada a 77 sprites y prioridad de equipo reversible");
}

static void testTeamPalettesStayPerUnit(void)
{
    UnitView view;
    Sprite *units[2];

    resetFixture(&view, units, 2, 2);
    fakeUnits[0].team = TEAM_BLUE;
    fakeUnits[1].team = TEAM_RED;
    CameraCursor_init(20, 20);
    assert(UnitView_init(&view, &fakeController, units, 2));
    assert(((createdSprites[0].attributes >> 13) & 3) == PAL2);
    assert(((createdSprites[1].attributes >> 13) & 3) == PAL3);
    UnitView_release(&view);
    assert(releasedCount == 2);
    UnitView_release(&view);
    assert(releasedCount == 2);
    puts("PASS: cada unidad conserva la paleta de su equipo");
}

static void testPartialInitializationRollsBack(void)
{
    UnitView view;
    Sprite *units[3];

    resetFixture(&view, units, 3, 3);
    CameraCursor_init(20, 20);
    failAllocationAt = 2;
    assert(!UnitView_init(&view, &fakeController, units, 3));
    assert(createdCount == 2);
    assert(releasedCount == 2);
    assert(units[0] == NULL && units[1] == NULL && units[2] == NULL);
    assert(view.sprites == NULL && view.capacity == 0);
    puts("PASS: una inicializacion parcial libera los sprites reservados");
}


static u8 countAllocatedUnitSprites(const Sprite **sprites, u8 count)
{
    u8 i;
    u8 allocated = 0;
    for (i = 0; i < count; i++)
        if (sprites[i] != NULL) allocated++;
    return allocated;
}

static void assertActiveSpriteIndexConsistency(const UnitView *view)
{
    bool seen[UNIT_VIEW_MAX_UNITS];
    u8 i;

    memset(seen, 0, sizeof(seen));
    assert(view->activeSpriteIndexCount == view->activeSpriteCount);
    for (i = 0; i < view->activeSpriteIndexCount; i++)
    {
        const u8 index = view->activeSpriteIndices[i];
        assert(index < view->capacity);
        assert(view->sprites[index] != NULL);
        assert(!seen[index]);
        seen[index] = TRUE;
    }
    for (i = 0; i < view->capacity; i++)
        assert(seen[i] == (view->sprites[i] != NULL));
}

static void testSpriteReentersWhenCameraReturnsAcrossThresholdEndpoint(void)
{
    UnitView view;
    Sprite *units[1];
    CameraCursorState camera = { 0, 0, 0, 0 };

    resetFixture(&view, units, 1, 1);
    fakeUnits[0].x = 0;
    fakeUnits[0].y = 1;
    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 1));
    assert(units[0] != NULL);

    /* El sprite se conserva una celda fuera del viewport para anticipar
       su entrada/salida; al superar ese margen se libera sin barrer unidades. */
    camera.cameraX = 43;
    assert(UnitView_update(&view, &fakeController, &camera));
    unitReadCount = 0;
    camera.cameraX = 44;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[0] == NULL);
    assert(unitReadCount == 0);
    assertActiveSpriteIndexConsistency(&view);

    /* Al volver en sentido contrario, el sprite se recupera antes de alcanzar
       el viewport real, dentro del margen de precarga de una celda. */
    camera.cameraX = 43;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[0] != NULL);
    assertActiveSpriteIndexConsistency(&view);

    UnitView_release(&view);
    assert(view.activeSpriteCount == 0 && view.activeSpriteIndexCount == 0);
    puts("PASS: sprites precargados una celda antes de entrar y retenidos hasta salir del margen");
}


static void testPartialViewportSpritesAreVisibleAndPrefetchedAhead(void)
{
    UnitView view;
    Sprite *units[2];
    CameraCursorState camera = { 0, 0, 0, 0 };

    resetFixture(&view, units, 2, 2);
    /* La unidad azul intersecta la pantalla física, aunque cruce el borde
       lógico de celdas completas (312 px). Debe seguir visible. */
    fakeUnits[0].x = 12;
    fakeUnits[0].y = 3;
    /* La roja está fuera de la pantalla física, pero dentro del margen de
       precarga: su sprite existe y sigue oculto hasta entrar. */
    fakeUnits[1].x = 14;
    fakeUnits[1].y = 3;

    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 2));
    assert(units[0] != NULL);
    assert(units[1] != NULL);
    assert(units[0]->visibility == VISIBLE);
    assert(units[1]->x == (s16)(14 * CELL_PIXELS + 4));
    assert(units[1]->visibility == HIDDEN);

    fakeController.moving = TRUE;
    fakeController.movingIndex = 0;
    fakeRenderOffsetX = 8;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[0]->x == 300);
    assert(units[0]->visibility == VISIBLE);

    /* Al avanzar la cámara, el sprite precargado entra en la pantalla física
       y se revela sin recrearse. */
    {
        const u8 oldCreatedCount = createdCount;
        fakeController.moving = FALSE;
        fakeController.movingIndex = -1;
        fakeRenderOffsetX = 0;
        camera.cameraX = 24;
        assert(UnitView_update(&view, &fakeController, &camera));
        assert(createdCount == oldCreatedCount);
        assert(units[1] != NULL);
        assert(units[1]->x == (s16)(14 * CELL_PIXELS + 4 - 24));
        assert(units[1]->visibility == VISIBLE);
    }

    UnitView_release(&view);
    puts("PASS: borde físico visible; precarga oculta y revelado al entrar sin recrear sprite");
}

static void testOffscreenUnitsAreNotRegisteredAndIdleUpdateIsNoop(void)
{
    UnitView view;
    Sprite *units[8];
    CameraCursorState camera = { 0, 0, 0, 0 };
    u8 i;

    resetFixture(&view, units, 8, 8);
    for (i = 4; i < 8; i++) fakeUnits[i].x = (u16)(16 + i);
    CameraCursor_init(40, 40);

    assert(UnitView_init(&view, &fakeController, units, 8));
    assert(countAllocatedUnitSprites((const Sprite **)units, 8) == 4);
    assertActiveSpriteIndexConsistency(&view);
    assert(createdCount == 4);

    /* Quiet frame: no recálculo de posiciones ni escrituras de sprites. */
    {
        const u16 oldPositions = positionWrites;
        const u8 oldCreates = createdCount;
        unitReadCount = 0;
        assert(!UnitView_update(&view, &fakeController, &camera));
        assert(positionWrites == oldPositions);
        assert(createdCount == oldCreates);
        assert(unitReadCount == 0);
    }

    /* Al desplazar la cámara, solo se conservan unidades del viewport nuevo. */
    camera.cameraX = 20 * CELL_PIXELS;
    unitReadCount = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    /* El salto grande descubre cuatro candidatos, pero los añade a la SAT
       uno por fotograma para no provocar un pico de trabajo. */
    assert(countAllocatedUnitSprites((const Sprite **)units, 8) == 1);
    for (i = 0; i < 3; i++)
        (void)UnitView_update(&view, &fakeController, &camera);
    assert(countAllocatedUnitSprites((const Sprite **)units, 8) == 4);
    assert(createdCount == 8);
    assert(releasedCount == 4);
    assertActiveSpriteIndexConsistency(&view);
    /* Solo consulta al controlador las cuatro unidades que entraron al viewport. */
    assert(unitReadCount <= 4);
    /* Entrar en pantalla no provoca otra carga gráfica: solo se precargan 4 tipos. */
    assert(tileLoadCount == UNIT_TYPE_COUNT);

    /* Un desplazamiento pequeño reposiciona los activos sin barrer unidades ocultas. */
    unitReadCount = 0;
    camera.cameraX++;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(unitReadCount == 0);
    assertActiveSpriteIndexConsistency(&view);

    /* Un estado visual explícito debe invalidar la vista incluso con cámara quieta. */
    fakeUnits[4].alive = FALSE;
    UnitView_invalidate(&view);
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[4] == NULL);
    assert(releasedCount == 5);

    UnitView_release(&view);
    assert(releasedCount == 8);
    puts("PASS: sprites fuera de pantalla liberados; fotograma quieto sin cambios");
}


static void testCameraAdmissionUsesSpatialEdgeIndex(void)
{
    UnitView view;
    Sprite *units[4];
    CameraCursorState camera = { 0, 0, 0, 0 };
    CellData cells[40 * 40];
    MapData map = { .width = 40, .height = 40, .cells = cells,
                    .cellCapacity = 40 * 40 };
    u16 i;

    resetFixture(&view, units, 4, 2);
    fakeUnits[0].team = TEAM_BLUE;
    fakeUnits[0].x = 0;
    fakeUnits[0].y = 1;
    fakeUnits[1].team = TEAM_BLUE;
    fakeUnits[1].x = 15;
    fakeUnits[1].y = 1;
    for (i = 0; i < 40 * 40; i++)
    {
        cells[i].terrain = TERRAIN_PATH;
        cells[i].occupantId = CELL_NO_OCCUPANT;
        cells[i].eventSetId = 0;
        cells[i].visionState = MAP_VISION_VISIBLE;
    }

    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 4));
    UnitView_setFogContext(&view, &map, TEAM_BLUE, TRUE, FALSE);
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(view.spatialIndexValid);
    assert(view.unitColumnHead[15] == 1);
    assert(units[1] == NULL);

    /* La celda se precarga al entrar en el margen extendido, antes de que
       el sprite llegue al viewport real (cámara X=45 aproximadamente). */
    camera.cameraX = 29;
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(units[1] != NULL);
    camera.cameraX = 45;
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(units[1] != NULL);
    assertActiveSpriteIndexConsistency(&view);

    /* Si una unidad cambia de celda, el índice espacial debe migrarla a la
       nueva fila/columna para que el siguiente scroll también la encuentre. */
    fakeController.moving = TRUE;
    fakeController.movingIndex = 1;
    fakeUnits[1].x = 14;
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(view.unitColumnHead[15] == UNIT_VIEW_NO_INDEX);
    assert(view.unitColumnHead[14] == 1);
    fakeController.moving = FALSE;
    fakeController.movingIndex = -1;
    (void)UnitView_update(&view, &fakeController, &camera);

    UnitView_release(&view);
    puts("PASS: la admisión por borde consulta el índice espacial y no barre unidades lógicas");
}

static void testCameraCandidateBitsetKeepsPriorityWithoutQueueShifts(void)
{
    UnitView view;
    Sprite *units[4];
    CameraCursorState camera = { 0, 0, 0, 0 };
    CellData cells[40 * 40];
    MapData map = { .width = 40, .height = 40, .cells = cells,
                    .cellCapacity = 40 * 40 };
    u16 i;

    resetFixture(&view, units, 4, 4);
    /* Los cuatro efectivos entran a la vez por la columna 13. El candidato
       seleccionado debe ganar aunque tenga un ID superior a los rivales. */
    fakeUnits[0].team = TEAM_RED;
    fakeUnits[0].x = 15;
    fakeUnits[0].y = 1;
    fakeUnits[1].team = TEAM_BLUE;
    fakeUnits[1].x = 15;
    fakeUnits[1].y = 2;
    fakeUnits[2].team = TEAM_RED;
    fakeUnits[2].x = 15;
    fakeUnits[2].y = 3;
    fakeUnits[3].team = TEAM_BLUE;
    fakeUnits[3].x = 15;
    fakeUnits[3].y = 4;
    fakeController.selectedIndex = 1;

    for (i = 0; i < 40 * 40; i++)
    {
        cells[i].terrain = TERRAIN_PATH;
        cells[i].occupantId = CELL_NO_OCCUPANT;
        cells[i].eventSetId = 0;
        cells[i].visionState = MAP_VISION_VISIBLE;
    }

    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 4));
    UnitView_setFogContext(&view, &map, TEAM_BLUE, TRUE, FALSE);
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(units[0] == NULL && units[1] == NULL &&
           units[2] == NULL && units[3] == NULL);

    camera.cameraX = 29;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[1] != NULL); /* prioridad 3: seleccionada */
    assert(units[0] == NULL && units[2] == NULL && units[3] == NULL);
    assert(view.pendingSpriteCandidateCount == 3);
    assert(view.pendingSpriteCandidateQueued[0]);
    assert(!view.pendingSpriteCandidateQueued[1]);
    assert(view.pendingSpriteCandidateQueued[2]);
    assert(view.pendingSpriteCandidateQueued[3]);
    assert(view.pendingSpriteCandidatePriority[0] == 1);
    assert(view.pendingSpriteCandidatePriority[2] == 1);
    assert(view.pendingSpriteCandidatePriority[3] == 2);
    assertActiveSpriteIndexConsistency(&view);

    /* La cola de bits conserva los candidatos restantes sin duplicarlos. */
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(view.pendingSpriteCandidateCount == 2);
    assert(units[3] != NULL); /* prioridad 2 precede a los rojos */
    assert(view.pendingSpriteCandidateQueued[0]);
    assert(!view.pendingSpriteCandidateQueued[3]);
    assertActiveSpriteIndexConsistency(&view);

    UnitView_release(&view);
    puts("PASS: el índice de bits conserva prioridad de sprites al entrar por el borde");
}

static void testSharedVisionShowsUnitsVisibleToEitherTeam(void)
{
    UnitView view;
    Sprite *units[2];
    CameraCursorState camera = { 0, 0, 0, 0 };
    CellData cells[20 * 20];
    MapData map = { .width = 20, .height = 20, .cells = cells,
                    .cellCapacity = 20 * 20 };
    u16 i;

    resetFixture(&view, units, 2, 2);
    fakeUnits[0].team = TEAM_BLUE;
    fakeUnits[0].x = 0;
    fakeUnits[0].y = 1;
    fakeUnits[1].team = TEAM_RED;
    fakeUnits[1].x = 1;
    fakeUnits[1].y = 1;
    for (i = 0; i < 20 * 20; i++)
    {
        cells[i].terrain = TERRAIN_PATH;
        cells[i].occupantId = CELL_NO_OCCUPANT;
        cells[i].eventSetId = 0;
        cells[i].visionState = 0;
    }

    /* Azul ve (0,1), Rojo ve (1,1); su unión permite ver ambas unidades. */
    cells[20].visionState =
        (u8)(MAP_VISION_VISIBLE | (MAP_VISION_UNSEEN << 2));
    cells[21].visionState =
        (u8)(MAP_VISION_UNSEEN | (MAP_VISION_VISIBLE << 2));

    CameraCursor_init(20, 20);
    assert(UnitView_init(&view, &fakeController, units, 2));
    UnitView_setFogContext(&view, &map, TEAM_RED, TRUE, FALSE);
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(units[0] == NULL); /* La vista roja normal oculta al azul no visible. */
    assert(units[1] != NULL);

    UnitView_setFogContext(&view, &map, TEAM_RED, TRUE, TRUE);
    (void)UnitView_update(&view, &fakeController, &camera);
    assert(units[0] != NULL); /* Azul lo ve: aparece durante la vista espectadora. */
    assert(units[1] != NULL); /* Rojo lo ve: sigue apareciendo. */

    UnitView_release(&view);
    puts("PASS: la vista compartida muestra unidades visibles por cualquiera de los equipos");
}


static void testMovementEndSnapsSpriteToLogicalCell(void)
{
    UnitView view;
    Sprite *units[8];
    CameraCursorState camera = { 0, 0, 0, 0 };
    Sprite *movingSprite;
    const s16 baseX = (s16)(3 * CELL_PIXELS +
        (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2);
    const s16 nextX = (s16)(4 * CELL_PIXELS +
        (CELL_PIXELS - UNIT_SPRITE_SIZE_PIXELS) / 2);

    resetFixture(&view, units, 8, 8);
    fakeUnits[0].x = 3;
    fakeUnits[0].y = 1;
    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 8));
    movingSprite = units[0];
    assert(movingSprite != NULL);
    assert(movingSprite->x == baseX);

    /* Caso 1: un tramo empieza a interpolar, pero se bloquea al completarse.
       El controlador conserva la casilla lógica; la vista debe hacer snap. */
    fakeController.moving = TRUE;
    fakeController.movingIndex = 0;
    fakeRenderOffsetX = CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(movingSprite->x == baseX + CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS);

    fakeController.moving = FALSE;
    fakeController.movingIndex = -1;
    fakeRenderOffsetX = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(movingSprite->x == baseX);
    assert(view.lastScreenX[0] == baseX);

    /* Caso 2: llegada válida. La lógica cambia de casilla en el último frame;
       el sprite debe acabar centrado en la nueva casilla, no un frame atrás. */
    fakeController.moving = TRUE;
    fakeController.movingIndex = 0;
    fakeRenderOffsetX = CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(movingSprite->x == baseX + CELL_PIXELS - UNIT_MOVE_SPEED_PIXELS);

    fakeUnits[0].x = 4;
    fakeController.moving = FALSE;
    fakeController.movingIndex = -1;
    fakeRenderOffsetX = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(movingSprite->x == nextX);
    assert(view.lastScreenX[0] == nextX);

    UnitView_release(&view);
    puts("PASS: snap final al completar o abortar un movimiento");
}

static void testIncrementalFogRefreshTouchesOnlyChangedUnitVisibility(void)
{
    UnitView view;
    Sprite *units[4];
    CameraCursorState camera = { 0, 0, 0, 0 };
    CellData cells[20 * 20];
    MapData map = { .width = 20, .height = 20, .cells = cells,
                    .cellCapacity = 20 * 20 };
    const u16 changedCell[1] = { (u16)(1 * 20 + 3) };
    u16 i;

    resetFixture(&view, units, 4, 2);
    fakeUnits[0].team = TEAM_BLUE;
    fakeUnits[0].x = 0;
    fakeUnits[0].y = 1;
    fakeUnits[1].team = TEAM_RED;
    fakeUnits[1].x = 3;
    fakeUnits[1].y = 1;
    for (i = 0; i < 20 * 20; i++)
    {
        cells[i].terrain = TERRAIN_PATH;
        cells[i].occupantId = CELL_NO_OCCUPANT;
        cells[i].eventSetId = 0;
        cells[i].visionState = MAP_VISION_UNSEEN;
    }

    CameraCursor_init(20, 20);
    assert(UnitView_init(&view, &fakeController, units, 4));
    UnitView_setFogContext(&view, &map, TEAM_BLUE, TRUE, FALSE);
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[1] == NULL); /* Rojo sigue oculto en una celda desconocida. */

    /* Solo cambia la visión de la casilla donde está esa unidad enemiga. */
    cells[changedCell[0]].visionState = MAP_VISION_VISIBLE;
    UnitView_setFogContext(&view, &map, TEAM_BLUE, TRUE, FALSE);
    UnitView_setFogDirtyCells(&view, changedCell, 1, map.width);
    assert(view.fogDirtyCellsPending && !view.fogRefreshPending);
    unitReadCount = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(unitReadCount == 2);
    assert(units[1] != NULL);
    assert(units[1]->x == (s16)(3 * CELL_PIXELS + 4));

    /* Al perderse la visión, el mismo refresco selectivo oculta la unidad. */
    cells[changedCell[0]].visionState = MAP_VISION_EXPLORED;
    UnitView_setFogContext(&view, &map, TEAM_BLUE, TRUE, FALSE);
    UnitView_setFogDirtyCells(&view, changedCell, 1, map.width);
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(units[1] == NULL);

    UnitView_release(&view);
    puts("PASS: la niebla incremental solo cambia el sprite situado en una celda afectada");
}

static void testMovingUnitUsesFastPath(void)
{
    UnitView view;
    Sprite *units[8];
    CameraCursorState camera = { 0, 0, 0, 0 };
    Sprite *movingSprite;

    resetFixture(&view, units, 8, 8);
    CameraCursor_init(40, 40);
    assert(UnitView_init(&view, &fakeController, units, 8));

    fakeController.moving = TRUE;
    fakeController.movingIndex = 0;
    movingSprite = units[0];
    fakeRenderOffsetX = 8;
    unitReadCount = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    /* Sin cámara móvil, actualiza solamente el sprite que se está animando. */
    assert(unitReadCount <= 2);
    assert(movingSprite->x == 12);

    /* En el seguimiento de cámara, los demás sprites salen de caché; la unidad
       seguida conserva su posición subcelda en lugar de saltar a su celda base. */
    camera.cameraX = 1;
    fakeRenderOffsetX = 12;
    unitReadCount = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(unitReadCount <= 3);
    assert(movingSprite->x == 15);

    fakeController.moving = FALSE;
    fakeController.movingIndex = -1;
    fakeRenderOffsetX = 0;
    assert(UnitView_update(&view, &fakeController, &camera));
    assert(movingSprite->x == 3);
    assert(view.lastScreenX[0] == 3);
    UnitView_release(&view);
    puts("PASS: movimiento y cámara usan caché; snap final a celda exacta");
}

int main(void)
{
    testViewContainsUnitsButNoPathMarkers();
    testHardwareSpriteBudgetWithMoreLogicalUnits();
    testTeamPalettesStayPerUnit();
    testPartialInitializationRollsBack();
    testSpriteReentersWhenCameraReturnsAcrossThresholdEndpoint();
    testPartialViewportSpritesAreVisibleAndPrefetchedAhead();
    testOffscreenUnitsAreNotRegisteredAndIdleUpdateIsNoop();
    testCameraAdmissionUsesSpatialEdgeIndex();
    testCameraCandidateBitsetKeepsPriorityWithoutQueueShifts();
    testSharedVisionShowsUnitsVisibleToEitherTeam();
    testIncrementalFogRefreshTouchesOnlyChangedUnitVisibility();
    testMovingUnitUsesFastPath();
    testMovementEndSnapsSpriteToLogicalCell();
    return 0;
}
