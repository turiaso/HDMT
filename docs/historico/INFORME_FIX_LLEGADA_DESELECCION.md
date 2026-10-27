# Corrección: pausa al llegar al destino y selección residual

## Diagnóstico del flujo de llegada

El parón breve coincidía con el final de la orden porque el controlador detenía la animación pero mantenía seleccionada la unidad. En el mismo fotograma, `updateMovementRangeVisuals()` interpretaba que la unidad ya no se movía, volvía a activar el alcance y ejecutaba `MovementGlow_redraw()`. Esa función llama a `MAP_scrollToEx(..., TRUE)`, que fuerza el redibujado del área visible del mapa. Así, el alcance se recalculaba y todo el tilemap visible se repintaba justo en la llegada, una tarea evitable que encaja con el tirón descrito.

## Cambios aplicados

1. **Deselección automática al terminar una orden.** Cuando se alcanza el último paso y `UnitController_update()` devuelve `UNIT_ACTION_MOVED`, el controlador limpia el movimiento, la ruta pendiente, la vista previa y la selección. No hace falta pulsar A para quitar la selección.
2. **Sin redibujado de alcance al final de la ruta.** Mientras la unidad se mueve, el alcance ya está desactivado. Al llegar, como ya no queda unidad seleccionada, el estado de `MovementRangeCache` sigue desactivado y se evita reactivar el glow y forzar un redibujado completo en el último fotograma.
3. **HUD consistente en la última casilla.** `main.c` conserva el puntero al registro de la unidad que se estaba moviendo antes de actualizar el controlador y compara las coordenadas a través de ese registro. Así se refresca también el HUD de celda del último paso, aunque la unidad se haya deseleccionado durante la actualización.
4. **Captura de objetivo.** `UnitController_haltMovement()` limpia también la selección al finalizar la partida, tanto si la ruta terminó justo en el objetivo como si fue interrumpida al atravesarlo.
5. **Interrupciones no exitosas.** Si se bloquea el siguiente tramo, la unidad se detiene centrada y conserva la selección para que el jugador pueda preparar otra orden; no se confunde un bloqueo con una llegada correcta.

## Regresiones cubiertas

- Ruta completada: la unidad queda centrada y ocupando su casilla, la selección pasa a `NULL` y la máscara de alcance queda vacía.
- Puntos agotados: tras llegar, la unidad queda deseleccionada; solo se puede comprobar su falta de puntos seleccionándola de nuevo.
- Captura de objetivo durante una ruta: se detiene el movimiento, termina la misión y la selección queda limpia.
- Bloqueo a mitad de ruta: se conserva la selección para permitir replantear el movimiento.

## Pruebas

- `sh tests/run_tests.sh`: 15 secciones más comprobaciones estáticas de las transferencias de paleta, superadas con GCC.
- Compilación/análisis estático con `gcc -fanalyzer` sobre los módulos modificados, sin diagnósticos.
- Comprobación de formato: sin espacios finales ni líneas de código superiores a 100 columnas en los archivos tocados.
- La suite se ejecuta también desde el ZIP extraído de nuevo.
- No se usó Clang.

No está disponible una compilación real de SGDK en este entorno (`GDK` no está definido), así que falta verificar el efecto visual en la ROM con SGDK/BlastEm. El diagnóstico del redibujado se fundamenta en el flujo exacto de código; el comportamiento final de hardware todavía necesita esa validación visual.
