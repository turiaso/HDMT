# Informe de refactorización para mantenimiento

**Fecha:** 26 de octubre de 2026  
**Base:** última versión entregada con la auditoría de movimiento y turnos.  
**Objetivo:** facilitar futuras modificaciones sin cambiar intencionadamente las reglas visibles del juego.

## Cambios aplicados

### 1. Límites compartidos en un solo lugar

`src/map_data.h` define ahora `GAMEPLAY_MAP_MAX_SIDE` y `GAMEPLAY_MAP_MAX_CELLS`. Los buffers estáticos de `Pathfinder`, `FogOfWar` y `UnitView` derivan sus alias locales de esas constantes, en lugar de repetir `40` y `1600` por separado.

`MapData` sigue siendo un contenedor genérico: no se le impone el límite visual de 40x40. Esto es importante para las pruebas de pathfinding con mapas rectangulares o rutas largas. Los módulos de gameplay validan por separado los límites que realmente usan.

### 2. Menos recorridos para resolver objetivos

Antes, las condiciones que comparaban unidades vivas de Azul y Rojo recorrían la lista de unidades una vez por equipo. `GameTurn_checkObjective()` usa ahora `countAliveUnitsByTeam()` para contar los dos bandos en una única pasada cuando la misión es eliminación total o supervivencia.

Se centralizó también el valor predeterminado de los objetivos temporizados en `requiredObjectiveTurns()`. Un `requiredTurns` de cero sigue significando una ronda; no se han cambiado las condiciones de victoria.

### 3. Contratos de los módulos documentados

Se ampliaron los comentarios de las cabeceras de `UnitController`, `MapData`, `PathfinderWorkspace`, `FogOfWar` y `GameTurn`. Explican qué estado es autoritativo, quién posee la memoria, qué datos son cachés y qué invariantes deben respetarse al editar.

### 4. Comprobación de calidad reproducible

Se añadió `tools/check_source_quality.sh`, que ejecuta:

- `tools/check_source_quality.py`: revisa los 42 archivos C/H de `src/` en busca de tabulaciones, espacios al final, ausencia de salto final y líneas de más de 100 columnas.
- GCC con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer` para cada módulo C, usando los stubs de pruebas.

## Guía para seguir editando

1. Consulta `docs/guia_desarrollo.md` para localizar el módulo dueño del comportamiento.
2. Mantén las reglas de juego en `MapData`, `UnitController`, `GameTurn`, `Combat` o `EnemyAI`, no en el código de presentación.
3. Haz un cambio pequeño y acompáñalo de una prueba que describa el comportamiento esperado.
4. Ejecuta `sh tests/run_tests.sh`, `sh tests/run_tests_sanitized.sh` y `sh tools/check_source_quality.sh`.
5. Si el cambio afecta a VDP, sprites, scroll, máscaras, niebla o HUD, compila la ROM con SGDK y verifícala visualmente en BlastEm. Los stubs host no sustituyen esta última prueba.

## Validación de esta entrega

- `sh tests/run_tests.sh`: 23 grupos, pruebas adicionales y estrés de combate superados.
- `sh tests/run_tests_sanitized.sh`: mismas suites superadas con ASan/UBSan.
- `sh tools/check_source_quality.sh`: 42 archivos C/H correctos y todos los módulos pasan GCC `-fanalyzer`.
- El refactor no modifica la cámara, la niebla, la precarga de sprites, los escenarios ni el diseño de RAM compartida.

**No validado en esta sesión:** compilación con la toolchain SGDK real ni ejecución visual de la ROM en BlastEm.
