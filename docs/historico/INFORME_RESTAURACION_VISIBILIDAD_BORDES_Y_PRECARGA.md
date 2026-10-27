# Restauración de visibilidad en bordes y precarga anticipada

## Síntoma

La corrección anterior exigía que el sprite completo cupiese dentro del rectángulo lógico alineado a celdas (`VIEW_PIXELS_W/H`). Una unidad que todavía intersectaba parte del viewport podía quedar oculta. Esto producía una separación visual entre el terreno que se veía en el borde y el sprite que debía representarlo.

## Corrección

- Se restablece la regla de intersección previa: basta con que el sprite se cruce con el viewport lógico para mostrarse; no es necesario que sus 16x16 píxeles quepan enteros.
- Se mantiene separada la zona de precarga, que se extiende una celda alrededor del viewport. Los sprites de esa zona se crean y conservan, pero permanecen ocultos hasta entrar en el viewport.
- Se refresca también la visibilidad tras ajustar una unidad al centro exacto de su celda al finalizar una animación.
- Se conservan las máscaras originales, la niebla real por casilla, el índice espacial y el límite de sprites de la SAT.

## Regresión añadida

`tests/unit_view_test.c` verifica que:

1. Un sprite que intersecta parcialmente el borde lógico se mantiene visible.
2. Una unidad situada fuera del viewport se precarga, pero permanece oculta.
3. Cuando la cámara se desplaza y la unidad entra en el viewport, el sprite existente se hace visible sin crearse otra instancia.

## Verificación ejecutada

- `sh tests/run_tests.sh`: las 23 suites y las pruebas adicionales pasan.
- `sh tests/run_tests_sanitized.sh`: las 23 suites y las pruebas adicionales pasan bajo ASan/UBSan cuando están disponibles.
- GCC `-fanalyzer` en todos los módulos: correcto.
- No se ha compilado una ROM con SGDK ni validado aún esta revisión visualmente en BlastEm. Esa comprobación sigue siendo necesaria para confirmar el resultado gráfico final en ejecución real.
