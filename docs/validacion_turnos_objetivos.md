# Validación del progreso de objetivos por turnos

**Estado:** validación funcional en host superada. La ROM SGDK y el comportamiento visual en BlastEm siguen pendientes.

## Reglas comprobadas

- `MISSION_HOLD_POINT` no avanza al llamar repetidamente a `GameTurn_checkObjective()`; esa función verifica el estado/control, pero no representa el cierre de un turno.
- La racha progresa únicamente cuando termina el turno del equipo que ocupa el punto.
- Los turnos del rival no incrementan el contador.
- Consultar el objetivo varias veces antes del cierre de turno no puede contar la misma posesión varias veces.
- Los escenarios reales de puente se verifican con ambos equipos como ocupantes, y ambos ganan exactamente al completar sus tres cierres de turno.
- Cuando la misión termina, posteriores intentos de cerrar turnos o consultar el objetivo no repiten el resultado ni cambian la cuenta final.

## Prueba ejecutable

`tests/scenario_test.c`, en `validateScenarioMissionFlow()`, usa cada definición real de escenario y ambos equipos. Antes de cada cierre de turno llama cuatro veces a `GameTurn_checkObjective()` y exige que el progreso permanezca intacto; después verifica el avance esperado. También comprueba que el progreso final no puede alterarse tras la victoria.

Ejecutar desde la raíz:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
```

La comprobación host cubre las reglas de estado y turnos, pero no sustituye compilar una ROM con SGDK y comprobarla en BlastEm.
