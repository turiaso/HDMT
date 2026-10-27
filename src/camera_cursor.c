/*
 * Coordenadas de cámara y cursor. Mantiene la relación entre celdas lógicas y pantalla;
 * los cambios de cámara deben respetar el paso de celda y las áreas de selección.
 */
#include <genesis.h>
#include "game_config.h"
#include "camera_cursor.h"

/*
 * Control de cámara y cursor, incluido el auto-scroll y el modo restringido al alcance.
 * Este módulo no dibuja sprites ni consulta unidades directamente: recibe una máscara
 * de alcanzabilidad y publica posiciones lógicas/de pantalla para el resto del motor.
 */

typedef struct
{
    s16 position;
    s16 maxPosition;
    s16 velocity;
    s8 lastDirection;
    s8 previousDirection;
    u16 accelerationTicks;
} CameraAxis;

typedef struct
{
    s16 position;
    s16 target;
    s16 maxPosition;
    s8 activeDirection;
    s8 lastDirection;
    u16 repeatTicks;
} CursorAxis;

typedef struct
{
    s8 direction;
    u16 repeatTicks;
} TargetScrollAxis;

static CameraAxis cameraX = {0};
static CameraAxis cameraY = {0};
static CursorAxis cursorX = {0};
static CursorAxis cursorY = {0};
static TargetScrollAxis targetScrollX = {0};
static TargetScrollAxis targetScrollY = {0};
static s8 activeInputDirX;
static s8 activeInputDirY;
static u16 previousJoy;
static u16 mapWidth;
static u16 mapHeight;
static const u8 *reachableMask;
static u16 reachableMapWidth;
static u16 reachableMapHeight;
static u16 originCellX;
static u16 originCellY;
static s16 autoCameraTargetX;
static s16 autoCameraTargetY;
static bool autoCameraActive;
/* La cámara automática debe superar con margen la unidad (4 px/frame), para
   encuadrarla en pocas actualizaciones en vez de perseguirla lentamente. */

/* Operaciones básicas de los ejes de cámara y cursor. */

