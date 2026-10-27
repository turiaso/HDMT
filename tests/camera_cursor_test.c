#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camera_cursor.h"
#include "game_config.h"

static void testFreeCursorMovementRemainsSmooth(void)
{
    CameraCursorState state;
    u8 frame;

    CameraCursor_init(20, 20);
    state = CameraCursor_getState();
    assert(state.cameraX == 0 && state.cameraY == 0);
    assert(state.cursorX == 144 && state.cursorY == 96);

    /* En exploracion del escenario, la primera pulsacion inicia una animacion. */
    CameraCursor_update(BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cursorX == 144 + CURSOR_ANIM_SPEED);
    assert(state.cursorX != 144 + CELL_PIXELS);

    /* La posicion alcanza el siguiente objetivo gradualmente. */
    for (frame = 0; frame < (CELL_PIXELS / CURSOR_ANIM_SPEED) - 1; frame++)
        CameraCursor_update(BUTTON_RIGHT);

    state = CameraCursor_getState();
    assert(state.cameraX == 0);
    assert(state.cursorX == 144 + CELL_PIXELS);
}

static void testCameraPanAcceleratesToResponsiveSpeed(void)
{
    CameraCursorState state;
    u8 frame;

    CameraCursor_init(40, 40);
    CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cameraX == CAMERA_STEP);
    for (frame = 1; frame < 10; frame++)
        CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
    state = CameraCursor_getState();

    /* El paneo debe avanzar claramente más que la antigua rampa 2..4 px/frame. */
    assert(state.cameraX >= 55);
    assert(state.cursorX == 144);
    puts("PASS: paneo manual acelera con rapidez y conserva el cursor fijo en pantalla");
}

static void testCameraMovementAndEdgeBounds(void)
{
    CameraCursorState state;
    u8 frame;

    CameraCursor_init(20, 20);
    state = CameraCursor_getState();

    CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cameraX > 0);
    assert(state.cursorX == 144);

    for (frame = 0; frame < 100; frame++)
        CameraCursor_update(BUTTON_C | BUTTON_RIGHT);

    state = CameraCursor_getState();
    assert(state.cameraX == (20 * CELL_PIXELS - VIEW_PIXELS_W));
    assert(state.cursorX <= VIEW_PIXELS_W - CELL_PIXELS);
    assert(state.cursorX > 144);

    for (frame = 0; frame < 160; frame++)
        CameraCursor_update(BUTTON_C | BUTTON_LEFT);

    state = CameraCursor_getState();
    assert(state.cameraX == 0);
    assert(state.cursorX >= 0);
}

static void testCameraStartsAtConfigurableHorizontalMargin(void)
{
    CameraCursorState state;
    u8 frame;
    s16 expectedX = VIEW_PIXELS_W - CELL_PIXELS -
                    CAMERA_EDGE_MARGIN_X_CELLS * CELL_PIXELS;

    CameraCursor_init(20, 20);
    for (frame = 0; frame < 80; frame++)
    {
        CameraCursor_update(BUTTON_RIGHT);
        state = CameraCursor_getState();
        if (state.cameraX > 0) break;
    }

    assert(state.cameraX > 0);
    assert(state.cursorX == expectedX);
}

static void testCameraStartsAtConfigurableVerticalMargin(void)
{
    CameraCursorState state;
    u8 frame;
    s16 expectedY = VIEW_PIXELS_H - CELL_PIXELS -
                    CAMERA_EDGE_MARGIN_Y_CELLS * CELL_PIXELS;

    CameraCursor_init(20, 20);
    for (frame = 0; frame < 60; frame++)
    {
        CameraCursor_update(BUTTON_DOWN);
        state = CameraCursor_getState();
        if (state.cameraY > 0) break;
    }

    assert(state.cameraY > 0);
    assert(state.cursorY == expectedY);
}


