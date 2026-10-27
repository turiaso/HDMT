# Corrección: actualización dinámica de la niebla de guerra

## Cambio

`src/main.c` ahora comprueba `FogOfWar_update()` en cada fotograma, en lugar de depender exclusivamente de `fogUpdatePending` para invocarlo. Esto evita que los cambios de posición lógica de unidades (incluidos cambios originados por la IA u otras ramas del bucle) queden sin detectar hasta una actualización posterior.

`FogOfWar_update()` mantiene la comparación de snapshots: si no cambian las posiciones, equipos, estado de vida o alcance visual, no procesa regiones ni provoca redibujado. Cuando una unidad cambia de casilla, la actualización incremental despeja su nueva zona visible, conserva como explorado el terreno que deja atrás y respeta la línea de visión y los obstáculos.

## Validación

Ejecutado `tests/run_tests.sh`: las 23 comprobaciones disponibles terminaron correctamente, incluidas las pruebas de niebla incremental, oclusión, exploración persistente, ocultación de enemigos, integración de `main.c` y simulación de turno de IA con 60 unidades.

No se ha validado visualmente en BlastEm en esta sesión; debe comprobarse la apariencia en movimiento y al desplazar la cámara.
