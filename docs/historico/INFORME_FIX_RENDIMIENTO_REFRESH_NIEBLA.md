# Corrección de regresión de rendimiento: refresco de niebla

## Causa identificada

En `src/main.c`, el bucle principal pasó de ejecutar `refreshFogForActiveTeam(&state)` solo cuando `fogUpdatePending` era verdadero a ejecutarlo incondicionalmente en todos los fotogramas. Ese cambio se introdujo al intentar que la visión se actualizara inmediatamente durante movimientos de unidades/IA.

Sin embargo, cada llamada a `FogOfWar_update()` compara hasta 60 snapshots, limpia las marcas dirty anteriores, guarda snapshots y reconstruye el índice de unidades por equipo, aunque no haya cambiado nada. Por tanto, el supuesto camino «barato» seguía haciendo trabajo por fotograma y podía degradar el rendimiento general.

## Corrección aplicada

Se ha restaurado la condición `if (fogUpdatePending) refreshFogForActiveTeam(&state);` manteniendo las nuevas reglas de niebla. Las rutas de cambio de contexto, combate, cambio de turno y movimiento lógico ya marcan `fogUpdatePending`; durante los movimientos se marca al cambiar la posición lógica de la unidad. De este modo, el refresco ocurre ante cambios y no por el mero paso de cada fotograma.

No se han revertido los cambios de cobertura/alcance de movimiento ni las reglas de exploración persistente.

## Validación

- `tests/run_tests.sh`: pasan las 23 comprobaciones disponibles.
- `tests/run_tests_sanitized.sh`: pasan las 23 comprobaciones con ASan/UBSan.
- No se ha medido FPS en una ROM dentro de BlastEm ni se ha ejecutado una compilación SGDK en este entorno; la mejora se basa en eliminar el trabajo redundante identificado, no en una medición de emulador.
