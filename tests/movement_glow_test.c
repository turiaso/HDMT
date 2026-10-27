#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "movement_glow.h"
#include "combat.h"
#include "game_config.h"

static Map testMap;
static MapDataPatchCallback *storedCallback;
static u16 writtenPalette[16];
static u16 paletteWriteCount;
static u16 lastPaletteTransfer;
static u32 lastScrollX;
static u32 lastScrollY;
static bool lastScrollForced;
static u16 tileRowWrites;
static u16 tileRowX[32];
static u16 tileRowY[32];
static u16 tileRowWidth[32];
static u16 tileRowData[32][128];
static u16 tileRowTransfer[32];

void PAL_setPalette(u16 palette, const u16 *data, u16 transfer)
{
    u8 i;
    lastPaletteTransfer = transfer;
    if (palette != PAL1) return;
    for (i = 0; i < 16; i++) writtenPalette[i] = data[i];
    paletteWriteCount++;
}

void MAP_setDataPatchCallback(Map *map, MapDataPatchCallback *callback)
{
    assert(map == &testMap);
    storedCallback = callback;
}

void MAP_scrollToEx(Map *map, u32 x, u32 y, bool forceRedraw)
{
    assert(map == &testMap);
    lastScrollX = x;
    lastScrollY = y;
    lastScrollForced = forceRedraw;
}


u16 MAP_getTile(Map *map, u16 x, u16 y)
{
    assert(map == &testMap);
    return (u16)(0x20u + (y * 128u) + x);
}

void VDP_setTileMapDataRow(u16 plane, const u16 *data, u16 row,
                           u16 x, u16 width, u16 transfer)
{
    u8 i;
    const u16 call = tileRowWrites++;
    assert(plane == BG_A);
    assert(width > 0 && width <= 128);
    assert(call < 32);
    tileRowX[call] = x;
    tileRowY[call] = row;
    tileRowWidth[call] = width;
    tileRowTransfer[call] = transfer;
    for (i = 0; i < width; i++) tileRowData[call][i] = data[i];
}

static u16 makeColor(u8 red, u8 green, u8 blue)
{
    return (u16)(((u16)red << 1) | ((u16)green << 5) | ((u16)blue << 9));
}

static u8 colorRed(u16 color) { return (u8)((color >> 1) & 7); }
static u8 colorGreen(u16 color) { return (u8)((color >> 5) & 7); }
static u8 colorBlue(u16 color) { return (u8)((color >> 9) & 7); }

static void assertTilePalette(u16 tile, u16 palette)
{
    assert(((tile & TILE_ATTR_PALETTE_MASK) >> TILE_ATTR_PALETTE_SFT) == palette);
}

static void testReachableTilesArePatchedWithoutChangingTheirTileIndex(void)
{
    u16 tiles[6];
    const u8 mask[6] = { 0, 1, 0, 0, 0, 0 };
    u8 i;

    for (i = 0; i < 6; i++) tiles[i] = (u16)(0x8010 | i | (PAL2 << 13));
    MovementGlow_setReachableMask(mask, 3, 2);
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 6);

    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL0);
    for (i = 3; i < 6; i++) assertTilePalette(tiles[i], PAL1);
    for (i = 0; i < 6; i++) assert((tiles[i] & 0x1FFF) == (u16)(0x10 | i));
    for (i = 0; i < 6; i++) assert((tiles[i] & 0x8000) != 0);
}

static void testColumnUpdatesUseTileRowsAndClearOldGlow(void)
{
    u16 tiles[6];
    const u8 mask[6] = { 0, 1, 0, 0, 0, 0 };
    u8 i;

    for (i = 0; i < 6; i++) tiles[i] = (u16)(0x0010 | (PAL1 << 13));
    MovementGlow_setReachableMask(mask, 3, 2);
    storedCallback(&testMap, tiles, 3, 0, COLUMN_UPDATE, 6);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);
    for (i = 3; i < 6; i++) assertTilePalette(tiles[i], PAL0);

    MovementGlow_setReachableMask(NULL, 3, 2);
    storedCallback(&testMap, tiles, 3, 0, COLUMN_UPDATE, 6);
    for (i = 0; i < 6; i++) assertTilePalette(tiles[i], PAL0);
}

