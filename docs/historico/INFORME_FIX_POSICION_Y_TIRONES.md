# Corrección de final de movimiento y tirones de rendimiento

## Cambios incluidos

- Al terminar (o abortarse) un movimiento, la posición del sprite se fuerza a las coordenadas de la celda lógica. Esto evita depender solo de la caché de posición de pantalla para el último fotograma.
- La actualización incremental de niebla pasa a limitar el recálculo de visibilidad de sprites a unidades en celdas cuya visibilidad cambió, más los casos en que el estado de una unidad cambió. Las unidades no afectadas mantienen su estado y posición renderizada.
- En un cambio de una única unidad, `FogOfWar` comprueba primero la cobertura desde la nueva posición y evita reevaluar la visibilidad previa de las casillas que siguen visibles. También evita contar/trabajar las casillas no afectadas.
- Al ocultar el alcance de movimiento, el glow retira solo los estilos de las casillas marcadas y refresca las filas visibles afectadas, evitando forzar el redibujado completo del viewport.
- Se conserva la actualización condicional de niebla; no se vuelve a recalcular incondicionalmente cada fotograma.

## Pruebas

Se ejecutaron `tests/run_tests.sh` y `tests/run_tests_sanitized.sh`. Las 23 comprobaciones pasan en ambos recorridos, incluyendo tests de snap final al completar/abortar, cámara y movimiento en caché, actualización incremental de visibilidad, refresco localizado del glow, niebla de guerra, 20.000 mapas aleatorios y simulación del turno de IA con 60 unidades.

## Limitación de validación

No se ha podido compilar una ROM SGDK ni medir FPS en BlastEm en este entorno porque no está disponible la toolchain/emulador. Los cambios están cubiertos por las pruebas del proyecto, pero la mejora real de FPS y la apariencia final deben confirmarse en la ejecución del usuario con el contador de FPS habilitado.
