# Refuerzo del posicionamiento y coordinación de la IA

## Alcance

Esta revisión mejora la selección de posiciones para atacar y el movimiento coordinado de unidades aliadas. No modifica deliberadamente la cámara, la niebla de guerra, los sprites, las reglas de turno ni la validación autoritativa del movimiento y del combate.

## Cambios

- `friendlyTrafficPositionPenalty()` añade penalizaciones tácticas suaves para reducir el amontonamiento y evitar que una unidad se coloque justo delante de otra que avanza hacia el mismo objetivo.
- `collectNearbyFriendlyIndices()` recopila una sola vez por decisión las unidades aliadas cercanas; la IA evita recorrer el ejército completo para cada casilla candidata.
- La función de puntuación favorece el progreso hacia el objetivo cuando dos destinos tienen la misma puntuación táctica, antes de desempatar por coste de ruta.
- `firingLanePositionPenalty()` distingue con mayor claridad una casilla dentro del alcance geométrico pero con línea de tiro bloqueada de otra con línea despejada.
- La cobertura y la separación son preferencias, no restricciones de movimiento. El pathfinder sigue definiendo las casillas alcanzables; el controlador de unidades y el sistema de combate siguen validando las órdenes.
- En pasos estrechos, la IA puede formar una cola y avanzar cuando la casilla se libera, en lugar de intentar atravesar unidades o terreno intransitable.

## Regresiones añadidas

En `tests/enemy_ai_test.c`:

- `testInfantryUsesCoverWhenProgressIsEquivalent`: favorece la cobertura cuando el progreso y la posibilidad de disparar son comparables.
- `testTwoTanksChooseSeparatedFiringPositions`: dos blindados seleccionan posiciones de tiro distintas.
- `testAILeavesFriendlyApproachLaneOpen`: una unidad rodea a una aliada y deja libre su corredor de avance.
- `testNarrowBridgeQueuesAndReleasesUnitsSafely`: tres unidades hacen cola frente a un puente de una casilla y cruzan al liberarse el paso, sin solapamientos ni entradas en agua.

## Validación realizada

Ejecutado `sh tools/audit_host.sh` en el proyecto empaquetado. Resultado:

- comprobación de declaraciones SGDK y PNG: superada;
- suite funcional: 23 grupos y pruebas adicionales superados;
- ASan/UBSan, incluida la simulación de combate de 120 unidades: superados;
- comprobación de estilo C/H: superada;
- GCC `-fanalyzer`: todos los módulos sin diagnósticos.

## Validación SGDK/BlastEm pendiente

También se intentó ejecutar `sh tools/build_sgdk.sh`. No pudo empezar la compilación porque `GDK` no está definido y no están disponibles `m68k-elf-gcc`, `rescomp` ni BlastEm. La sesión no tiene acceso de red para instalar esas herramientas. Por tanto, este resultado no constituye una compilación ROM ni una validación visual del juego.

En un entorno con SGDK instalado, configura `GDK` y ejecuta `sh tools/build_sgdk.sh`. Después abre la ROM generada en BlastEm y sigue `docs/pruebas_regresion_emulador.md`, prestando especial atención a mapas abiertos, puentes estrechos, bordes de cámara y escenarios de 120 unidades.
