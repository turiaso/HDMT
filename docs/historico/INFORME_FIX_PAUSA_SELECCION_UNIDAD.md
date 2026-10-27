# Corrección de la pausa al mostrar el alcance de movimiento

## Causa

Al seleccionar una unidad, `main.c` reconstruía la caché de estilos dos veces (una para el alcance y otra para los objetivos atacables) y forzaba a SGDK a redibujar todo el viewport con `MAP_scrollToEx(..., TRUE)`. La caché cubre el mapa lógico de 40×40 celdas, aunque la selección solo altera el estilo de una parte de él.

## Cambios

- Nuevo `MovementGlow_setMasks()` actualiza conjuntamente el alcance y los objetivos atacables.
- La caché anterior solo se recalcula en celdas que estaban resaltadas o que tienen una marca nueva.
- Las celdas con estilos diferentes se agrupan por tramos horizontales y se parchean con las transferencias de filas existentes.
- El camino normal de selección/desselección ya no fuerza un redraw completo del mapa. Se conserva ese redraw como recuperación si la caché no está inicializada.
- `Combat_buildAttackableMask()` descarta enemigos fuera del alcance Manhattan antes de llamar a la predicción completa, que sigue siendo la validación común de ataque.

## Verificación

Las pruebas host comprueban que una selección parchea solo las celdas que cambian, conserva la prioridad de niebla, ataque y movimiento, y no fuerza un scroll completo en el camino normal.
Las suites normales y ASan/UBSan deben ejecutarse antes de compilar la ROM.

## Límite

No se ha realizado una compilación cruzada SGDK ni una medición de FPS en BlastEm en este entorno.
