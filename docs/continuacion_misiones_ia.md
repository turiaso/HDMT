# Continuación de misiones e IA

## Validación añadida

- Se amplió el fixture de reglas de partida para admitir hasta cuatro unidades sin reservar memoria dinámica.
- Se añadió una regresión para el límite de rondas con superioridad numérica: al finalizar la ronda configurada, gana el equipo con más unidades supervivientes.
- Se mantiene la prueba que garantiza que el límite solo se evalúa tras completar el turno de ambos equipos y que el resultado no se emite dos veces.

## Resultado de validación

- `sh tests/run_tests.sh`: correcto (23 grupos y pruebas adicionales).
- `sh tests/run_tests_sanitized.sh`: correcto con ASan/UBSan.
- `sh tools/check_source_quality.sh`: correcto; estilo y GCC `-fanalyzer` sin diagnósticos.
- `sh tools/audit_host.sh`: correcto, incluidos recursos, presupuesto de hardware, densidad de sprites y suites de pruebas.

Estas pruebas validan la lógica en el entorno host. No sustituyen la compilación de ROM con SGDK ni la prueba visual en BlastEm o hardware real.
