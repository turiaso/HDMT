# Feedback temporal de acciones del HUD

## Objetivo

El renglón de estado se utiliza tanto para previsualizaciones (ruta, ataque o reparación) como para informar del resultado de una acción. Antes, la previsualización del siguiente fotograma podía reemplazar inmediatamente mensajes como el daño causado o la razón por la que una orden no era válida.

## Contrato de la API

- `CellHud_setActionFeedback()` muestra el resultado de una acción y lo conserva durante `HUD_ACTION_FEEDBACK_FRAMES` fotogramas.
- `CellHud_setContextStatus()` actualiza información derivada del cursor, pero no pisa el resultado mientras esté activo.
- `CellHud_setUnitStatus()` se reserva para cambios de estado prioritarios (por ejemplo, transición de equipo o inicio de la vista de la IA) y cancela el feedback anterior.
- `CellHud_tick()` avanza el temporizador una vez por fotograma de juego.
- Al mover el cursor a otra casilla, `CellHud_update()` libera inmediatamente el feedback para mostrar la nueva previsión.

La duración es deliberadamente corta; no crea colas ni asigna memoria dinámica. Los textos siguen usando la caché de línea y no provocan escrituras repetidas si no cambian.

## Integración

`main.c` utiliza feedback persistente para resultados y rechazos de movimiento, combate y reparación, y estado contextual para rutas y previsiones. Si se añade un mensaje nuevo, hay que decidir explícitamente si representa un resultado (feedback) o una lectura actual del cursor (contexto).

## Pruebas

`tests/cell_hud_test.c` verifica que una previsión no sobrescribe un resultado durante el plazo configurado, que el contexto vuelve a mostrarse al expirar y que mover el cursor libera el mensaje antes de tiempo. `tests/run_tests.sh` verifica además que el temporizador está conectado al bucle y que la función de previsualización no usa la API prioritaria.