static void assertCursorDoesNotPauseAtEdge(u16 direction, bool horizontal,
                                           bool cameraAtFarEdge)
{
    CameraCursorState state;
    s16 cameraLimit;
    s16 cursorMargin;
    s16 cursorPosition;
    u16 cameraInput = BUTTON_C | direction;
    u16 frame;

    CameraCursor_init(20, 20);
    state = CameraCursor_getState();

    if (cameraAtFarEdge)
    {
        cameraLimit = horizontal
            ? (20 * CELL_PIXELS - VIEW_PIXELS_W)
            : (20 * CELL_PIXELS - VIEW_PIXELS_H);

        for (frame = 0; frame < 160; frame++)
        {
            state = CameraCursor_getState();
            if ((horizontal ? state.cameraX : state.cameraY) == cameraLimit)
                break;
            CameraCursor_update(cameraInput);
        }

        state = CameraCursor_getState();
        assert((horizontal ? state.cameraX : state.cameraY) == cameraLimit);
    }

    cursorPosition = horizontal ? state.cursorX : state.cursorY;
    cursorMargin = cameraAtFarEdge
        ? (horizontal ? VIEW_PIXELS_W : VIEW_PIXELS_H) - CELL_PIXELS -
              (horizontal ? CAMERA_EDGE_MARGIN_X_CELLS
                          : CAMERA_EDGE_MARGIN_Y_CELLS) * CELL_PIXELS
        : (horizontal ? CAMERA_EDGE_MARGIN_X_CELLS
                      : CAMERA_EDGE_MARGIN_Y_CELLS) * CELL_PIXELS;

    for (frame = 0; frame < 120 && cursorPosition != cursorMargin; frame++)
    {
        CameraCursor_update(direction);
        state = CameraCursor_getState();
        cursorPosition = horizontal ? state.cursorX : state.cursorY;
    }

    assert(cursorPosition == cursorMargin);

    /* En exploracion libre, un nuevo toque mueve el objetivo y anima el cursor. */
    CameraCursor_update(0);
    CameraCursor_update(direction);
    state = CameraCursor_getState();
    cursorPosition = horizontal ? state.cursorX : state.cursorY;
    assert(cursorPosition != cursorMargin +
        (cameraAtFarEdge ? CELL_PIXELS : -CELL_PIXELS));

    for (frame = 0; frame < (CELL_PIXELS / CURSOR_ANIM_SPEED) - 1; frame++)
        CameraCursor_update(direction);
    state = CameraCursor_getState();
    cursorPosition = horizontal ? state.cursorX : state.cursorY;

    if (cameraAtFarEdge)
        assert(cursorPosition == cursorMargin + CELL_PIXELS);
    else
        assert(cursorPosition == cursorMargin - CELL_PIXELS);
}


static void testCursorIsRestrictedToReachableCells(void)
{
    u8 reachable[20 * 20] = { 0 };
    CameraCursorState state;
    u8 frame;
    const u16 originX = 6;
    const u16 originY = 4;

    reachable[originY * 20 + originX + 1] = TRUE;
    CameraCursor_init(20, 20);

    /* Simula el cursor entre píxeles al seleccionar una unidad. */
    CameraCursor_update(BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cursorX == originX * CELL_PIXELS + CURSOR_ANIM_SPEED);

    CameraCursor_setMovementMask(reachable, 20, 20, originX, originY);
    state = CameraCursor_getState();
    /* Entrar en modo destino lo alinea con la casilla de la unidad. */
    assert(state.cursorX == originX * CELL_PIXELS);
    assert(state.cursorY == originY * CELL_PIXELS);

    /* En modo selección, la pulsación salta una celda completa. */
    CameraCursor_update(BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cursorX == (originX + 1) * CELL_PIXELS);

    /* Mantener la dirección no permite avanzar a una celda no alcanzable. */
    for (frame = 0; frame < 80; frame++)
        CameraCursor_update(BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cursorX == (originX + 1) * CELL_PIXELS);

    CameraCursor_update(0);
    CameraCursor_update(BUTTON_LEFT);
    state = CameraCursor_getState();
    /* El ancla se puede recuperar aunque no se marque como destino de glow. */
    assert(state.cursorX == originX * CELL_PIXELS);

    CameraCursor_setMovementMask(NULL, 20, 20, 0, 0);
    CameraCursor_update(0);
    CameraCursor_update(BUTTON_LEFT);
    state = CameraCursor_getState();
    /* Fuera del modo destino vuelve la interpolación por píxeles. */
    assert(state.cursorX < originX * CELL_PIXELS);
    assert(state.cursorX > (originX - 1) * CELL_PIXELS);
}


static bool isCellReachable(const u8 *mask, u16 width, u16 height,
                            u16 x, u16 y, u16 anchorX, u16 anchorY)
{
    if ((x == anchorX) && (y == anchorY)) return TRUE;
    if ((x >= width) || (y >= height)) return FALSE;
    return mask[(u32)y * width + x] != 0;
}

