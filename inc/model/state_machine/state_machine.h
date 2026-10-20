#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "genesis.h"

/**
 * Estado de la máquina de estados.
 */
typedef State (*State)(void *ctx);

/**
 * Estructura que define los métodos asociados a una pantalla.
 */
typedef struct StateMachine {
    State state;              ///< Callback para manejar el estado actual de la máquina de estados
    void *ctx;                 ///< Contexto asociado al estado actual de la máquina de estados
} StateMachine;

/**
 * Crea la máquina de estados con un estado y contexto dados.
 */
StateMachine *StateMachine_init(State stateInitial, void *ctx);

/**
 * Actualiza la máquina de estados ejecutando el callback del estado actual.
 */
void StateMachine_update(StateMachine *sm);

/**
 * Destruye la máquina de estados liberando la memoria asociada.
 */
void StateMachine_destroy(StateMachine **sm);

/**
 * Estado nulo de la máquina de estados.
 */
State STATE_NULL(void *ctx);

#endif /* STATE_MACHINE_H */