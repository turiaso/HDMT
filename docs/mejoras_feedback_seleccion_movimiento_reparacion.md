# Ficha de selección y feedback de movimiento/reparación

## Cambios

- La ficha de la unidad seleccionada se identifica con `SEL` y resume sus PV, ataque, defensa, alcance y visión; los ingenieros también muestran alcance y cantidad de reparación.
- Cuando el cursor vuelve a la casilla de origen de la unidad seleccionada, el HUD conserva esta ficha en vez de sustituirla por una ruta trivial de cero pasos.
- Las rutas válidas usan `RUTA N PASOS / COSTE C / MP M`; los destinos sin ruta muestran `SIN RUTA VALIDA`.
- Si la unidad ya consumió la acción compartida, el HUD distingue ataque y reparación realizados del presupuesto de movimiento agotado.
- El objetivo de una reparación válida ve la recuperación prevista y los PV resultantes; al confirmar, el HUD muestra la recuperación real y los PV finales.
- `Combat_predictRepairUnits()` devuelve la recuperación prevista en `healthRestored`, pero sigue sin modificar unidades ni consumir la acción. `Combat_repair()` aplica esa misma cantidad, evitando duplicar el cálculo.

## Pruebas

- `tests/cell_hud_test.c`: formato de ficha seleccionada, mensajes de ruta y reparación prevista/confirmada.
- `tests/combat_test.c`: predicción de recuperación limitada por PV faltantes sin mutar la salud ni la acción del ingeniero.
- Suite funcional, ASan/UBSan, recursos y GCC `-fanalyzer` ejecutados después de los cambios.

La validación host no sustituye una inspección visual de la ROM en SGDK/BlastEm. Esta modificación no cambia la cámara, la niebla de guerra, la precarga de sprites ni los gráficos de las unidades.