static void assertRestrictedCursorIsAligned(const u8 *mask,
                                            u16 width, u16 height,
                                            u16 anchorX, u16 anchorY)
{
    const CameraCursorState state = CameraCursor_getState();
    const s32 worldX = (s32)state.cameraX + state.cursorX;
    const s32 worldY = (s32)state.cameraY + state.cursorY;
    const u16 cellX = (u16)(worldX / CELL_PIXELS);
    const u16 cellY = (u16)(worldY / CELL_PIXELS);

    assert(worldX >= 0 && worldY >= 0);
    assert((worldX % CELL_PIXELS) == 0);
    assert((worldY % CELL_PIXELS) == 0);
    assert(isCellReachable(mask, width, height, cellX, cellY,
                           anchorX, anchorY));
}

static void testCameraScrollCannotEscapeMovementMask(void)
{
    enum { TEST_MAP_WIDTH = 40, TEST_MAP_HEIGHT = 20 };
    static u8 reachable[TEST_MAP_WIDTH * TEST_MAP_HEIGHT];
    const u16 anchorX = 6;
    const u16 anchorY = 4;
    CameraCursorState state;
    u16 frame;
    u16 x;

    memset(reachable, 0, sizeof(reachable));
    for (x = anchorX; x <= 18; x++)
        reachable[anchorY * TEST_MAP_WIDTH + x] = TRUE;

    CameraCursor_init(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);

    /* Seleccionar tras explorar deja la cámara en una fase subcelda realista. */
    CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
    state = CameraCursor_getState();
    assert(state.cameraX == CAMERA_STEP);

    CameraCursor_setMovementMask(reachable, TEST_MAP_WIDTH, TEST_MAP_HEIGHT,
                                 anchorX, anchorY);
    assertRestrictedCursorIsAligned(reachable, TEST_MAP_WIDTH, TEST_MAP_HEIGHT,
                                    anchorX, anchorY);

    /* El scroll automático no puede desalinear el cursor ni salir de la máscara. */
    for (frame = 0; frame < 500; frame++)
    {
        CameraCursor_update(BUTTON_RIGHT);
        assertRestrictedCursorIsAligned(reachable, TEST_MAP_WIDTH,
                                        TEST_MAP_HEIGHT, anchorX, anchorY);
    }

    state = CameraCursor_getState();
    assert(((state.cameraX + state.cursorX) / CELL_PIXELS) == 18);
    assert(state.cameraX == (s16)(18 * CELL_PIXELS - state.cursorX));

    /* El control directo de cámara obedece la misma máscara y cuadrícula. */
    for (frame = 0; frame < 200; frame++)
    {
        CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
        assertRestrictedCursorIsAligned(reachable, TEST_MAP_WIDTH,
                                        TEST_MAP_HEIGHT, anchorX, anchorY);
    }
    state = CameraCursor_getState();
    assert(((state.cameraX + state.cursorX) / CELL_PIXELS) == 18);

    /* La deselección restaura la cámara y el cursor suaves de exploración. */
    CameraCursor_setMovementMask(NULL, TEST_MAP_WIDTH, TEST_MAP_HEIGHT, 0, 0);
    CameraCursor_update(0);
    CameraCursor_update(BUTTON_LEFT);
    state = CameraCursor_getState();
    assert(state.cursorX != ((state.cameraX + state.cursorX) / CELL_PIXELS) * CELL_PIXELS);
}

