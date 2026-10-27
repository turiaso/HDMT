#ifndef SCENARIO_MENU_H
#define SCENARIO_MENU_H

#include <genesis.h>

/*
 * Muestra el selector desde el escenario indicado y devuelve el índice elegido.
 * Un índice fuera de rango se normaliza al primer escenario.
 */
u8 ScenarioMenu_showFrom(u8 initialIndex);

/* Entrada compatible: abre el selector resaltando ACTIVE_MAP_INDEX. */
u8 ScenarioMenu_show(void);

#endif
