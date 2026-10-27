# Informe de optimización: niebla y presentación incremental

## Cambios realizados

1. **Niebla de guerra:** `FogOfWar_update()` distingue cambios que alteran la visión táctica de cambios exclusivos del presupuesto de movimiento. Gastar/restaurar puntos o cambiar `maxMovementPoints` actualiza los snapshots y el alcance móvil cuando corresponda, pero ya no marca regiones de visión sucias ni ejecuta comprobaciones de línea de visión por ese único motivo.
2. **Redibujado:** se conserva el sistema incremental existente: las celdas de niebla se agrupan en tramos horizontales, los tiles se parchean solo cuando cambia el estado visible/no explorado, el scroll del mapa se emite solo si cambia la cámara y `SPR_update()` solo se llama si algún sprite cambió.
3. **HUD de diagnóstico:** `SYS_getFPS()` se consulta en cada fotograma para mantener correcta la estimación de SGDK; el texto se dibuja explícitamente en BG_B cada 8 fotogramas. El prefijo y el valor ocupan las columnas 27–34, fuera de la máscara lateral que empieza en la columna 35.
4. **Regresión:** se añade una prueba que cambia únicamente `maxMovementPoints` y verifica que la actualización de niebla procesa cero celdas de visión y no genera celdas sucias.

## Validación

- `sh tests/run_tests.sh`: suite completa.
- `sh tests/run_tests_sanitized.sh`: suite con ASan/UBSan cuando están disponibles.
- GCC `-fanalyzer` sobre todos los módulos.

La validación host no sustituye una compilación ROM con SGDK real ni una medición de FPS en BlastEm o hardware. El ahorro de escrituras y del trabajo de LOS está comprobado por la ruta de código y la prueba de regresión; el impacto en FPS reales debe medirse allí.
