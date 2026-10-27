# HUD: objetivo y turno en filas separadas

## Problema

La fila del turno mezclaba equipo activo, objetivo, progreso temporal y puntos de movimiento. En pantallas estrechas, el texto podía truncarse y el dato principal de la misión no destacaba.

## Cambio

- La fila de turno presenta `Tn EQUIPO` y añade `MP actual/máximo` solo cuando hay una unidad seleccionada.
- La fila siguiente muestra el objetivo en lenguaje directo:
  - `OBJ: CAPTURAR X21,Y21`
  - `OBJ: PUENTE - AZUL 1/3`
  - `OBJ: PUENTE - SIN CONTROL 0/3`
  - `OBJ: RESISTIR 2/4`
  - `OBJ: ELIMINAR TODO`
- Al terminar la misión, la fila del objetivo se limpia para no dejar un contador antiguo bajo el resultado.
- Se conserva una caché por fila para no escribir en el plano BG_B cuando el texto no cambia.
- `HUD_TEXT_LINES` pasa a 7: cuatro filas de celda y tres filas de estado (unidad, turno y objetivo). El FPS sigue en la fila 0.

## Validación

`tests/cell_hud_test.c` comprueba los cuatro tipos de misión, el puente sin control, el resultado final, la ausencia de escrituras redundantes y el contenido de cada fila. Se ejecutaron:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
sh tools/check_source_quality.sh
python3 tools/check_resources.py
```

Las cuatro comprobaciones pasan en el entorno host. No sustituyen a la revisión visual de la ROM con SGDK/BlastEm, que continúa pendiente.