static s16 clampS16(s16 value, s16 minValue, s16 maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

static s16 snapTarget(s16 position, s16 maxPosition, s8 direction)
{
    const s16 lower = (position / CELL_PIXELS) * CELL_PIXELS;
    s16 upper = lower + CELL_PIXELS;
    s16 target;

    if (direction > 0)
    {
        target = ((position + CELL_PIXELS - 1) / CELL_PIXELS) * CELL_PIXELS;
    }
    else if (direction < 0)
    {
        target = lower;
    }
    else
    {
        if (upper > maxPosition)
        {
            upper = maxPosition;
        }
        target = ((position - lower) <= (upper - position)) ? lower : upper;
    }
    return clampS16(target, 0, maxPosition);
}

static s16 moveToward(s16 position, s16 target, s16 step)
{
    if (position < target)
    {
        position += step;
        if (position > target)
        {
            position = target;
        }
    }
    else if (position > target)
    {
        position -= step;
        if (position < target)
        {
            position = target;
        }
    }
    return position;
}

static void updateCameraAxis(CameraAxis *axis, s8 desiredDirection)
{
    const s16 targetVelocity = desiredDirection * CAMERA_MAX_STEP;

    if (desiredDirection != 0)
    {
        axis->lastDirection = desiredDirection;
    }
    if (desiredDirection != axis->previousDirection)
    {
        axis->previousDirection = desiredDirection;
        axis->accelerationTicks = (axis->velocity != 0)
            ? CAMERA_ACCEL_INTERVAL - 1 : 0;
    }

    if ((axis->velocity == 0) && (desiredDirection != 0))
    {
        axis->velocity = desiredDirection * CAMERA_STEP;
        axis->accelerationTicks = 0;
    }
    else if (axis->velocity != targetVelocity)
    {
        axis->accelerationTicks++;
        if (axis->accelerationTicks >= CAMERA_ACCEL_INTERVAL)
        {
            axis->accelerationTicks = 0;
            if (axis->velocity < targetVelocity)
            {
                axis->velocity++;
            }
            else if (axis->velocity > targetVelocity)
            {
                axis->velocity--;
            }
        }
    }
    else
    {
        axis->accelerationTicks = 0;
    }

    axis->position += axis->velocity;
    if (axis->position < 0 || axis->position > axis->maxPosition)
    {
        axis->position = clampS16(axis->position, 0, axis->maxPosition);
        if ((axis->position == 0 && axis->velocity < 0) ||
            (axis->position == axis->maxPosition && axis->velocity > 0))
        {
            axis->velocity = 0;
            axis->accelerationTicks = 0;
        }
    }

    if ((axis->velocity == 0) && (desiredDirection == 0))
    {
        const s16 snappedTarget = snapTarget(axis->position, axis->maxPosition,
                                             axis->lastDirection);
        axis->position = moveToward(axis->position, snappedTarget, SNAP_SPEED);
    }
}

static s8 resolveDirection(u16 joy, u16 oldJoy, u16 negativeButton,
                           u16 positiveButton, s8 activeDirection,
                           s8 lastDirection)
{
    const bool negativeHeld = (joy & negativeButton) != 0;
    const bool positiveHeld = (joy & positiveButton) != 0;

    if (negativeHeld && positiveHeld)
    {
        const bool negativeNew = (oldJoy & negativeButton) == 0;
        const bool positiveNew = (oldJoy & positiveButton) == 0;

        if (negativeNew && !positiveNew)
        {
            return -1;
        }
        if (positiveNew && !negativeNew)
        {
            return 1;
        }
        if (activeDirection != 0)
        {
            return activeDirection;
        }
        if (lastDirection != 0)
        {
            return lastDirection;
        }
        return 1;
    }
    if (negativeHeld)
    {
        return -1;
    }
    if (positiveHeld)
    {
        return 1;
    }
    return 0;
}

/* Máscara de alcanzabilidad y navegación restringida. */

static bool isCellReachable(u16 cellX, u16 cellY, bool allowOrigin)
{
    u32 index;

    if ((cellX >= mapWidth) || (cellY >= mapHeight))
    {
        return FALSE;
    }
    if (reachableMask == NULL)
    {
        return TRUE;
    }
    if (allowOrigin && (cellX == originCellX) && (cellY == originCellY))
    {
        return TRUE;
    }
    if ((cellX >= reachableMapWidth) || (cellY >= reachableMapHeight))
    {
        return FALSE;
    }

    index = (u32)cellY * reachableMapWidth + cellX;
    return reachableMask[index] != 0;
}

/*
 * En modo destino, la esquina del cursor debe coincidir exactamente con la
 * esquina de una celda. Validamos la máscara y la alineación en un solo sitio
 * para que el cursor, el glow y la selección lógica nunca diverjan.
 */
static bool isAlignedWorldCellReachable(s16 cameraPixelX, s16 cameraPixelY,
                                        s16 cursorPixelX, s16 cursorPixelY,
                                        bool allowOrigin)
{
    const s32 worldPixelX = (s32)cameraPixelX + cursorPixelX;
    const s32 worldPixelY = (s32)cameraPixelY + cursorPixelY;
    u16 cellX;
    u16 cellY;

    if ((worldPixelX < 0) || (worldPixelY < 0) ||
        ((worldPixelX % CELL_PIXELS) != 0) ||
        ((worldPixelY % CELL_PIXELS) != 0))
    {
        return FALSE;
    }

    cellX = (u16)(worldPixelX / CELL_PIXELS);
    cellY = (u16)(worldPixelY / CELL_PIXELS);
    return isCellReachable(cellX, cellY, allowOrigin);
}

static bool isCursorCellReachable(s16 cursorPixelX, s16 cursorPixelY)
{
    return isAlignedWorldCellReachable(cameraX.position, cameraY.position,
                                       cursorPixelX, cursorPixelY, TRUE);
}

static void tryMoveCursorAxis(CursorAxis *axis, s8 direction, bool horizontal)
{
    const s16 candidate = clampS16(
        axis->position + direction * CELL_PIXELS, 0, axis->maxPosition);
    const s16 candidateX = horizontal ? candidate : cursorX.position;
    const s16 candidateY = horizontal ? cursorY.position : candidate;

    if ((candidate == axis->position) ||
        !isCursorCellReachable(candidateX, candidateY))
    {
        return;
    }

    /* En modo destino, el cursor salta una casilla y nunca interpola. */
    axis->position = candidate;
    axis->target = candidate;
}

static void updateCursorAxis(CursorAxis *axis, s8 direction, bool horizontal)
{
    const u16 repeatInterval = (reachableMask != NULL)
        ? CURSOR_TARGET_REPEAT_INTERVAL : CURSOR_REPEAT_INTERVAL;

    if (direction == 0)
    {
        axis->activeDirection = 0;
        axis->repeatTicks = 0;
        return;
    }

    if (direction != axis->activeDirection)
    {
        axis->activeDirection = direction;
        axis->lastDirection = direction;
        axis->repeatTicks = 0;
        if (reachableMask != NULL)
        {
            tryMoveCursorAxis(axis, direction, horizontal);
        }
        else
        {
            axis->target += direction * CELL_PIXELS;
        }
    }
    else if (++axis->repeatTicks >= repeatInterval)
    {
        axis->repeatTicks = 0;
        if (reachableMask != NULL)
        {
            tryMoveCursorAxis(axis, direction, horizontal);
        }
        else
        {
            axis->target += direction * CELL_PIXELS;
        }
    }

    if (reachableMask == NULL)
    {
        axis->target = clampS16(axis->target, 0, axis->maxPosition);
        axis->target = (axis->target / CELL_PIXELS) * CELL_PIXELS;
    }
}

static void animateCursorAxis(CursorAxis *axis)
{
    axis->position = moveToward(axis->position, axis->target,
                                CURSOR_ANIM_SPEED);
}

static bool cameraAtEdge(const CameraAxis *camera, s8 direction)
{
    return (direction < 0 && camera->position <= 0) ||
           (direction > 0 && camera->position >= camera->maxPosition);
}

static s8 updateCursorAndResolveScroll(CursorAxis *cursor, s8 direction,
                                       CameraAxis *camera, u16 marginCells,
                                       bool horizontal)
{
    const s16 margin = (s16)(marginCells * CELL_PIXELS);

    if (reachableMask == NULL)
    {
        const bool targetInMargin =
            (direction < 0 && cursor->target <= margin) ||
            (direction > 0 &&
             cursor->target >= cursor->maxPosition - margin);
        const bool positionAtMargin =
            (direction < 0 && cursor->position <= margin) ||
            (direction > 0 &&
             cursor->position >= cursor->maxPosition - margin);

        /* En exploración libre, la cámara espera a que el cursor animado
           alcance el margen antes de comenzar el scroll. */
        if ((direction != 0) && targetInMargin &&
            !cameraAtEdge(camera, direction))
        {
            if (!positionAtMargin)
            {
                return 0;
            }
            cursor->activeDirection = 0;
            cursor->repeatTicks = 0;
            return direction;
        }
    }
    else
    {
        const bool inMargin =
            (direction < 0 && cursor->position <= margin) ||
            (direction > 0 &&
             cursor->position >= cursor->maxPosition - margin);

        /* En destino, la cámara avanza por celdas sin desalinear el cursor. */
        if ((direction != 0) && inMargin && !cameraAtEdge(camera, direction))
        {
            cursor->activeDirection = 0;
            cursor->repeatTicks = 0;
            return direction;
        }
    }

    updateCursorAxis(cursor, direction, horizontal);
    return 0;
}

static void handleDirectCameraAxis(CameraAxis *camera, CursorAxis *cursor,
                                   s8 *direction, bool horizontal)
{
    if (cameraAtEdge(camera, *direction))
    {
        updateCursorAxis(cursor, *direction, horizontal);
        *direction = 0;
        return;
    }
    cursor->activeDirection = 0;
    cursor->repeatTicks = 0;
}

static bool shouldAdvanceTargetCamera(TargetScrollAxis *scroll, s8 direction)
{
    if (direction == 0)
    {
        scroll->direction = 0;
        scroll->repeatTicks = 0;
        return FALSE;
    }

    if (direction != scroll->direction)
    {
        scroll->direction = direction;
        scroll->repeatTicks = 0;
        return TRUE;
    }

    scroll->repeatTicks++;
    if (scroll->repeatTicks < CAMERA_TARGET_SCROLL_INTERVAL)
    {
        return FALSE;
    }

    scroll->repeatTicks = 0;
    return TRUE;
}

/*
 * En modo destino el auto-scroll solo avanza una celda completa. Esto mantiene
 * alineados el tilemap y el sprite del cursor durante todo el desplazamiento.
 * En el borde físico del mapa puede quedar un resto menor que una celda; se
 * compensa con el cursor para conservar la celda lógica y la alineación exacta.
 */
static void advanceTargetCameraByCell(CameraAxis *camera, CursorAxis *cursor,
                                      TargetScrollAxis *scroll, s8 direction,
                                      bool horizontal)
{
    s32 candidate;
    s16 delta;
    s16 nextCursorPosition = cursor->position;
    s16 nextCameraX;
    s16 nextCameraY;
    s16 nextCursorX;
    s16 nextCursorY;

    if (!shouldAdvanceTargetCamera(scroll, direction))
    {
        return;
    }

    candidate = (s32)camera->position + direction * CELL_PIXELS;
    if (candidate < 0)
    {
        candidate = 0;
    }
    if (candidate > camera->maxPosition)
    {
        candidate = camera->maxPosition;
    }

    delta = (s16)(candidate - camera->position);
    if (delta == 0)
    {
        return;
    }

    /* Solo el último paso hacia el borde puede ser menor que una celda. */
    if ((delta > -CELL_PIXELS) && (delta < CELL_PIXELS))
    {
        nextCursorPosition = (s16)(cursor->position - delta);
        if ((nextCursorPosition < 0) ||
            (nextCursorPosition > cursor->maxPosition))
        {
            return;
        }
    }

    nextCameraX = horizontal ? (s16)candidate : cameraX.position;
    nextCameraY = horizontal ? cameraY.position : (s16)candidate;
    nextCursorX = horizontal ? nextCursorPosition : cursorX.position;
    nextCursorY = horizontal ? cursorY.position : nextCursorPosition;

    /* No se mueve la cámara si la celda que quedaría bajo el cursor no vale. */
    if (!isAlignedWorldCellReachable(nextCameraX, nextCameraY,
                                     nextCursorX, nextCursorY, TRUE))
    {
        return;
    }

    camera->position = (s16)candidate;
    camera->velocity = 0;
    camera->accelerationTicks = 0;
    camera->previousDirection = direction;
    camera->lastDirection = direction;

    if ((delta > -CELL_PIXELS) && (delta < CELL_PIXELS))
    {
        cursor->position = nextCursorPosition;
        cursor->target = nextCursorPosition;
        cursor->activeDirection = 0;
        cursor->repeatTicks = 0;
    }
}

/* Entrada del mando y selección del modo cámara/cursor. */

static void updateInput(u16 joy)
{
    const bool cameraButton = (joy & BUTTON_C) != 0;
    s8 dx = resolveDirection(joy, previousJoy, BUTTON_LEFT, BUTTON_RIGHT,
                              activeInputDirX, cameraX.lastDirection);
    s8 dy = resolveDirection(joy, previousJoy, BUTTON_UP, BUTTON_DOWN,
                              activeInputDirY, cameraY.lastDirection);

    if (cameraButton)
    {
        activeInputDirX = dx;
        activeInputDirY = dy;
        handleDirectCameraAxis(&cameraX, &cursorX, &dx, TRUE);
        handleDirectCameraAxis(&cameraY, &cursorY, &dy, FALSE);
    }
    else
    {
        const s8 cursorDx = resolveDirection(
            joy, previousJoy, BUTTON_LEFT, BUTTON_RIGHT,
            cursorX.activeDirection, cursorX.lastDirection);
        const s8 cursorDy = resolveDirection(
            joy, previousJoy, BUTTON_UP, BUTTON_DOWN,
            cursorY.activeDirection, cursorY.lastDirection);

        dx = updateCursorAndResolveScroll(&cursorX, cursorDx, &cameraX,
                                          CAMERA_EDGE_MARGIN_X_CELLS, TRUE);
        dy = updateCursorAndResolveScroll(&cursorY, cursorDy, &cameraY,
                                          CAMERA_EDGE_MARGIN_Y_CELLS, FALSE);
        activeInputDirX = activeInputDirY = 0;
    }

    if (reachableMask == NULL)
    {
        /* Exploración libre: cámara suave y acelerada. */
        updateCameraAxis(&cameraX, dx);
        updateCameraAxis(&cameraY, dy);
    }
    else
    {
        /* Selección de destino: scroll por celdas para conservar la cuadrícula. */
        advanceTargetCameraByCell(&cameraX, &cursorX, &targetScrollX, dx, TRUE);
        advanceTargetCameraByCell(&cameraY, &cursorY, &targetScrollY, dy, FALSE);
    }
    previousJoy = joy;
}

/* API pública de cámara y cursor. */

void CameraCursor_init(u16 logicalMapWidth, u16 logicalMapHeight)
{
    const u16 mapPixelWidth = logicalMapWidth * CELL_PIXELS;
    const u16 mapPixelHeight = logicalMapHeight * CELL_PIXELS;
    const u16 viewportWidth =
        (mapPixelWidth < VIEW_PIXELS_W) ? mapPixelWidth : VIEW_PIXELS_W;
    const u16 viewportHeight =
        (mapPixelHeight < VIEW_PIXELS_H) ? mapPixelHeight : VIEW_PIXELS_H;

    cameraX = (CameraAxis){ 0 };
    cameraY = (CameraAxis){ 0 };
    cursorX = (CursorAxis){ 0 };
    cursorY = (CursorAxis){ 0 };
    targetScrollX = (TargetScrollAxis){ 0 };
    targetScrollY = (TargetScrollAxis){ 0 };
    activeInputDirX = activeInputDirY = 0;
    previousJoy = 0;
    reachableMask = NULL;
    reachableMapWidth = reachableMapHeight = 0;
    originCellX = originCellY = 0;
    autoCameraTargetX = autoCameraTargetY = 0;
    autoCameraActive = FALSE;
    mapWidth = logicalMapWidth;
    mapHeight = logicalMapHeight;

    cameraX.maxPosition = (mapPixelWidth > VIEW_PIXELS_W)
        ? (s16)(mapPixelWidth - VIEW_PIXELS_W) : 0;
    cameraY.maxPosition = (mapPixelHeight > VIEW_PIXELS_H)
        ? (s16)(mapPixelHeight - VIEW_PIXELS_H) : 0;
    cursorX.maxPosition = (viewportWidth >= CELL_PIXELS)
        ? (s16)(viewportWidth - CELL_PIXELS) : 0;
    cursorY.maxPosition = (viewportHeight >= CELL_PIXELS)
        ? (s16)(viewportHeight - CELL_PIXELS) : 0;

    cursorX.position = cursorX.target = clampS16(
        ((viewportWidth / 2) / CELL_PIXELS) * CELL_PIXELS,
        0, cursorX.maxPosition);
    cursorY.position = cursorY.target = clampS16(
        ((viewportHeight / 2) / CELL_PIXELS) * CELL_PIXELS,
        0, cursorY.maxPosition);
}

static void relocateCursorToAllowedCell(void)
{
    const u32 viewEndX = (u32)cameraX.position + VIEW_PIXELS_W;
    const u32 viewEndY = (u32)cameraY.position + VIEW_PIXELS_H;
    const u16 firstCellX = (u16)(cameraX.position / CELL_PIXELS);
    const u16 firstCellY = (u16)(cameraY.position / CELL_PIXELS);
    const u32 visibleLastCellX = viewEndX / CELL_PIXELS;
    const u32 visibleLastCellY = viewEndY / CELL_PIXELS;
    const u16 lastCellX = (u16)((visibleLastCellX < mapWidth)
        ? visibleLastCellX : (u32)(mapWidth - 1u));
    const u16 lastCellY = (u16)((visibleLastCellY < mapHeight)
        ? visibleLastCellY : (u32)(mapHeight - 1u));
    s32 bestX = cursorX.position;
    s32 bestY = cursorY.position;
    u32 bestDistance = 0xFFFFFFFFu;
    u16 cellX;
    u16 cellY;

    for (cellY = firstCellY; cellY <= lastCellY; cellY++)
    {
        const s32 screenY = (s32)cellY * CELL_PIXELS - cameraY.position;
        if ((screenY < -(CELL_PIXELS - 1)) ||
            (screenY > cursorY.maxPosition))
            continue;

        for (cellX = firstCellX; cellX <= lastCellX; cellX++)
        {
            const s32 screenX = (s32)cellX * CELL_PIXELS - cameraX.position;
            const s32 dx = (screenX > cursorX.position)
                ? screenX - cursorX.position : cursorX.position - screenX;
            const s32 dy = (screenY > cursorY.position)
                ? screenY - cursorY.position : cursorY.position - screenY;
            const u32 distance = (u32)(dx + dy);

            if ((screenX < -(CELL_PIXELS - 1)) ||
                (screenX > cursorX.maxPosition) ||
                (distance >= bestDistance) ||
                !isCellReachable(cellX, cellY, TRUE))
                continue;

            bestDistance = distance;
            bestX = screenX;
            bestY = screenY;
        }
    }

    if (bestDistance != 0xFFFFFFFFu)
    {
        cursorX.position = cursorX.target = (s16)bestX;
        cursorY.position = cursorY.target = (s16)bestY;
        cursorX.activeDirection = cursorY.activeDirection = 0;
        cursorX.repeatTicks = cursorY.repeatTicks = 0;
    }
}

void CameraCursor_setMovementMask(const u8 *reachableCells,
                                  u16 rangeWidth, u16 rangeHeight,
                                  u16 selectedCellX, u16 selectedCellY)
{
    const bool wasRestricted = reachableMask != NULL;
    const bool originChanged = (reachableCells != NULL) &&
        ((selectedCellX != originCellX) || (selectedCellY != originCellY));

    if (reachableCells == NULL)
    {
        reachableMask = NULL;
        reachableMapWidth = reachableMapHeight = 0;
        originCellX = originCellY = 0;
        targetScrollX = (TargetScrollAxis){ 0 };
        targetScrollY = (TargetScrollAxis){ 0 };
        cameraX.velocity = cameraY.velocity = 0;
        cameraX.accelerationTicks = cameraY.accelerationTicks = 0;

        /* Al volver a explorar, la animación se reanuda desde la posición
           actual y se dirige al siguiente punto de rejilla de pantalla. */
        cursorX.target = snapTarget(cursorX.position, cursorX.maxPosition, 0);
        cursorY.target = snapTarget(cursorY.position, cursorY.maxPosition, 0);
        cursorX.activeDirection = cursorY.activeDirection = 0;
        cursorX.repeatTicks = cursorY.repeatTicks = 0;
        return;
    }

    reachableMask = reachableCells;
    reachableMapWidth = rangeWidth;
    reachableMapHeight = rangeHeight;
    originCellX = selectedCellX;
    originCellY = selectedCellY;
    targetScrollX = (TargetScrollAxis){ 0 };
    targetScrollY = (TargetScrollAxis){ 0 };
    cameraX.velocity = cameraY.velocity = 0;
    cameraX.accelerationTicks = cameraY.accelerationTicks = 0;

    /* Al entrar en modo destino o cambiar de unidad, coloca el cursor en
       la celda de origen y compensa la posición actual de la cámara. */
    if (!wasRestricted || originChanged)
    {
        const s32 originScreenX =
            (s32)selectedCellX * CELL_PIXELS - cameraX.position;
        const s32 originScreenY =
            (s32)selectedCellY * CELL_PIXELS - cameraY.position;

        cursorX.position = clampS16((s16)originScreenX, 0,
                                    cursorX.maxPosition);
        cursorY.position = clampS16((s16)originScreenY, 0,
                                    cursorY.maxPosition);
        cursorX.target = cursorX.position;
        cursorY.target = cursorY.position;
        cursorX.activeDirection = cursorY.activeDirection = 0;
        cursorX.repeatTicks = cursorY.repeatTicks = 0;
    }

    if (!isCursorCellReachable(cursorX.position, cursorY.position))
        relocateCursorToAllowedCell();
}

void CameraCursor_update(u16 joy)
{
    const s16 oldCameraX = cameraX.position;
    const s16 oldCameraY = cameraY.position;

    updateInput(joy);
    if (reachableMask == NULL)
    {
        if (((joy & BUTTON_C) == 0) || (cameraX.position == oldCameraX))
            animateCursorAxis(&cursorX);
        if (((joy & BUTTON_C) == 0) || (cameraY.position == oldCameraY))
            animateCursorAxis(&cursorY);
    }
}

CameraCursorState CameraCursor_getState(void)
{
    return (CameraCursorState){
        cameraX.position, cameraY.position,
        cursorX.position, cursorY.position
    };
}


/* Enfoque programático de cámara para el modo espectador de la IA. */

void CameraCursor_setAutoCameraTarget(s16 targetX, s16 targetY)
{
    autoCameraTargetX = clampS16(targetX, 0, cameraX.maxPosition);
    autoCameraTargetY = clampS16(targetY, 0, cameraY.maxPosition);
    autoCameraActive = (cameraX.position != autoCameraTargetX) ||
                       (cameraY.position != autoCameraTargetY);

    /* No mezclar la aceleración manual con el seguimiento automático. */
    cameraX.velocity = cameraY.velocity = 0;
    cameraX.accelerationTicks = cameraY.accelerationTicks = 0;
    cameraX.previousDirection = cameraY.previousDirection = 0;
}

void CameraCursor_focusWorldPixel(s32 worldPixelX, s32 worldPixelY)
{
    const s32 targetX = worldPixelX - (VIEW_PIXELS_W / 2);
    const s32 targetY = worldPixelY - (VIEW_PIXELS_H / 2);
    const s16 clampedX = (targetX < 0) ? 0 :
        ((targetX > cameraX.maxPosition) ? cameraX.maxPosition : (s16)targetX);
    const s16 clampedY = (targetY < 0) ? 0 :
        ((targetY > cameraY.maxPosition) ? cameraY.maxPosition : (s16)targetY);

    CameraCursor_setAutoCameraTarget(clampedX, clampedY);
}

bool CameraCursor_updateAutoCamera(void)
{
    const s16 oldX = cameraX.position;
    const s16 oldY = cameraY.position;

    if (!autoCameraActive) return FALSE;

    cameraX.position = moveToward(cameraX.position, autoCameraTargetX,
                                  CAMERA_AUTO_STEP);
    cameraY.position = moveToward(cameraY.position, autoCameraTargetY,
                                  CAMERA_AUTO_STEP);
    autoCameraActive = (cameraX.position != autoCameraTargetX) ||
                       (cameraY.position != autoCameraTargetY);

    return (oldX != cameraX.position) || (oldY != cameraY.position);
}

bool CameraCursor_isAutoCameraActive(void)
{
    return autoCameraActive;
}

bool CameraCursor_isWorldPixelFocused(s32 worldPixelX, s32 worldPixelY)
{
    const s32 rawTargetX = worldPixelX - (VIEW_PIXELS_W / 2);
    const s32 rawTargetY = worldPixelY - (VIEW_PIXELS_H / 2);
    const s16 targetX = (rawTargetX < 0) ? 0 :
        ((rawTargetX > cameraX.maxPosition) ? cameraX.maxPosition
                                             : (s16)rawTargetX);
    const s16 targetY = (rawTargetY < 0) ? 0 :
        ((rawTargetY > cameraY.maxPosition) ? cameraY.maxPosition
                                             : (s16)rawTargetY);

    return !autoCameraActive && (cameraX.position == targetX) &&
           (cameraY.position == targetY);
}

bool CameraCursor_isWorldPointInsideSafeZone(s32 worldPixelX, s32 worldPixelY,
                                             u16 marginX, u16 marginY)
{
    const s32 screenX = worldPixelX - cameraX.position;
    const s32 screenY = worldPixelY - cameraY.position;
    const s32 leftMargin = ((s32)marginX * 2 >= VIEW_PIXELS_W)
        ? VIEW_PIXELS_W / 2 : marginX;
    const s32 topMargin = ((s32)marginY * 2 >= VIEW_PIXELS_H)
        ? VIEW_PIXELS_H / 2 : marginY;
    bool insideX = (screenX >= leftMargin) &&
                   (screenX <= (VIEW_PIXELS_W - leftMargin));
    bool insideY = (screenY >= topMargin) &&
                   (screenY <= (VIEW_PIXELS_H - topMargin));

    /* En un borde real de mapa el punto no siempre puede alcanzar el margen,
       pero exigimos que siga dentro del viewport y no fuera de la pantalla. */
    if (!insideX)
        insideX = ((cameraX.position <= 0) && (screenX >= 0) &&
                   (screenX < leftMargin)) ||
                  ((cameraX.position >= cameraX.maxPosition) &&
                   (screenX > (VIEW_PIXELS_W - leftMargin)) &&
                   (screenX < VIEW_PIXELS_W));
    if (!insideY)
        insideY = ((cameraY.position <= 0) && (screenY >= 0) &&
                   (screenY < topMargin)) ||
                  ((cameraY.position >= cameraY.maxPosition) &&
                   (screenY > (VIEW_PIXELS_H - topMargin)) &&
                   (screenY < VIEW_PIXELS_H));

    return insideX && insideY;
}