static u16 makeTerrainTile(u16 index);

static void testClearReachableMaskOnlyRefreshesLitCells(void)
{
    CellData cells[6];
    MapData map = { .width = 3, .height = 2, .cells = cells,
                    .cellCapacity = 6 };
    const u8 reachable[6] = { 0, 1, 0, 0, 0, 0 };
    u16 tiles[3];
    u16 i;

    memset(cells, 0, sizeof(cells));
    memset(tileRowX, 0, sizeof(tileRowX));
    memset(tileRowY, 0, sizeof(tileRowY));
    memset(tileRowWidth, 0, sizeof(tileRowWidth));
    tileRowWrites = 0;
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    testMap.baseTile = (u16)((PAL0 << TILE_ATTR_PALETTE_SFT) | 0x0100);

    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, FALSE, FALSE);
    MovementGlow_setReachableMask(reachable, 3, 2);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(50 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);

    /* Quitar el rango actualiza las tres filas de esa celda, no el mapa entero. */
    lastScrollX = 123;
    lastScrollY = 45;
    lastScrollForced = FALSE;
    assert(MovementGlow_clearReachableMask(0, 0));
    assert(tileRowWrites == 3);
    assert(lastScrollX == 123 && lastScrollY == 45 && !lastScrollForced);
    for (i = 0; i < 3; i++)
    {
        assert(tileRowX[i] == 3);
        assert(tileRowY[i] == i);
        assert(tileRowWidth[i] == 3);
        assertTilePalette(tileRowData[i][0], PAL0);
        assertTilePalette(tileRowData[i][1], PAL0);
        assertTilePalette(tileRowData[i][2], PAL0);
    }

    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
    puts("PASS: ocultar el alcance refresca solo las celdas iluminadas, sin scroll forzado");
}

static u16 tileIndex(u16 tile)
{
    return (u16)(tile & 0x07FFu);
}

static u16 makeTerrainTile(u16 index)
{
    return (u16)(0x9000u | index | ((u16)PAL0 << TILE_ATTR_PALETTE_SFT));
}

static void testCombinedMasksPatchOnlyChangedCellsWithoutFullRedraw(void)
{
    CellData cells[6];
    MapData map = { .width = 3, .height = 2, .cells = cells,
                    .cellCapacity = 6 };
    const u8 reachable[6] = { 0, 1, 1, 0, 0, 0 };
    const u8 attackable[6] = { 0, 0, 1, 0, 0, 0 };
    u16 tiles[6];
    u16 i;

    memset(cells, 0, sizeof(cells));
    memset(tileRowX, 0, sizeof(tileRowX));
    memset(tileRowY, 0, sizeof(tileRowY));
    memset(tileRowWidth, 0, sizeof(tileRowWidth));
    tileRowWrites = 0;
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    testMap.baseTile = (u16)((PAL0 << TILE_ATTR_PALETTE_SFT) | 0x0100);

    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, FALSE, FALSE);
    lastScrollForced = FALSE;

    assert(MovementGlow_setMasks(reachable, attackable, 3, 2, 0, 0));
    assert(!lastScrollForced);
    assert(tileRowWrites == 3); /* Un unico tramo horizontal, no el viewport. */

    for (i = 0; i < 6; i++) tiles[i] = makeTerrainTile((u16)(70 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 6);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);
    for (i = 3; i < 6; i++) assertTilePalette(tiles[i], PAL3);

    tileRowWrites = 0;
    assert(MovementGlow_setMasks(NULL, NULL, 3, 2, 0, 0));
    assert(!lastScrollForced);
    assert(tileRowWrites == 3); /* Al deseleccionar solo se restaura ese tramo. */

    for (i = 0; i < 6; i++) tiles[i] = makeTerrainTile((u16)(90 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 6);
    for (i = 0; i < 6; i++) assertTilePalette(tiles[i], PAL0);

    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
    puts("PASS: selección actualiza alcance y ataques con parcheo localizado, sin redraw completo");
}

