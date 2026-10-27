# Corrección: niebla del rango de movimiento tras completar una ruta

## Síntoma

Al terminar de moverse una unidad azul quedaban casillas con niebla dentro del rango de movimiento que esa unidad tiene desde su nueva posición.

## Causa

La exploración de movimiento estaba usando `movementPoints`, es decir, los puntos que quedan después de pagar el coste de cada casilla recorrida. Si una ruta consumía todos los puntos, `revealPlayerMovementAreas()` omitía la unidad (`movementPoints == 0`) y no exploraba el área a partir de su destino. Así, el resultado dependía accidentalmente de cuánto movimiento se hubiera gastado al llegar.

## Corrección

- La exploración persistente del terreno usa ahora `maxMovementPoints`, el rango completo de la unidad, centrado en su posición lógica actual.
- Al moverse una unidad, el refresco de niebla detecta su nueva posición y recalcula su zona de exploración desde esa celda, aunque `movementPoints` haya quedado en cero.
- La selección de unidades y las reglas reales de movimiento siguen usando los puntos restantes (`movementPoints`). Esta corrección no regala puntos ni permite realizar rutas que no sean legales.
- La exploración de casillas sigue siendo distinta de la visión táctica: una casilla puede quedar explorada y sin niebla opaca sin revelar una unidad enemiga que no esté dentro de la línea de visión.
- Los cambios en la cantidad máxima de movimiento invalidan/recalculan el rango de exploración; gastar puntos en el turno, por sí solo, ya no dispara un cálculo innecesario.

## Prueba de regresión

Se añadió una prueba en mapa abierto que inicia una unidad de infantería en `(2,4)`, la desplaza lógicamente hasta `(8,4)` y deja sus puntos actuales en cero. Tras `FogOfWar_update()`, la casilla `(14,4)` —alcanzable con el rango máximo desde la nueva posición, pero fuera del rango inicial y fuera de la visión táctica— debe estar explorada y no visible.

## Validación

- `tests/run_tests.sh`: 23 comprobaciones superadas.
- `tests/run_tests_sanitized.sh`: 23 comprobaciones superadas con ASan/UBSan cuando están disponibles.
- No se ha compilado una ROM SGDK ni se ha hecho una verificación visual en BlastEm en este entorno.
