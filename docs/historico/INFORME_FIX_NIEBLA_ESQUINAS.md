# Corrección de niebla de guerra: visión al cruzar esquinas

## Síntoma
La niebla podía formar huecos y zonas oscuras irregulares cerca de obstáculos. El trazado de línea de visión usaba supercover y, cuando el rayo cruzaba exactamente una esquina de la cuadrícula, rechazaba la visión si cualquiera de las dos celdas laterales era opaca. Eso trata celdas que el rayo solo roza como bloqueadores y cierra de forma excesiva pasos diagonales.

## Cambio
En `src/map_data.c`, `MapData_hasLineOfSight()` ahora avanza diagonalmente en cruces exactos de esquina sin considerar automáticamente bloqueadoras las dos celdas laterales. Las celdas realmente atravesadas por el rayo siguen bloqueando la visión; el destino opaco sigue siendo visible. La función mantiene su recorrido simétrico.

## Validación
- `tests/run_tests.sh`: 23 comprobaciones superadas.
- `tests/run_tests_sanitized.sh`: 23 comprobaciones superadas con ASan/UBSan.
- Incluye pruebas de simetría de LOS, oclusión por terreno, niebla incremental, exploración persistente, ocultación de enemigos y movimiento con giros.

## Pendiente
La validación automatizada no sustituye una comprobación visual de la ROM en BlastEm. Probar especialmente corredores diagonales junto a bosque/piedra, movimientos con cámara desplazándose y zonas ya exploradas tras retirar una unidad.
