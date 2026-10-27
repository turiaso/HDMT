# Pantalla de resultado y flujo de partida

## Contenido del resumen

`GameScreens_showMissionResult()` presenta la pantalla final sobre el plano de texto, tras salir del bucle de juego. Muestra el equipo ganador y el equipo derrotado (o el empate), el nombre del objetivo, el motivo del resultado, los turnos transcurridos, las unidades supervivientes de Azul y Rojo y, para objetivos temporizados, el progreso alcanzado frente al requisito.

Los motivos se centralizan en `GameTurn_getOutcomeReasonText()` y el éxito del objetivo en `GameTurn_didCompleteObjective()`, para que no se infieran en la capa gráfica. En supervivencia, una victoria del rival por eliminar al equipo objetivo muestra `OBJETIVO: NO CUMPLIDO`; un empate muestra `OBJETIVO: SIN RESOLVER`.

- Captura: `OBJETIVO CAPTURADO`.
- Mantenimiento: `OBJETIVO MANTENIDO`.
- Eliminación: `EJERCITO RIVAL ELIMINADO`.
- Supervivencia: `SUPERVIVENCIA COMPLETADA`, eliminación del rival o eliminación del equipo objetivo, según el estado final.
- Empate por ambos ejércitos vacíos: `AMBOS EQUIPOS ELIMINADOS`.
- Otros empates: `EMPATE TECNICO`.

`GameTurn_getTurnsElapsed()` cuenta los cierres reales de turno. El contador se incrementa dentro del único punto que despacha `TURN_END`, por lo que no crece con consultas al objetivo ni con reintentos del cierre. Un resultado inmediato durante la inicialización muestra cero turnos jugados.

`GameTurn_getAliveUnitCount()` cuenta las unidades que siguen vivas de un equipo. Los registros de unidades eliminadas permanecen en el controlador, pero no cuentan como supervivientes.

## Controles y reinicio

- **A:** repetir el escenario actual.
- **B o START:** volver al selector de misiones.

La pantalla espera a que se suelten los botones que pudieron causar el resultado antes de aceptar una nueva entrada; la selección se hace por flanco de pulsación y espera la liberación del botón al confirmar la acción.

Ambas opciones regresan al bucle exterior de `main.c`. La repetición vuelve a ejecutar `initGameData()` e `initVideo()` con el escenario actual. Volver al selector libera el mapa y termina el motor de sprites antes de abrir el menú. El selector conserva resaltado el último escenario elegido, pero no conserva estado de juego. `MovementGlow_release()` y `UnitView_release()` invalidan sus referencias prestadas antes de liberar mapa y sprites; después se limpian las cachés y los planos. El detalle del orden está en [`ciclo_partida_y_reinicio.md`](ciclo_partida_y_reinicio.md).

## Bloqueo tras finalizar

`GameTurn_canIssueCommands()` es la condición común de aceptación de órdenes. Los manejadores de entrada del jugador, las decisiones de la IA, la activación de una casilla, el avance de una unidad y `updateGameFrame()` la comprueban. `GameTurn_endTurn()` y `GameTurn_checkObjective()` también rechazan cambios después del resultado. Así no se pueden emitir ataques, rutas o cierres adicionales mientras se presenta el resumen.

El bloqueo es de la capa de juego; no cambia las reglas de combate ni toca el renderizado de cámara, niebla o sprites.

## Regresión

- `tests/game_turn_test.c` cubre contador de turnos, supervivientes, motivos de victoria/empate, éxito/fallo de supervivencia y rechazo de comandos tras el fin.
- `tests/game_screens_test.c` comprueba victoria, derrota, empate, motivos, progreso, contadores, botones A/B/START y que el texto no invade las columnas reservadas del HUD.
- `tests/mission_result_test.c` cubre la prioridad de B/START si se detectan ambas opciones.
- `tests/scenario_menu_test.c`, `tests/movement_glow_test.c` y `tests/session_lifecycle_test.sh` cubren la selección persistente y el ciclo de liberación/reinicio.

Ejecuta `tools/audit_host.sh` después de editar el flujo de resultados. La prueba de pantalla utiliza dobles host para las llamadas VDP y no sustituye la comprobación visual de una ROM SGDK en BlastEm.