static void testFogHidesUnknownTerrainAndRestoresKnownTerrain(void)
{
    CellData cell = { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, MAP_VISION_UNSEEN };
    MapData map = { .width = 1, .height = 1, .cells = &cell,
                    .cellCapacity = 1 };
    u16 tiles[3];
    const u8 reachable[1] = { 1 };
    u16 palette[16] = { 0 };
    u8 i;

    assert(MovementGlow_init(&testMap, palette));
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, TRUE, FALSE);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(20 + i));
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == 600);
        assertTilePalette(tiles[i], PAL2);
        assert((tiles[i] & 0x8000u) != 0);
    }

    /* Al revelar una celda, invalida su estilo cacheado antes del próximo scroll. */
    cell.visionState = MAP_VISION_VISIBLE;
    (void)MovementGlow_refreshFogCell(0, 0, 0, 0);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(20 + i));
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == (u16)(20 + i));
        assertTilePalette(tiles[i], PAL0);
    }

    /* En una celda explorada pero no visible, vuelve el terreno memorizado. */
    cell.visionState = MAP_VISION_EXPLORED;
    MovementGlow_setReachableMask(reachable, 1, 1);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(20 + i));
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == (u16)(20 + i));
        assertTilePalette(tiles[i], PAL1);
    }
    MovementGlow_setReachableMask(NULL, 1, 1);

    /* El modo sin niebla no altera el estado lógico (el juego ya no lo usa
       para el turno de IA; ese turno usa la unión de visión de ambos equipos). */
    cell.visionState = MAP_VISION_UNSEEN;
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, FALSE, FALSE);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(20 + i));
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == (u16)(20 + i));
        assertTilePalette(tiles[i], PAL0);
    }
    assert(MapData_getVisionState(&map, TEAM_BLUE, 0, 0) == MAP_VISION_UNSEEN);

    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
}

static void testSharedVisionKeepsFogOutsideBothTeamsVision(void)
{
    CellData cell = { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, 0 };
    CellData unseenCell = { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, 0 };
    CellData cells[2] = { cell, unseenCell };
    MapData map = { .width = 2, .height = 1, .cells = cells,
                    .cellCapacity = 2 };
    u16 tiles[3];
    u16 unseenTiles[3];
    u16 i;

    /* La casilla 0 solo la ve Azul; la casilla 1 no la conoce ningún equipo. */
    cells[0].visionState = (u8)(MAP_VISION_VISIBLE | (MAP_VISION_UNSEEN << 2));
    cells[1].visionState = (u8)(MAP_VISION_UNSEEN | (MAP_VISION_UNSEEN << 2));
    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    MovementGlow_setFogContext(&map, TEAM_RED, 600, TRUE, TRUE);

    for (i = 0; i < 3; i++)
    {
        tiles[i] = makeTerrainTile((u16)(30 + i));
        unseenTiles[i] = makeTerrainTile((u16)(40 + i));
    }
    storedCallback(&testMap, tiles, 0, 0, ROW_UPDATE, 3);
    storedCallback(&testMap, unseenTiles, 3, 0, ROW_UPDATE, 3);

    /* La visión azul revela la primera celda en la vista conjunta. */
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == (u16)(30 + i));
        assertTilePalette(tiles[i], PAL0);
    }
    /* La segunda sigue usando el tile opaco porque ambos equipos la desconocen. */
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(unseenTiles[i]) == 600);
        assertTilePalette(unseenTiles[i], PAL2);
    }

    MovementGlow_setFogContext(NULL, TEAM_RED, 0, FALSE, FALSE);
}

