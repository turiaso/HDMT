# Informe de continuación: auditoría de IA y niebla de guerra

## Cambios realizados

- Añadida `testAIDoesNotTargetHiddenEnemy` a `tests/enemy_ai_test.c`. Coloca un enemigo adyacente que no es visible para Rojo, simula un foco heredado hacia ese enemigo y verifica que la IA invalida el foco, no lo ataca ni reduce sus puntos de vida; si inicia movimiento, también comprueba que la unidad no se solapa con el ocupante oculto.
- Documentada en `docs/ia_tactica_terreno.md` la regla de que los blancos y las amenazas tácticas deben proceder de enemigos actualmente visibles, no de posiciones conocidas solo por exploración pasada.
- Eliminada una asignación duplicada e innecesaria de `found = TRUE` en `selectBestDestination()`; no cambia la lógica de selección.

## Verificación

- `sh tests/run_tests.sh`: 23/23 grupos y prueba adicional de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos y prueba adicional superados con ASan/UBSan.
- GCC host con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer -fsyntax-only`: todos los módulos `src/*.c` pasan sin diagnósticos.
- Se mantienen la prueba de propiedades de pathfinding sobre 20.000 mapas y la simulación del turno de IA con 120 unidades.

## Límites de verificación

Las pruebas host usan stubs de SGDK. En este entorno no se ha compilado una ROM con la instalación real de SGDK ni se ha ejecutado BlastEm; por tanto, no se declara validación visual ni medición real de FPS. El resultado de esta iteración es una mejora de regresión y documentación, no un cambio de reglas de combate. La última regresión cubre también la invalidación de un blanco que deja de estar visible, no solo el caso de foco vacío.
