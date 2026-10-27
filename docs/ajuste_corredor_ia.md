# Ajuste de selección del corredor de IA

## Cambio

La selección del corredor para misiones de captura/mantenimiento ya no se basa casi exclusivamente en distancia Manhattan. Ahora compara una aproximación normalizada por el presupuesto máximo de movimiento de cada unidad, aplica la distancia como desempate y añade una penalización secundaria moderada por salud perdida.

Se penaliza con fuerza a las unidades que ya agotaron sus puntos de movimiento, para no asignar el papel principal a una unidad que ya no puede desplazarse en ese turno cuando existe otra opción.

## Límites del cálculo

La puntuación es una heurística de asignación de rol, no una ruta estimada exacta. Los costes por terreno, obstáculos y ocupación siguen siendo autoritativos en `Pathfinder` y `UnitController`; este cambio no permite atravesar terreno ni ocupar celdas inválidas.

No se ha añadido una búsqueda de rutas adicional por cada unidad al inicio del turno, para no elevar el coste computacional en el escenario de 120 unidades.

## Regresión

`testObjectiveRunnerAccountsForUnitMobility` pone a una unidad de infantería una casilla más cerca del objetivo y a un explorador un poco más lejos. Dado su mayor presupuesto de movimiento, el explorador debe ser elegido como corredor. La prueba también completa cualquier animación iniciada, y comprueba la asignación de escolta al resto del grupo.

## Validación ejecutada

- `sh tests/run_tests.sh`
- `sh tests/run_tests_sanitized.sh`
- `sh tools/check_source_quality.sh`
- `python3 tools/check_resources.py`

La ROM no se ha compilado en este entorno: SGDK y BlastEm siguen siendo necesarios para la validación visual y de rendimiento real.