static void testPaletteClonePulseAndForcedRedraw(void)
{
    u16 source[16];
    const u8 reachable[1] = { 1 };
    u16 i;

    for (i = 0; i < 16; i++) source[i] = makeColor((u8)(i % 8),
        (u8)((i + 2) % 8), (u8)((i + 4) % 8));
    source[0] = 0x0000;
    source[15] = makeColor(7, 7, 7);
    storedCallback = NULL;
    paletteWriteCount = 0;

    assert(MovementGlow_init(&testMap, source));
    assert(storedCallback != NULL);
    assert(paletteWriteCount == 1);
    assert(lastPaletteTransfer == DMA_QUEUE_COPY);
    for (i = 0; i < 16; i++) assert(writtenPalette[i] == source[i]);

    MovementGlow_redraw(96, 48);
    assert(lastScrollX == 96 && lastScrollY == 48 && lastScrollForced);
    MovementGlow_setReachableMask(reachable, 1, 1);

    MovementGlow_update();
    MovementGlow_update();
    MovementGlow_update();
    assert(paletteWriteCount == 1); /* La animacion avanza cada cuatro frames. */
    MovementGlow_update();
    assert(paletteWriteCount == 2);
    assert(lastPaletteTransfer == DMA_QUEUE_COPY);
    assert(writtenPalette[0] == source[0]);
    assert(colorRed(writtenPalette[1]) ==
           ((colorRed(source[1]) < 7) ? colorRed(source[1]) + 1 : 7));
    assert(colorGreen(writtenPalette[1]) ==
           ((colorGreen(source[1]) < 7) ? colorGreen(source[1]) + 1 : 7));
    assert(colorBlue(writtenPalette[1]) ==
           ((colorBlue(source[1]) < 7) ? colorBlue(source[1]) + 1 : 7));
    assert(writtenPalette[15] == source[15]); /* Sin desbordamiento de color. */

    MovementGlow_setReachableMask(NULL, 1, 1);
    for (i = 0; i < 8; i++) MovementGlow_update();
    assert(paletteWriteCount == 2); /* Sin CRAM DMA cuando no hay glow. */
}

static void testLocalizedFogRefreshTouchesOnlyOneCell(void)
{
    CellData cells[1600];
    MapData map = { .width = 40, .height = 40, .cells = cells,
                    .cellCapacity = 1600 };
    u16 row;
    u16 column;

    memset(cells, 0, sizeof(cells));
    memset(tileRowX, 0, sizeof(tileRowX));
    memset(tileRowY, 0, sizeof(tileRowY));
    tileRowWrites = 0;
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    testMap.baseTile = (u16)((PAL0 << TILE_ATTR_PALETTE_SFT) | 0x0100);
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, TRUE, FALSE);

    assert(MovementGlow_refreshFogCell(1, 1, 0, 0));
    assert(tileRowWrites == 3);
    for (row = 0; row < 3; row++)
    {
        assert(tileRowX[row] == 3);
        assert(tileRowY[row] == (u16)(3 + row));
        assert(tileRowTransfer[row] == DMA_QUEUE_COPY);
        for (column = 0; column < 3; column++)
        {
            assert((tileRowData[row][column] & 0x07FFu) == 600);
            assert(((tileRowData[row][column] & TILE_ATTR_PALETTE_MASK) >>
                    TILE_ATTR_PALETTE_SFT) == PAL2);
        }
    }

    /* Las celdas fuera de cámara no escriben sobre otras entradas del plano. */
    assert(!MovementGlow_refreshFogCell(20, 20, 0, 0));
    assert(tileRowWrites == 3);
    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
}


