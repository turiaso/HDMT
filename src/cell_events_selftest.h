#ifndef CELL_EVENTS_SELFTEST_H
#define CELL_EVENTS_SELFTEST_H

/*
 * Prueba funcional opcional que corre dentro del juego con el compilador SGDK.
 * Déjalo a 0 para la ROM normal. Para la prueba, cámbialo a 1, recompila y
 * abre la salida de KDebug del emulador/debugger.
 */
#ifndef CELL_EVENTS_SELF_TEST
#define CELL_EVENTS_SELF_TEST 0
#endif

#if CELL_EVENTS_SELF_TEST
#include <genesis.h>
bool CellEvents_runSelfTest(void);
#endif

#endif
