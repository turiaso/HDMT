#ifndef CAMERA_CURSOR_H
#define CAMERA_CURSOR_H

#include <genesis.h>

typedef struct
{
    s16 cameraX;
    s16 cameraY;
    s16 cursorX;
    s16 cursorY;
} CameraCursorState;

/* Inicializa el controlador usando las dimensiones lógicas del mapa. */
void CameraCursor_init(u16 mapWidth, u16 mapHeight);
/*
 * Activa o desactiva la selección de destino.
 * La máscara contiene 1 en cada celda alcanzable; el origen siempre se admite.
 * Con máscara activa, el cursor y el auto-scroll de cámara respetan la máscara
 * y mantienen alineadas las coordenadas mundiales con la cuadrícula.
 */
void CameraCursor_setMovementMask(const u8 *reachableCells,
                                  u16 rangeWidth, u16 rangeHeight,
                                  u16 selectedCellX, u16 selectedCellY);
/*
 * Exploración: cursor y cámara suaves.
 * Selección: cursor por celdas y cámara desplazada en celdas completas.
 */
void CameraCursor_update(u16 joy);
CameraCursorState CameraCursor_getState(void);
/* Cámara automática para observar los turnos de la IA. World pixels son el
   punto que debe quedar centrado en el viewport. */
void CameraCursor_focusWorldPixel(s32 worldPixelX, s32 worldPixelY);
/* Fija un destino de cámara en píxeles de mapa (usado al volver al jugador). */
void CameraCursor_setAutoCameraTarget(s16 cameraPixelX, s16 cameraPixelY);
/* Avanza suavemente hacia el destino. Devuelve TRUE si la cámara se movió. */
bool CameraCursor_updateAutoCamera(void);
bool CameraCursor_isAutoCameraActive(void);
bool CameraCursor_isWorldPixelFocused(s32 worldPixelX, s32 worldPixelY);
/* Comprueba si un punto del mundo está dentro de un margen interior del
   viewport; en los bordes reales del mapa acepta el margen imposible de alcanzar. */
bool CameraCursor_isWorldPointInsideSafeZone(s32 worldPixelX, s32 worldPixelY,
                                             u16 marginX, u16 marginY);

#endif
