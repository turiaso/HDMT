# Diagnóstico de rendimiento · edición con 120 unidades

## Objetivo

Preparar una medición reproducible de la última versión (mapa de 40×40, 60 unidades por equipo) sin introducir otra optimización basada en suposiciones. Esta entrega conserva la lógica del juego y añade instrumentación por fase para identificar qué trabajo se realiza cuando cae el contador de FPS.

## Qué se registra

`VDP_showFPS(FALSE, 33, 0)` permanece activo en pantalla y muestra la tasa real estimada por SGDK para la ROM ejecutándose en el modo de vídeo configurado. La instrumentación `PERF_DIAGNOSTICS=1` registra, en muestras aisladas de 120 fotogramas por actividad:

- `IDLE`: juego sin órdenes ni animaciones.
- `PLAYER_SCROLL`: paneo manual con C + cruceta.
- `PLAYER_CURSOR`: movimiento del cursor del jugador.
- `PLAYER_MOVE`: movimiento de una unidad azul.
- `AI_DECISION`: fase de decisión de IA sin movimiento/cámara automática.
- `AI_CAMERA`: reencuadre entre actores rojos.
- `AI_MOVE`: movimiento rojo sin seguimiento de cámara activo.
- `AI_MOVE_CAMERA`: movimiento rojo mientras la cámara automática sigue activa.
- `CAMERA_RETURN`: vuelta de la cámara al encuadre azul.

Cada bloque `PERF` informa llamadas de scroll, suma de píxeles desplazados, actualizaciones de la SAT, refrescos y celdas de niebla, tramos DMA, búsquedas de ruta, nodos procesados, construcciones de alcance, registros de unidades visitados, candidatos de IA, movimientos y ataques. Las muestras se acumulan por fase y no mezclan sus contadores, aunque dos fases se alternen.

## Procedimiento de medición en SGDK/BlastEm

1. Compila este ZIP con la toolchain SGDK configurada y ejecuta la ROM en BlastEm con el modo de vídeo que uses normalmente.
2. En reposo, espera a que aparezca `PERF PHASE IDLE`; apunta el valor de FPS en la pantalla y copia ese bloque del log.
3. Mantén C + una dirección de la cruceta para desplazar la cámara durante varios segundos. Apunta FPS mientras se mueve, no en el instante del mensaje de log, y copia el bloque `PERF PHASE PLAYER_SCROLL`.
4. Ordena un recorrido largo de una unidad azul y registra `PLAYER_MOVE`.
5. Termina turno y deja que Rojo actúe. Registra especialmente `AI_MOVE_CAMERA`, `AI_CAMERA` y `AI_DECISION`, además del FPS durante el peor bajón.

La ventana de 120 fotogramas sirve como cantidad de muestras de la actividad, no necesariamente como 120 frames consecutivos: se acumulan solo frames clasificados en esa fase. En NTSC representa aproximadamente dos segundos de actividad; en PAL aproximadamente 2,4 segundos. Las impresiones `KLog` pueden provocar una breve perturbación en el instante del reporte, así que el FPS justo en ese instante no debe tomarse como el valor sostenido.

## Interpretación

- Muchos `scroll calls` y `scroll pixels`, con SAT moderada y `unit records` bajo, apunta a coste relacionado con scroll/actualización del mapa.
- Mucho `SAT updates` indica que se reconstruye la tabla de sprites en demasiados frames.
- Muchos `unit records` en scroll indican que se invalida o recorre la caché de unidades.
- `fog cells`, `fog row DMA`, `routes`, `path nodes` o `range builds` altos durante `IDLE` o `PLAYER_SCROLL` revelarían trabajo inesperado fuera de las acciones de juego.
- `AI_CANDIDATES`, nodos de ruta y construcciones de alcance altos durante `AI_DECISION` ayudan a distinguir el coste de decidir frente al de animar/correr la cámara.

## Resultado verificable en este entorno

- Las suites normales y sanitizadas comprueban que la instrumentación compila y que cada bloque de fase acumula exactamente 120 frames y conserva los contadores correctos.
- No se ha compilado una ROM con el toolchain SGDK ni se ha ejecutado BlastEm en este entorno. Por eso este documento **no declara todavía ningún FPS medido**. El siguiente paso es utilizar el registro y el FPS de la ROM, y decidir una optimización a partir de esos datos.

## Desactivar después de medir

Cambiar `#define PERF_DIAGNOSTICS 1` por `0` en `src/game_config.h`. La medición por fases es una herramienta de diagnóstico, no un cambio necesario para la versión final de juego.
