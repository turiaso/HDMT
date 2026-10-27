# Informe: sincronización entre movimiento y mapa

## Cambios realizados

- La ruta valida cada siguiente paso **antes de comenzar a animar ese tramo**. Si la unidad ya no ocupa su casilla o la siguiente casilla está ocupada, es intransitable o no existe, se detiene antes de avanzar visualmente hacia ella.
- La lógica de movimiento se ha reorganizado para calcular dirección/destino, validar el comienzo del tramo y confirmar la llegada por separado.
- Las coordenadas lógicas de la unidad y la ocupación de las celdas se actualizan al completar el tramo. El render sigue usando la posición lógica más los píxeles interpolados; al alcanzar el límite de 24 px, el siguiente estado queda exactamente en el origen de la nueva casilla (`x * CELL_PIXELS`, `y * CELL_PIXELS`) con el contador del tramo a cero.
- Se comprueba que la casilla origen contiene a la unidad que se está moviendo. La asignación del destino no debe sobrescribir una ocupación existente; si la asignación falla, se intenta restaurar el origen.
- El cierre de movimientos interrumpidos se centraliza en `stopMovement()`, que limpia ruta, paso, píxeles pendientes y previsualización, pero conserva la unidad seleccionada para recalcular otro destino.
- Las órdenes incompatibles siguen bloqueadas durante la animación; también se comprueba con una prueba que intentar seleccionar otra unidad o cancelar no cambia el avance visual.
- Se añadieron pruebas para el primer desplazamiento interpolado, la llegada exacta al centro y un obstáculo que aparece en el siguiente paso entre dos tramos. Se comprueba que no se envían eventos de salida/entrada para el paso rechazado y que la unidad conserva la casilla que ocupa.
- Actualizados `docs/fase_4.md` y `README.md`. En la revisión posterior se refactorizó también el despacho de eventos compartidos; el informe consolidado es `INFORME_FASE4_REFACTORIZACION_EVENTOS.md`. La inicialización de la fuente SGDK y la distribución de VRAM del HUD no se han tocado.

## Verificación ejecutada

Se ejecutó `sh tests/run_tests.sh` usando GCC. Pasaron los 11 apartados de la suite: eventos, cámara/cursor, mapa, pathfinding, controlador de unidades, vista de sprites, reserva de VRAM, contenido del HUD, orden de inclusión, sintaxis/integración y la ejecución con los sanitizadores admitidos por ese GCC. No se ejecutó Clang.

Estas son pruebas host. No sustituyen una compilación real con SGDK ni una comprobación visual de la ROM en BlastEm.

## Comportamiento resultante

1. La unidad conserva su ocupación lógica de origen mientras recorre visualmente un tramo.
2. Al completar los 24 píxeles, la unidad queda centrada en la nueva casilla y el ocupante lógico cambia en ese límite.
3. Antes del tramo siguiente se comprueba de nuevo la ocupación/transitabilidad. Si el paso está bloqueado, no comienza una nueva interpolación, la unidad se queda centrada y puede recibir otra orden tras recalcular la ruta.
4. Mientras se ejecuta el movimiento no se procesan entradas de selección o cancelación.

Esta validación presupone el modelo actual del bucle de juego: no procesa órdenes nuevas durante un tramo, y los eventos de casilla se ejecutan al terminar un tramo. Si futuros subsistemas modifican ocupaciones de forma asíncrona en mitad del tramo, habrá que definir una política adicional para esos cambios.