static void testVerticalTargetScrollCannotEscapeMovementMask(void)
{
    enum { TEST_MAP_WIDTH = 20, TEST_MAP_HEIGHT = 30 };
    static u8 reachable[TEST_MAP_WIDTH * TEST_MAP_HEIGHT];
    const u16 anchorX = 4;
    const u16 anchorY = 4;
    CameraCursorState state;
    u16 frame;
    u16 y;

    memset(reachable, 0, sizeof(reachable));
    for (y = anchorY; y <= 20; y++)
        reachable[y * TEST_MAP_WIDTH + anchorX] = TRUE;

    CameraCursor_init(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
    CameraCursor_setMovementMask(reachable, TEST_MAP_WIDTH, TEST_MAP_HEIGHT,
                                 anchorX, anchorY);

    for (frame = 0; frame < 500; frame++)
    {
        CameraCursor_update(BUTTON_DOWN);
        assertRestrictedCursorIsAligned(reachable, TEST_MAP_WIDTH,
                                        TEST_MAP_HEIGHT, anchorX, anchorY);
    }

    state = CameraCursor_getState();
    assert(((state.cameraY + state.cursorY) / CELL_PIXELS) == 20);
}


static void testTargetScrollReachesMapEdgesWithoutLosingAlignment(void)
{
    enum { TEST_MAP_WIDTH = 40, TEST_MAP_HEIGHT = 30 };
    static u8 horizontalReachable[TEST_MAP_WIDTH * TEST_MAP_HEIGHT];
    static u8 verticalReachable[TEST_MAP_WIDTH * TEST_MAP_HEIGHT];
    const u16 anchorX = 6;
    const u16 anchorY = 4;
    CameraCursorState state;
    u16 frame;
    u16 x;
    u16 y;

    memset(horizontalReachable, 0, sizeof(horizontalReachable));
    for (x = anchorX; x < TEST_MAP_WIDTH; x++)
        horizontalReachable[anchorY * TEST_MAP_WIDTH + x] = TRUE;

    CameraCursor_init(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
    CameraCursor_update(BUTTON_C | BUTTON_RIGHT);
    CameraCursor_setMovementMask(horizontalReachable, TEST_MAP_WIDTH,
                                 TEST_MAP_HEIGHT, anchorX, anchorY);

    for (frame = 0; frame < 1200; frame++)
    {
        CameraCursor_update(BUTTON_RIGHT);
        assertRestrictedCursorIsAligned(horizontalReachable, TEST_MAP_WIDTH,
                                        TEST_MAP_HEIGHT, anchorX, anchorY);
    }

    state = CameraCursor_getState();
    assert(state.cameraX == (s16)(TEST_MAP_WIDTH * CELL_PIXELS - VIEW_PIXELS_W));
    assert(((state.cameraX + state.cursorX) / CELL_PIXELS) == TEST_MAP_WIDTH - 1);

    memset(verticalReachable, 0, sizeof(verticalReachable));
    for (y = anchorY; y < TEST_MAP_HEIGHT; y++)
        verticalReachable[y * TEST_MAP_WIDTH + anchorX] = TRUE;

    CameraCursor_init(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
    CameraCursor_update(BUTTON_C | BUTTON_DOWN);
    CameraCursor_setMovementMask(verticalReachable, TEST_MAP_WIDTH,
                                 TEST_MAP_HEIGHT, anchorX, anchorY);

    for (frame = 0; frame < 1200; frame++)
    {
        CameraCursor_update(BUTTON_DOWN);
        assertRestrictedCursorIsAligned(verticalReachable, TEST_MAP_WIDTH,
                                        TEST_MAP_HEIGHT, anchorX, anchorY);
    }

    state = CameraCursor_getState();
    assert(state.cameraY == (s16)(TEST_MAP_HEIGHT * CELL_PIXELS - VIEW_PIXELS_H));
    assert(((state.cameraY + state.cursorY) / CELL_PIXELS) == TEST_MAP_HEIGHT - 1);
}

static void testAutomaticCameraFocusAndReturn(void)
{
    CameraCursorState state;
    s32 focusX = 32 * CELL_PIXELS + CELL_PIXELS / 2;
    s32 focusY = 24 * CELL_PIXELS + CELL_PIXELS / 2;
    u16 frame;

    CameraCursor_init(40, 40);
    CameraCursor_focusWorldPixel(focusX, focusY);
    assert(CameraCursor_isAutoCameraActive());
    state = CameraCursor_getState();
    assert(state.cameraX == 0 && state.cameraY == 0);

    /* El encuadre automático ya no avanza a un ritmo inferior al de la unidad. */
    assert(CameraCursor_updateAutoCamera());
    state = CameraCursor_getState();
    assert(state.cameraX == CAMERA_AUTO_STEP &&
           state.cameraY == CAMERA_AUTO_STEP);

    for (frame = 0; frame < 300 && CameraCursor_isAutoCameraActive(); frame++)
        (void)CameraCursor_updateAutoCamera();

    state = CameraCursor_getState();
    assert(!CameraCursor_isAutoCameraActive());
    assert(state.cameraX + VIEW_PIXELS_W / 2 == focusX);
    assert(state.cameraY + VIEW_PIXELS_H / 2 == focusY);
    assert(CameraCursor_isWorldPixelFocused(focusX, focusY));

    /* La cámara puede volver suavemente al encuadre guardado del jugador. */
    CameraCursor_setAutoCameraTarget(0, 0);
    assert(CameraCursor_isAutoCameraActive());
    assert(!CameraCursor_isWorldPixelFocused(focusX, focusY));
    for (frame = 0; frame < 300 && CameraCursor_isAutoCameraActive(); frame++)
        (void)CameraCursor_updateAutoCamera();
    state = CameraCursor_getState();
    assert(!CameraCursor_isAutoCameraActive());
    assert(state.cameraX == 0 && state.cameraY == 0);
    assert(CameraCursor_isWorldPixelFocused(VIEW_PIXELS_W / 2,
                                            VIEW_PIXELS_H / 2));
}


static void testAICameraSafeZoneAvoidsUnneededRecentering(void)
{
    CameraCursorState state;
    const s32 margin = CELL_PIXELS;
    const s32 maxX = 40 * CELL_PIXELS - VIEW_PIXELS_W;
    const s32 maxY = 40 * CELL_PIXELS - VIEW_PIXELS_H;
    u16 frame;

    CameraCursor_init(40, 40);
    assert(CameraCursor_isWorldPointInsideSafeZone(
        VIEW_PIXELS_W / 2, VIEW_PIXELS_H / 2, margin, margin));
    /* Un actor visible pero demasiado pegado al borde debe pedir reencuadre. */
    assert(!CameraCursor_isWorldPointInsideSafeZone(
        VIEW_PIXELS_W - margin / 2, VIEW_PIXELS_H / 2, margin, margin));
    /* En el borde inicial, el margen izquierdo es inalcanzable pero válido. */
    assert(CameraCursor_isWorldPointInsideSafeZone(
        CELL_PIXELS / 2, VIEW_PIXELS_H / 2, margin, margin));

    CameraCursor_setAutoCameraTarget((s16)maxX, (s16)maxY);
    for (frame = 0; frame < 200 && CameraCursor_isAutoCameraActive(); frame++)
        (void)CameraCursor_updateAutoCamera();
    state = CameraCursor_getState();
    assert(state.cameraX == maxX && state.cameraY == maxY);
    /* En la esquina final del mapa también se acepta el margen imposible. */
    assert(CameraCursor_isWorldPointInsideSafeZone(
        40 * CELL_PIXELS - CELL_PIXELS / 2,
        40 * CELL_PIXELS - CELL_PIXELS / 2, margin, margin));
    /* Pero nunca se considera dentro si el punto ya está fuera de pantalla. */
    assert(!CameraCursor_isWorldPointInsideSafeZone(
        40 * CELL_PIXELS + CELL_PIXELS,
        40 * CELL_PIXELS - CELL_PIXELS / 2, margin, margin));
    puts("PASS: zona segura de cámara evita recentrar actores visibles y respeta bordes del mapa");
}

static void testCursorDoesNotPauseAtMapEdges(void)
{
    assertCursorDoesNotPauseAtEdge(BUTTON_LEFT, TRUE, FALSE);
    assertCursorDoesNotPauseAtEdge(BUTTON_RIGHT, TRUE, TRUE);
    assertCursorDoesNotPauseAtEdge(BUTTON_UP, FALSE, FALSE);
    assertCursorDoesNotPauseAtEdge(BUTTON_DOWN, FALSE, TRUE);
}

int main(void)
{
    testFreeCursorMovementRemainsSmooth();
    testCameraPanAcceleratesToResponsiveSpeed();
    testCameraMovementAndEdgeBounds();
    testCameraStartsAtConfigurableHorizontalMargin();
    testCameraStartsAtConfigurableVerticalMargin();
    testCursorDoesNotPauseAtMapEdges();
    testCursorIsRestrictedToReachableCells();
    testCameraScrollCannotEscapeMovementMask();
    testVerticalTargetScrollCannotEscapeMovementMask();
    testTargetScrollReachesMapEdgesWithoutLosingAlignment();
    testAutomaticCameraFocusAndReturn();
    testAICameraSafeZoneAvoidsUnneededRecentering();
    puts("PASS: cámara/cursor, scroll alineado, máscara estricta y seguimiento automático");
    return 0;
}
