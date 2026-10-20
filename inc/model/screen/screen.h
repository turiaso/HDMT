#ifndef SCREEN_H
#define SCREEN_H

#include "conf.h"
#include "genesis.h"
/*
#include <model/objs/gfxos/sprites/gfxo_sprite.h>
#include <services/language_service.h>
#include <utils/texts/texts.h>
*/

/// Tiempo de bloqueo inicial para evitar efectos no deseados al pulsar botones
#define SCREEN_INITIAL_DELAY 300U

/**
 * Enumeración de los tipos de pantallas disponibles.
 */
typedef enum ScreenType {
#define X_MACRO(name) name,
    SCREEN_MACRO_LIST
#undef X_MACRO
        SCREEN_MACRO_TOTAL
} ScreenType;

/**
 * Callback para inicializar una pantalla.
 */
typedef void (*ScreenOnInitCb)();

/**
 * Callback para salir de una pantalla.
 */
typedef void (*ScreenOnExitCb)();

/**
 * Callback para pintar una pantalla.
 */
typedef void (*ScreenOnUpdateCb)();

/**
 * Callback para leer el estado del joystick.
 */
typedef void (*ScreenOnReadJoyCb)();

/**
 * Callback para manejar eventos del joystick.
 * @param joy Identificador del joystick.
 * @param changed Botones que cambiaron de estado.
 * @param state Estado actual de los botones.
 */
typedef void (*ScreenOnJoyEventCb)(u16 joy, u16 changed, u16 state);

/**
 * Callback para manejar interrupciones horizontales (H-Int).
 * Se ejecuta cuando el VDP alcanza una línea específica durante el renderizado.
 * Útil para efectos de scrolling paralaje, cambios de paleta o splits de pantalla.
 *
 * Para usar, agregar en la inicialización de la pantalla:
 *   VDP_setHInterrupt(1);      // Habilita la interrupción horizontal
 *   VDP_setHIntCounter(100);   // Dispara la H-Int en la línea 100
 *
 * Los cambios a aplicar deben realizarse en modo CPU dentro de este callback.
 * Ejemplo: cambiar paletas, modificar scroll, etc.
 *          PAL_setColors(0, newPalette, 16, CPU);
 *
 * Nota: Usar VInt para restarurar configuraciones para el inicio de FRAME.
 *
 * @see ScreenOnVIntCb
 */
typedef void (*ScreenOnHIntCb)();

/**
 * Callback para manejar interrupciones verticales.
 *
 * Se ejecuta al inicio de cada frame, útil para restaurar configuraciones.
 */
typedef void (*ScreenOnVIntCb)();

/**
 * Estructura que define los métodos asociados a una pantalla.
 */
typedef struct ScreenInfo {
    ScreenOnInitCb init;         ///< Callback para inicializar la pantalla
    ScreenOnExitCb exit;         ///< Callback para salir de la pantalla
    ScreenOnUpdateCb update;     ///< Callback para pintar la pantalla
    ScreenOnReadJoyCb readJoy;   ///< Callback para leer el joystick
    ScreenOnJoyEventCb joyEvent; ///< Callback para manejar eventos del joystick
    ScreenOnHIntCb hInt;         ///< Callback para manejar interrupciones horizontales
    ScreenOnVIntCb vInt;         ///< Callback para manejar interrupciones verticales
} ScreenInfo;

/// Macro para definir los métodos de una pantalla
#define SCREEN_MACRO_DEFINITION_CONSTRUCTOR(name)                                                                                                    \
    void name##_init();                                                                                                                              \
    void name##_exit();                                                                                                                              \
    void name##_update();                                                                                                                            \
    void name##_readJoy();                                                                                                                           \
    void name##_onJoyEvent(u16 joy, u16 changed, u16 state);                                                                                         \
    void name##_hInt();                                                                                                                              \
    void name##_vInt();

/// Macro para crear una entrada en la lista de pantallas
#define SCREEN_MACRO_INFO_ENTRY(name)                                                                                                                \
    ((ScreenInfo){                                                                                                                                   \
        .init = name##_init,                                                                                                                         \
        .exit = name##_exit,                                                                                                                         \
        .update = name##_update,                                                                                                                     \
        .readJoy = name##_readJoy,                                                                                                                   \
        .joyEvent = name##_onJoyEvent,                                                                                                               \
        .hInt = name##_hInt,                                                                                                                         \
        .vInt = name##_vInt})

#define X_MACRO(name) SCREEN_MACRO_DEFINITION_CONSTRUCTOR(name);
SCREEN_MACRO_LIST
#undef X_MACRO

#endif /* SCREEN_H */