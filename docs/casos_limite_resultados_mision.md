# Casos límite: bajas, control de objetivos y resultado final

**Estado:** pruebas host superadas. La compilación con SGDK real y la ejecución en BlastEm siguen pendientes.

## Corrección aplicada

`GameTurn_init()` evalúa ahora las condiciones de misión al terminar los eventos `TURN_START`, pero impide emitir un `CELL_EVENT_TURN_END` ficticio si la partida ya está resuelta en ese momento. Antes, una misión de eliminación que empezara con un equipo vacío podía cerrar un turno que el jugador todavía no había jugado. Si la misión continúa activa, el turno se deja abierto y `TURN_END` vuelve a funcionar con normalidad.

## Reglas verificadas

- Si ambos equipos se quedan sin unidades, el resultado es empate, incluso cuando las bajas ocurren en el mismo evento de cierre de turno.
- Las bajas causadas por `CELL_EVENT_TURN_END` se aplican antes de resolver el resultado de ese cierre.
- El cierre de un turno emite un solo `CELL_EVENT_TURN_END`; el empate posterior no vuelve a despacharlo.
- Cuando un equipo se queda sin unidades, `MISSION_CAPTURE_POINT` y `MISSION_HOLD_POINT` siguen exigiendo su objetivo. En `MISSION_HOLD_POINT`, el superviviente tiene que ocupar el punto y completar la cantidad de finales de turno configurada.
- Un rival adyacente no disputa automáticamente una casilla ocupada. El control de `MISSION_HOLD_POINT` cambia cuando la abandona el ocupante o este es eliminado y el rival ocupa la casilla.
- Una casilla ocupada no puede recibir otra unidad, y una baja del ocupante libera la ocupación y reinicia la racha de mantenimiento.
- Las misiones `MISSION_ELIMINATE_ALL` y `MISSION_SURVIVE_TURNS` resuelven la ausencia de un bando según sus reglas; si no queda ninguna unidad de ninguno, prevalece el empate.

## Pruebas añadidas

`tests/game_turn_test.c` cubre:

- resultado inmediato al iniciar una misión con un equipo vacío, sin evento `TURN_END` prematuro;
- empate cuando los dos últimos supervivientes son eliminados por efectos de `TURN_END`;
- liberación de ambas casillas de ocupación después de las bajas;
- superviviente único que todavía debe capturar o mantener el objetivo;
- enemigo adyacente que no cambia por sí solo el equipo que controla el punto;
- rechazo de la orden de entrar en una casilla ocupada.

## Ejecutar las pruebas

Desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
sh tools/check_source_quality.sh
```

Estas pruebas verifican las reglas de estado en el entorno host. No sustituyen la compilación de una ROM con SGDK ni su validación visual en BlastEm.
