# Corrección del parón en la última casilla

## Síntoma

Durante el recorrido subcelda el sprite avanzaba correctamente, pero al alcanzar el destino podía quedarse visualmente unos píxeles antes del centro y después completar ese pequeño desplazamiento.

## Causa

El controlador sí completaba la interpolación y actualizaba la posición lógica. Sin embargo, en ese mismo fotograma `updateGameFrame()` detectaba el fin del movimiento y ejecutaba `refreshFogForActiveTeam()` antes de llamar a `UnitView_update()` y presentar la tabla de sprites. El refresco de visión/niebla podía consumir el tiempo del fotograma entre el último paso de interpolación y la actualización visual que centra el sprite.

## Corrección

- Se detecta si el controlador ha terminado de moverse en el fotograma actual.
- Ese fotograma termina de actualizar y presentar el sprite en el centro exacto de su destino, sin ejecutar primero el refresco diferido de niebla.
- La niebla y el HUD pendientes se actualizan al comienzo del siguiente fotograma, antes de procesar una nueva orden del jugador. Esto evita aceptar órdenes con una visión obsoleta.
- No se cambia la velocidad de interpolación ni la posición lógica; se mantiene el snap exacto a la celda.
- No se vuelve a calcular la niebla en cada fotograma de movimiento.

## Verificación

Se ejecutó `tests/run_tests.sh`: las 23 comprobaciones pasaron. Las pruebas existentes incluyen continuidad píxel a píxel, destino centrado, cancelación/obstáculos y sincronización de ocupación. No se ejecutó la ROM en BlastEm en este entorno, así que la fluidez visual final debe confirmarse con el contador FPS ya integrado.
