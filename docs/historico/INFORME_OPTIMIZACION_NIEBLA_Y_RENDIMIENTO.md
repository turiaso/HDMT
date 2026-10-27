# Corrección de niebla y optimización del recálculo de movimiento

## Causa del bloqueo

El cálculo de alcance inicial recorría las casillas que el pathfinder consideraba alcanzables, pero volvía a descartarlas si no había línea de visión. Eso dejaba como `UNSEEN` casillas que sí eran destinos legales de movimiento; la interfaz rechaza destinos no explorados. También debía combinarse la cobertura de todas las unidades azules sin revelar enemigos ocultos.

## Corrección funcional

- Se explora la unión del alcance real de movimiento de las unidades azules, respetando costes, terreno y ocupación.
- Las casillas alcanzables sin línea de visión se marcan como `EXPLORED`, no `VISIBLE`: se puede planificar/mover allí sin revelar enemigos ocultos.
- Cuando una unidad deja libre una casilla, se recalculan las aliadas cuyo presupuesto de movimiento podría permitirles llegar a ella. Los cambios simultáneos de topología y los cambios de equipo observador usan el cálculo completo por seguridad.
- La exploración anterior persiste y la visión táctica mantiene prioridad.

## Optimización de rendimiento

El recálculo previo ejecutaba el pathfinder para todas las unidades azules después de cada orden de movimiento, aunque solo se hubiera movido una. Ahora se acumulan unidades candidatas durante la animación y se procesa el conjunto al acabar:

- La unidad que se ha movido se recalcula en su posición final.
- Las aliadas que podrían beneficiarse de cada casilla que quedó libre se incluyen usando la distancia Manhattan como cota conservadora (cada paso cuesta al menos 1).
- Si solo cambia el presupuesto de movimiento, se recalculan las unidades cuyo presupuesto cambió, no todo el ejército.
- Si cambian varias posiciones/ocupaciones simultáneamente, se conserva el recálculo global como fallback para no perder casillas válidas.
- Los cambios exclusivos de alcance de visión no disparan el pathfinder de movimiento.

Así una orden normal deja de hacer hasta 30 búsquedas ponderadas de movimiento y pasa a hacer solo las búsquedas de las unidades afectadas; el caso global se mantiene para cambios simultáneos que lo requieren.

## Pruebas

`tests/run_tests.sh` y `tests/run_tests_sanitized.sh` completan las 23 comprobaciones. Incluyen niebla/visión, varias unidades aliadas, casillas alcanzables detrás de obstáculos, apertura de pasillos, cambios de presupuesto, ocultación de enemigos, rutas sobre 20.000 mapas aleatorios y ASan/UBSan.

## Límite de validación

La validación automatizada se ha ejecutado en host con GCC. No se ha compilado una ROM con SGDK ni se ha comprobado visualmente en BlastEm en esta sesión.
