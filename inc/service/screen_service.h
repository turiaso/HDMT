#ifndef SCREEN_SERVICE_H
#define SCREEN_SERVICE_H

#include "conf.h"
#include "genesis.h"
#include <model/screen/screen.h>

/// Intervalo de actualización del servicio de pantallas en ticks
#define SCREEN_SERVICE_UPDATE_TICK_INTERVAL 10

/**
 * Inicializa el servicio de pantallas.
 * Configura el juego y establece la pantalla inicial.
 */
void SCREEN_SERVICE_init();

/**
 * Actualiza el servicio de pantallas.
 * Maneja la lógica de actualización, renderizado y eventos de joystick.
 */
void SCREEN_SERVICE_update();

#endif /* SCREEN_SERVICE_H */