# Auditoría del sistema de objetivos y resultados de misión

**Estado:** los hallazgos de la auditoría inicial fueron corregidos y están cubiertos por pruebas de regresión.

La auditoría inicial detectó tres problemas: tratamiento inconsistente de ambos equipos sin unidades, evaluación del objetivo temporizado antes de los eventos `TURN_END` y falta de pruebas para transiciones de ocupación. La implementación vigente está descrita en [corrección del sistema de objetivos](correccion_objetivos_mision.md).

## Contrato actual

- `MISSION_CAPTURE_POINT`: termina inmediatamente cuando una unidad ocupa el objetivo.
- `MISSION_HOLD_POINT`: gana el equipo que mantiene el punto durante el número configurado de finales de sus propios turnos. Si el punto queda vacío o cambia de equipo, se reinicia la racha.
- `MISSION_ELIMINATE_ALL`: gana el equipo que conserva unidades vivas cuando el rival queda sin ellas.
- `MISSION_SURVIVE_TURNS`: gana el equipo objetivo tras completar los finales de turno propios requeridos, siempre que siga vivo; perder todas sus unidades concede la victoria al rival.
- Si ambos equipos quedan sin unidades, el resultado es empate para los cuatro tipos de misión.
- `CELL_EVENT_TURN_END` se procesa antes de volver a comprobar las condiciones de victoria. Cada cierre se despacha una sola vez.
- `requiredTurns == 0` equivale a un turno para los objetivos temporizados `MISSION_HOLD_POINT` y `MISSION_SURVIVE_TURNS`. No se usa para `MISSION_CAPTURE_POINT`.

## Cobertura

`tests/game_turn_test.c` comprueba casos límite y transiciones, incluidas las unidades eliminadas, la pérdida y recuperación del objetivo, los equipos vacíos y los eventos de cierre. `tests/scenario_test.c` integra las definiciones reales de las tres misiones con el motor de turnos: verifica que todos los despliegues llegan al objetivo y que cada equipo puede ganar según la misión configurada. Consulta [auditoría de las configuraciones de escenario](auditoria_escenarios.md).

## Validación

- `sh tests/run_tests.sh`: superado.
- `sh tests/run_tests_sanitized.sh`: superado con ASan/UBSan, incluido el estrés de combate con 120 unidades.
- GCC `-fanalyzer` en todos los módulos de `src/`: superado.

Estas comprobaciones usan stubs host; no sustituyen una ROM compilada con SGDK ni una ejecución visual en BlastEm.
