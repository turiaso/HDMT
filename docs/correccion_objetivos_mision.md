# Corrección del sistema de objetivos y resultados

**Estado:** implementado y probado en host. La compilación de ROM con SGDK y la prueba visual en BlastEm siguen pendientes.

## Reglas resultantes

- **Empate por falta total de unidades:** si Azul y Rojo tienen cero unidades vivas, el resultado es `GAME_OUTCOME_DRAW`, independientemente del tipo de misión. La pantalla final y el HUD muestran `EMPATE`.
- **Eliminar todas:** si queda un solo equipo con unidades vivas, gana ese equipo; si no queda ninguno, hay empate.
- **Sobrevivir turnos:** el equipo objetivo pierde si se queda sin unidades; gana si el rival se queda sin unidades. Si ambos se quedan sin unidades, prevalece el empate global.
- **Capturar y mantener:** se conserva el diseño existente según el cual eliminar al rival no sustituye al objetivo. Sin unidades en ambos bandos se declara empate porque ya no es posible completar la misión.
- **Turnos de objetivo:** un `requiredTurns` de cero sigue significando un turno.

## Orden de cierre de turno

`GameTurn_endTurn()` ahora despacha `CELL_EVENT_TURN_END` antes de comprobar el estado final y avanzar el contador de objetivos temporizados. Los cambios de estado hechos por dichos eventos (por ejemplo, eliminar al ocupante del objetivo) se reflejan antes de conceder la victoria.

`GameTurn.activeTurnEndDispatched` hace que cada cierre de turno se emita como máximo una vez. Si una misión termina por captura, eliminación o progreso temporizado, `finishWithOutcome()` reutiliza el mismo cierre en lugar de despachar un segundo evento. También se bloquean reintentos que pudieran duplicar el progreso si no puede abrirse el turno siguiente; un fallo de transición irrecuperable termina en empate en vez de dejar el juego activo pero atascado.

## Pruebas nuevas

Se ampliaron `tests/game_turn_test.c` con comprobaciones de:

- ambos equipos vacíos para los cuatro tipos de misión;
- un único equipo superviviente en `MISSION_ELIMINATE_ALL` y `MISSION_SURVIVE_TURNS`;
- conservación de la regla de objetivo primario en `MISSION_CAPTURE_POINT`;
- efecto `TURN_END` que elimina al ocupante antes de evaluar `MISSION_HOLD_POINT`;
- abandono del objetivo y reinicio de la racha;
- eliminación en combate del ocupante y posterior captura por el rival;
- `requiredTurns == 0` y un único despacho `TURN_END` cuando el objetivo concluye la misión.
- Integración con los tres escenarios reales: captura inmediata y mantenimiento durante tres finales de turno, para Azul y para Rojo.
- Ruta al objetivo desde todas las unidades de los tres despliegues.

La validación de configuración y conectividad está detallada en [`auditoria_escenarios.md`](auditoria_escenarios.md).

## Validación ejecutada

- `sh tests/run_tests.sh`: superado, 23 grupos y pruebas adicionales.
- `sh tests/run_tests_sanitized.sh`: superado, incluidos ASan/UBSan y estrés de combate con 120 unidades.
- GCC `-fanalyzer`: todos los módulos de `src/` analizados sin errores.

Las pruebas utilizan stubs de SGDK. No sustituyen la compilación con la toolchain real ni la comprobación del resultado visual en BlastEm.
