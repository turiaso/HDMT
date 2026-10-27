# Posicionamiento y coordinación de la IA

## Qué hace el ajuste

- Reduce la tendencia de unidades de apoyo y flanqueo a amontonarse en la misma corona de posiciones.
- Penaliza situarse justo en el siguiente paso de una aliada que se dirige al mismo objetivo.
- Usa listas de aliadas cercanas precalculadas por decisión para no barrer las 120 unidades en cada casilla candidata.
- Resuelve empates tácticos dando prioridad al progreso hacia el blanco/objetivo antes que al coste menor de la ruta.
- Refuerza la preferencia por líneas de tiro abiertas, sin sustituir la validación de `MapData_hasLineOfSight()` ni las reglas de `Combat`.
- Conserva la preferencia por cobertura de la infantería y camino para blindados.

Las penalizaciones son preferencias, no restricciones nuevas. Si el puente es de una sola casilla, una unidad puede esperar en la orilla y cruzar cuando la casilla ocupada se libere. El pathfinder continúa decidiendo qué destinos son alcanzables y `UnitController` es responsable de movimiento y ocupación.

## Regresiones añadidas

Las pruebas `testInfantryUsesCoverWhenProgressIsEquivalent`, `testTwoTanksChooseSeparatedFiringPositions`, `testAILeavesFriendlyApproachLaneOpen` y `testNarrowBridgeQueuesAndReleasesUnitsSafely` verifican cobertura, separación de posiciones, elección de una ruta alternativa y cola segura en puente estrecho. El turno completo sigue comprobándose con 120 unidades en `tests/ai_full_turn_test.c` y la integridad después del combate en `tests/ai_combat_stress_test.c`.

## Validación de ROM

Las pruebas host no sustituyen el build SGDK. Para compilar y abrir en BlastEm, sigue [`validacion_sgdk_y_batalla.md`](validacion_sgdk_y_batalla.md) y [`pruebas_regresion_emulador.md`](pruebas_regresion_emulador.md). Registra el primer error de `rescomp`/C si falla y no marques el build como correcto hasta que la ROM arranque.
