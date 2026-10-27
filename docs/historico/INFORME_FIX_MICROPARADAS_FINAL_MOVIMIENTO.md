# Corrección de la pausa tras terminar un recorrido

## Causa

Al terminar una ruta, `FogOfWar_update()` procesaba de forma síncrona todas las unidades aliadas marcadas como posibles beneficiarias de una casilla que había quedado libre. Cada candidata ejecutaba `Pathfinder_buildReachability()` y recorría su rectángulo de alcance. En un grupo de hasta 30 unidades, varias búsquedas quedaban concentradas en el primer fotograma posterior a la llegada, provocando una pausa perceptible.

Además, cada una de las llamadas posteriores usadas para terminar ese trabajo volvía a copiar hasta 60 snapshots y reconstruía el índice de unidades por equipo, aunque no hubiera cambiado ninguna posición.

## Solución

- El alcance de la unidad que acaba de moverse tiene prioridad y se calcula en la primera actualización posterior a la llegada, manteniendo la revelación de todo su rango máximo aunque haya consumido sus puntos.
- Los alcances de otras unidades aliadas afectadas por las casillas liberadas se procesan por tandas de una unidad por actualización. La máscara persiste entre fotogramas y no se descartan candidatas.
- `FogOfWar_hasPendingMovementExploration()` permite al bucle principal programar los siguientes fragmentos solo mientras quede trabajo.
- Se impide que el bloque de niebla de inicio y el bloque de final de `updateGameFrame()` procesen dos fragmentos en el mismo fotograma.
- Si no ha cambiado ninguna unidad, se evita copiar snapshots y reconstruir el índice del equipo en cada fragmento.
- Los cambios de visibilidad se siguen renderizando en el fotograma en el que se producen; las casillas exploradas previamente permanecen exploradas y la cobertura real de visión sigue determinando si los enemigos son visibles.

## Pruebas

- `tests/run_tests.sh`: las 23 suites pasan.
- `tests/run_tests_sanitized.sh`: las 23 suites pasan con ASan/UBSan.
- Se añadió `testMovementExplorationIsChunkedAfterRoute`: verifica que la unidad recién movida se procesa primero, que las demás candidatas no se procesan todas de golpe y que el trabajo pendiente se completa en las actualizaciones siguientes sin alterar la ocupación.

No se ha compilado una ROM ni medido el contador de FPS en BlastEm en este entorno. La reducción del trabajo por fotograma está verificada en código y mediante pruebas, pero la fluidez final debe comprobarse en la ROM.