static void testContiguousFogCellsAreBatchedIntoTileRows(void)
{
    CellData cells[1600];
    MapData map = { .width = 40, .height = 40, .cells = cells,
                    .cellCapacity = 1600 };
    u16 row;
    u16 column;

    memset(cells, 0, sizeof(cells));
    memset(tileRowX, 0, sizeof(tileRowX));
    memset(tileRowY, 0, sizeof(tileRowY));
    memset(tileRowWidth, 0, sizeof(tileRowWidth));
    tileRowWrites = 0;
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    testMap.baseTile = (u16)((PAL0 << TILE_ATTR_PALETTE_SFT) | 0x0100);
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, TRUE, FALSE);

    /* Dos celdas contiguas escriben seis tiles por fila y solo tres DMAs. */
    assert(MovementGlow_refreshFogCellRun(1, 2, 1, 0, 0));
    assert(tileRowWrites == 3);
    for (row = 0; row < 3; row++)
    {
        assert(tileRowX[row] == 3);
        assert(tileRowY[row] == (u16)(3 + row));
        assert(tileRowWidth[row] == 6);
        assert(tileRowTransfer[row] == DMA_QUEUE_COPY);
        for (column = 0; column < 6; column++)
        {
            assert((tileRowData[row][column] & 0x07FFu) == 600);
            assert(((tileRowData[row][column] & TILE_ATTR_PALETTE_MASK) >>
                    TILE_ATTR_PALETTE_SFT) == PAL2);
        }
    }

    /* Si el rango cruza el límite del plano circular, divide cada fila en dos. */
    tileRowWrites = 0;
    assert(MovementGlow_refreshFogCellRun(21, 22, 1, 504, 0));
    assert(tileRowWrites == 6);
    assert(tileRowX[0] == 63 && tileRowWidth[0] == 1);
    assert(tileRowX[1] == 0 && tileRowWidth[1] == 5);
    assert(tileRowX[2] == 63 && tileRowWidth[2] == 1);
    assert(tileRowX[3] == 0 && tileRowWidth[3] == 5);
    assert(tileRowX[4] == 63 && tileRowWidth[4] == 1);
    assert(tileRowX[5] == 0 && tileRowWidth[5] == 5);

    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
}


static void testAttackableTargetHighlightUsesRedPaletteAndRestoresStyle(void)
{
    CellData cells[3] = {
        { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, MAP_VISION_VISIBLE },
        { TERRAIN_PATH, CELL_NO_OCCUPANT, 0, MAP_VISION_VISIBLE },
        { TERRAIN_PATH, 1, 0, MAP_VISION_VISIBLE }
    };
    MapData map = { .width = 3, .height = 1, .cells = cells,
                    .cellCapacity = 3 };
    const u8 reachable[3] = { 0, 1, 0 };
    const u8 targetable[3] = { 0, 0, COMBAT_TARGET_ATTACKABLE };
    const u8 repairableTarget[3] = { 0, 0, COMBAT_TARGET_REPAIRABLE };
    const u8 overlappingTarget[3] = { 0, COMBAT_TARGET_ATTACKABLE, 0 };
    u16 tiles[3];
    u16 i;

    memset(tileRowX, 0, sizeof(tileRowX));
    memset(tileRowY, 0, sizeof(tileRowY));
    memset(tileRowWidth, 0, sizeof(tileRowWidth));
    tileRowWrites = 0;
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    testMap.baseTile = (u16)((PAL0 << TILE_ATTR_PALETTE_SFT) | 0x0100);

    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, FALSE, FALSE);
    MovementGlow_setReachableMask(reachable, 3, 1);
    MovementGlow_setAttackableMask(targetable, 3, 1);

    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(80 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);

    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(90 + i));
    storedCallback(&testMap, tiles, 6, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL3);

    /* Al quitar el blanco, la casilla recupera su terreno normal; solo se
       transfieren las tres filas visuales de esa casilla. */
    assert(MovementGlow_clearAttackableMask(0, 0));
    assert(tileRowWrites == 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileRowX[i] == 6 && tileRowY[i] == i && tileRowWidth[i] == 3);
        assertTilePalette(tileRowData[i][0], PAL0);
    }

    /* Si el objetivo está también en el alcance de movimiento, al borrar la
       marca roja vuelve al glow PAL1, no pierde el resaltado inferior. */
    MovementGlow_setAttackableMask(overlappingTarget, 3, 1);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(100 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL3);
    assert(MovementGlow_clearAttackableMask(0, 0));
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(110 + i));
    storedCallback(&testMap, tiles, 3, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);

    /* Los aliados reparables usan el resaltado de alcance, no el rojo de ataque. */
    tileRowWrites = 0;
    assert(MovementGlow_setMasks(reachable, repairableTarget, 3, 1, 0, 0));
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(115 + i));
    storedCallback(&testMap, tiles, 6, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++) assertTilePalette(tiles[i], PAL1);
    assert(MovementGlow_clearAttackableMask(0, 0));

    /* Fog desconocido siempre tiene prioridad sobre la máscara de objetivos. */
    cells[2].visionState = MAP_VISION_UNSEEN;
    MovementGlow_setFogContext(&map, TEAM_BLUE, 600, TRUE, FALSE);
    MovementGlow_setAttackableMask(targetable, 3, 1);
    for (i = 0; i < 3; i++) tiles[i] = makeTerrainTile((u16)(120 + i));
    storedCallback(&testMap, tiles, 6, 0, ROW_UPDATE, 3);
    for (i = 0; i < 3; i++)
    {
        assert(tileIndex(tiles[i]) == 600);
        assertTilePalette(tiles[i], PAL2);
    }
    assert(MovementGlow_clearAttackableMask(0, 0));
    MovementGlow_setReachableMask(NULL, 3, 1);
    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
    puts("PASS: blancos atacables resaltados, restauracion localizada y niebla prioritaria");
}

