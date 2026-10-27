# Línea de visión y oclusión de terreno

## Comportamiento

La visión y los disparos ahora respetan los obstáculos intermedios del mapa lógico:

- **Bosque:** transitable con coste 2, defensa +1 y bloquea visión/disparo si está entre origen y destino.
- **Piedras:** transitable con coste 2, defensa +2 y bloquea visión/disparo si están entre origen y destino.
- **Camino:** coste 1, sin cobertura ni oclusión.
- **Agua:** intransitable, pero no bloquea la vista por sí sola.

La propia casilla objetivo puede ser bosque o piedras y aun así resulta visible/atacable desde una línea despejada; solo cuentan las casillas intermedias. El alcance de visión sigue siendo Chebyshev y el alcance de ataque sigue siendo Manhattan. No se han cambiado los valores de alcance por unidad.

## Implementación

`MapData_hasLineOfSight()` en `src/map_data.c` usa un recorrido entero y simétrico de la rejilla, sin coma flotante ni búfer temporal. Si el rayo pasa exactamente por una esquina, avanza en diagonal: las dos celdas laterales que solo toca por el borde no bloquean; sí bloquea una celda opaca que el recorrido atraviesa por su interior. Esta regla conserva los corredores diagonales y se fija mediante pruebas explícitas.

`FogOfWar` aplica esa comprobación al revelar terreno, reconstruir visibilidad de equipo y mantener la optimización incremental para una unidad que se mueve. `Combat_predictUnits()` rechaza disparos con `COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT`; el HUD de objetivo y el resultado del ataque muestran `OBSTACULO BLOQUEA TIRO`. La represalia también requiere línea despejada. La IA solo evalúa blancos autorizados por la visión de Rojo, y su estimación de peligro descarta enemigos que no puedan disparar desde su posición debido a oclusión.

## Pruebas

- `tests/map_data_test.c`: línea despejada, objetivo opaco visible, bosque/roca como obstáculos intermedios, fuera de mapa, simetría para todos los pares y regla explícita al rozar una esquina.
- `tests/fog_of_war_test.c`: la zona tras una roca permanece desconocida; al mover la unidad a un ángulo despejado se revela y al volver queda explorada pero fuera de visión.
- `tests/combat_test.c`: un tanque a alcance 2 no puede atacar a través de roca ni bosque; al despejar el segmento puede atacar incluso si el objetivo ocupa bosque (que aporta cobertura).
- `tests/enemy_ai_test.c`: si ve un blanco dentro de alcance pero una roca tapa el disparo, la IA se recoloca a una casilla que despeja el tiro y ataca en su siguiente decisión.

Validación host de esta edición: `sh tests/run_tests.sh` y `sh tests/run_tests_sanitized.sh` pasan con 23/23 grupos. GCC `-fanalyzer -Wall -Wextra -Werror -Wformat=2` no da diagnósticos en los módulos C. La simulación host no sustituye compilar la ROM con SGDK ni inspeccionar la regla en BlastEm; esa prueba visual sigue pendiente.
