# Diagnóstico: unidad que parece congelada y CRAM dots

## Hallazgo 1 — causa confirmada de los CRAM dots

`MovementGlow_update()` se ejecuta desde el bucle principal, antes de `SYS_doVBlankProcess()`, y estaba llamando a `PAL_setPalette(PAL1, glowColors, DMA)`. En SGDK, `DMA` es una transferencia inmediata. Al escribir CRAM durante el área activa de imagen pueden aparecer los artefactos conocidos como *CRAM dots*. El código actualiza PAL1 cada cuatro fotogramas, por lo que el defecto se repetía durante la partida.

### Corrección aplicada

- Cambiadas todas las llamadas de `PAL_setPalette` de `main.c`, `team.c` y `movement_glow.c` a `DMA_QUEUE_COPY`, incluida la animación repetida del glow.
- El pulso no emite transferencias de paleta cuando la máscara no contiene casillas alcanzables.
- Al terminar la misión ya no se sigue animando PAL1.
- Añadida una prueba de transferencia a `movement_glow_test.c` y una guardia estática para evitar reintroducir `PAL_setPalette(..., DMA)` en el código del juego.

SGDK documenta `DMA` como transferencia inmediata y `DMA_QUEUE_COPY` como transferencia copiada a la cola para el siguiente VBlank; las notas de SGDK también documentan correcciones de CRAM dots relacionadas con asegurar las actualizaciones de paleta en VBlank. Referencias: https://github.com/Stephane-D/SGDK/blob/master/inc/dma.h y https://github.com/Stephane-D/SGDK/releases.

## Hallazgo 2 — una posible causa del aparente congelamiento

El objetivo de la misión está en `(10, 10)`. `main.c` comprueba el objetivo después de cada actualización de movimiento. Si una ruta pasa por esa casilla en dirección a un destino más lejano, `GameTurn_checkObjective()` considera la partida ganada, `finishForTeam()` detiene la ruta y el bucle deja de aceptar controles. Esto es una condición de victoria, no un bloqueo infinito, pero puede parecer una congelación si la ruta atraviesa el objetivo accidentalmente.

La prueba `testCaptureDuringRouteFinishesMatchAtObjective()` reproduce ese caso. El HUD ahora indica explícitamente `VICTORIA: OBJETIVO CAPTURADO` y el resultado del encuentro.

Esta explicación es condicional: sin observar la coordenada exacta donde ocurrió la parada en la ROM no se puede afirmar que el movimiento reportado haya acabado precisamente en el objetivo.

## Comprobaciones

- `sh tests/run_tests.sh`: 15 secciones y guardia adicional contra DMA inmediato de paletas.
- GCC `-fanalyzer`: sin diagnósticos en `movement_glow.c`, `team.c`, `unit_controller.c`, `game_turn.c` y `main.c`.
- La suite se repetirá desde una extracción limpia del ZIP.
- No se ha usado Clang.
- No se ha compilado ni ejecutado una ROM con la instalación real de SGDK/BlastEm en este entorno; la transferencia CRAM real y el comportamiento visual deben comprobarse allí.

## Hallazgo adicional — tirón al completar una orden

En el flujo de llegada, `UnitController_update()` detenía la animación pero dejaba la unidad seleccionada. A continuación, `updateMovementRangeVisuals()` volvía a habilitar el alcance y `MovementGlow_redraw()` forzaba el repintado del mapa visible mediante `MAP_scrollToEx(..., TRUE)`. Esa reconstrucción ocurría precisamente al llegar al destino y podía producir el tirón de un fotograma. La revisión nueva deselecciona automáticamente la unidad al completar la orden; así el alcance sigue apagado tras el movimiento y no se solicita ese redibujado completo final. El HUD usa el registro de la unidad que se estaba moviendo para refrescar también la última casilla. Ver `INFORME_FIX_LLEGADA_DESELECCION.md`.
