#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include "unit_types.h"

/* Parámetros globales de hardware, cámara, cursor y HUD. */
#define HARDWARE_TILE          8
#define CELL_TILES             3
#define CELL_PIXELS            (HARDWARE_TILE * CELL_TILES)
#define SCREEN_PIXELS_W        320
#define SCREEN_PIXELS_H        224
#define VRAM_TILE_COUNT        512
/*
 * Escenario resaltado por defecto en el selector de arranque:
 *   0 = Punto de control (40x40, 60 unidades por bando).
 *   1 = Defensa del puente (40x40, 12 unidades por bando).
 *   2 = Puente del desfiladero (20x20, 6 unidades por bando).
 * La cruceta permite elegir cualquier escenario en la propia ROM.
 */
#ifndef ACTIVE_MAP_INDEX
#define ACTIVE_MAP_INDEX       0
#endif

/* Presentación y cámara durante el turno de la IA. */
#define AI_ATTACK_PRESENTATION_FRAMES 30u
/* La IA solo reencuadra al salir de la zona segura; después deja margen interior. */
#define AI_CAMERA_FOLLOW_START_CELLS 1u
#define AI_CAMERA_FOLLOW_STOP_CELLS  3u

/* Movimiento de cámara: píxeles por fotograma.
 * La cámara manual debe poder recorrer un mapa 40x40 con rapidez; mantener
 * cada paso por debajo de 16 px evita saltarse más de una fila/columna interna
 * del motor MAP de SGDK en una sola actualización. */
#define CAMERA_STEP            4
#define CAMERA_MAX_STEP        8
#define CAMERA_ACCEL_INTERVAL  2
#define CAMERA_AUTO_STEP       10
#define SNAP_SPEED             1

/* No permitir saltos continuos >=16 px/frame: el motor MAP de SGDK actualiza
   el anillo del tilemap en unidades de metatile de 16 px. */
#if (CAMERA_MAX_STEP >= 16) || (CAMERA_AUTO_STEP >= 16)
#error "La velocidad continua de camara debe ser menor que 16 px/frame"
#endif

/* Celdas de margen para iniciar el scroll antes de alcanzar el borde. */
#define CAMERA_EDGE_MARGIN_X_CELLS 2
#define CAMERA_EDGE_MARGIN_Y_CELLS 2

/* VRAM reservada para sprites de unidad y cursor. */
#define CURSOR_REPEAT_INTERVAL 8
#define CURSOR_TARGET_REPEAT_INTERVAL 4
/* Fotogramas entre pasos de celda al desplazar la cámara en modo destino.
 * La selección sigue alineada a la cuadrícula, pero no espera ocho frames. */
#define CAMERA_TARGET_SCROLL_INTERVAL 4
#define CURSOR_ANIM_SPEED      3
#define UNIT_VRAM_TILE_INDEX   464
#define UNIT_SPRITE_SIZE_PIXELS 16
#define UNIT_SPRITE_TILES_PER_AXIS (UNIT_SPRITE_SIZE_PIXELS / HARDWARE_TILE)
#define UNIT_SPRITE_TILE_COUNT (UNIT_SPRITE_TILES_PER_AXIS * UNIT_SPRITE_TILES_PER_AXIS)
#define UNIT_VRAM_TILE_COUNT   (UNIT_TYPE_COUNT * UNIT_SPRITE_TILE_COUNT)
#define CURSOR_VRAM_TILE_INDEX 500
#define CURSOR_SPRITE_TILE_COUNT (3 * 3)
#define UNIT_MOVE_SPEED_PIXELS   4

/*
 * Mantiene el atlas fuera de los tiles de sistema y de la longitud reservada
 * para la fuente SGDK. No usar TILE_USER_INDEX: cambia según
 * LEGACY_FONT_LOCATION y la configuración de la biblioteca enlazada.
 */
#define TERRAIN_VRAM_TILE_INDEX (TILE_SYSTEM_INDEX + TILE_SYSTEM_LENGTH + FONT_LENGTH)

/* HUD de celda, turno y objetivo en BG_B. */
#define HUD_TEXT_X             1
#define HUD_TEXT_Y             1
#define HUD_TEXT_WIDTH         35
#define HUD_CELL_INFO_LINES    4
#define HUD_TEXT_LINES         (HUD_CELL_INFO_LINES + 3)
#define HUD_ACTION_FEEDBACK_FRAMES 45u

/* Área lógica formada por celdas completas; el VDP puede mostrar los píxeles
   parciales de borde (320x224) sin máscaras estáticas sobre el plano HUD. */
#define VIEW_PIXELS_W          ((SCREEN_PIXELS_W / CELL_PIXELS) * CELL_PIXELS)
#define VIEW_PIXELS_H          ((SCREEN_PIXELS_H / CELL_PIXELS) * CELL_PIXELS)

/* Los tres sprites fijos son cursor, marcador de objetivo y reserva del tile
   de niebla; no se dibujan máscaras geométricas permanentes sobre el mapa. */
#define MD_VDP_SPRITE_LIMIT       80
#define FIXED_GAME_SPRITE_COUNT   3
#define MAX_UNIT_SPRITES          (MD_VDP_SPRITE_LIMIT - FIXED_GAME_SPRITE_COUNT)

/* El perfilado es opt-in: los logs pueden alterar el rendimiento observado.
 * Actívalo temporalmente con -DPERF_DIAGNOSTICS=1 para medir una build. */
#ifndef PERF_DIAGNOSTICS
#define PERF_DIAGNOSTICS          0
#endif

#endif
