# Continuación: coordinación de IA y consistencia del foco visible

## Objetivo

Endurecer el contrato de niebla de guerra de la IA roja sin cambiar combate, movimiento, pathfinding, roles tácticos ni las optimizaciones de memoria aplazadas.

## Cambio realizado

- Se reforzó `testAIDoesNotTargetHiddenEnemy` en `tests/enemy_ai_test.c` para simular un foco táctico heredado hacia un enemigo que ya no es visible.
- La prueba exige que `EnemyAI_update()` invalide el foco antes de elegir una acción, que no ataque al enemigo oculto y que no reduzca sus puntos de vida. Si la IA mueve la unidad, también se verifica que no termine en la misma casilla que el enemigo.
- Se actualizó `docs/ia_tactica_terreno.md` para dejar documentado el contrato de invalidación de foco cuando el blanco deja de estar visible.

La implementación existente de `refreshFocusTarget()` ya aplicaba esta regla; la mejora de esta iteración es fijarla como regresión explícita para impedir que una futura modificación de la coordinación de fuego la rompa.

## Verificación

- `sh tests/run_tests.sh`: 23/23 grupos y prueba extra de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos y prueba extra superados con ASan/UBSan.
- GCC host con `-Wall -Wextra -Werror -Wformat=2 -Wno-main -fanalyzer -fsyntax-only`: todos los módulos `src/*.c` pasan. `-Wno-main` es necesario porque SGDK usa su firma de entrada `int main(bool hardReset)`, que no es la firma de un ejecutable host estándar.
- Se mantiene la prueba de pathfinding sobre 20.000 mapas y la simulación de turno con 120 unidades.

## Limitaciones

Las pruebas host usan stubs de SGDK. No se ha compilado una ROM con el SGDK real ni se ha ejecutado BlastEm en este entorno. No se afirma validación visual de hardware ni una medición real de FPS.