static void testReleaseInvalidatesBorrowedMapAndMasks(void)
{
    const u16 paletteWritesBeforeRelease = paletteWriteCount;
    const u8 reachable[3] = { 0, 1, 0 };

    MovementGlow_release();
    MovementGlow_update();
    MovementGlow_update();
    MovementGlow_update();
    MovementGlow_update();
    assert(paletteWriteCount == paletteWritesBeforeRelease);
    assert(!MovementGlow_setMasks(reachable, NULL, 3, 1, 0, 0));
    assert(!MovementGlow_refreshFogCellRun(0, 0, 0, 0, 0));

    /* Una nueva sesión vuelve a enlazar su mapa sin estado visual antiguo. */
    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    assert(storedCallback != NULL);
    MovementGlow_setReachableMask(NULL, 0, 0);
    MovementGlow_setFogContext(NULL, TEAM_BLUE, 0, FALSE, FALSE);
    MovementGlow_release();
}

int main(void)
{
    memset(&testMap, 0, sizeof(testMap));
    testMap.planeWidth = 64;
    testMap.planeHeight = 64;
    testMap.plane = BG_A;
    assert(MovementGlow_init(&testMap, (const u16[16]){ 0 }));
    assert(storedCallback != NULL);
    testReachableTilesArePatchedWithoutChangingTheirTileIndex();
    testColumnUpdatesUseTileRowsAndClearOldGlow();
    testClearReachableMaskOnlyRefreshesLitCells();
    testCombinedMasksPatchOnlyChangedCellsWithoutFullRedraw();
    testFogHidesUnknownTerrainAndRestoresKnownTerrain();
    testSharedVisionKeepsFogOutsideBothTeamsVision();
    testPaletteClonePulseAndForcedRedraw();
    testLocalizedFogRefreshTouchesOnlyOneCell();
    testContiguousFogCellsAreBatchedIntoTileRows();
    testAttackableTargetHighlightUsesRedPaletteAndRestoresStyle();
    testReleaseInvalidatesBorrowedMapAndMasks();
    puts("PASS: paleta glow, parcheo, refresco localizado, liberación y reinicio de cachés");
    return 0;
}
