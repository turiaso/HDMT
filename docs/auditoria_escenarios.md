# Auditoría de las tres misiones

**Resultado:** configuraciones lógicas validadas en host. No sustituye la compilación con SGDK ni la validación visual en BlastEm.

## Hallazgos y cambios

- `Punto de control` usa `MISSION_CAPTURE_POINT`: al ocupar la casilla `(20,20)` la misión termina inmediatamente. Su `requiredTurns` estaba en `3`, un dato engañoso que no se consume para este tipo de misión; se cambió a `0` para expresar que no es temporizada.
- `Defensa del puente` y `Puente del desfiladero` usan `MISSION_HOLD_POINT` con `requiredTurns = 3`. El progreso avanza al finalizar turnos del equipo ocupante, no en el turno rival. Si cambia el control, el motor reinicia la racha; esta regla ya estaba cubierta por las pruebas del sistema de turnos y ahora también se ejercita con las definiciones reales.
- Los tres mapas comparten buffers de lógica/pathfinding por diseño. La documentación vigente tenía texto obsoleto sobre dos descriptores y sobre añadir un tercer escenario; está actualizado.

## Comprobaciones añadidas

`tests/scenario_test.c` valida por cada escenario:

- Dimensiones lógicas permitidas, recursos, terreno, objetivo transitable y objetivo inicialmente libre.
- Cantidad y composición de cada equipo, IDs consecutivos, celdas transitables, despliegue sin solapamientos y capacidad de almacenamiento.
- Una ruta válida desde cada unidad desplegada hasta el objetivo, usando su clase y evitando casillas ocupadas por otras unidades.
- Inicio real de `GameTurn` con la definición seleccionada y estado inicial activo.
- `Punto de control`: captura inmediata por Azul y por Rojo.
- Ambos escenarios de puente: progreso exactamente de tres finales de turno del equipo ocupante, sin contar los finales del equipo rival; victoria para Azul o para Rojo.
- Validación repetida de los objetivos antes de cerrar cada turno: cuatro consultas consecutivas no incrementan el progreso; este avanza como máximo una vez al finalizar el turno del equipo ocupante.
- Una vez alcanzado el tercer turno, los intentos de cerrar otro turno o volver a consultar el objetivo no duplican la victoria ni alteran el progreso final.

También se corrige la compilación del test de escenarios en los scripts Linux/macOS, sanitizado y Windows para enlazar `game_turn.c` y permitir estas pruebas de integración.

## Recursos gráficos y mapas

Se verificaron los tamaños actuales de imagen:

| Recurso | Tamaño PNG | Mapa lógico |
|---|---:|---:|
| `world_map.png` | 968×968 | 40×40 celdas |
| `bridge_map.png` | 968×968 | 40×40 celdas |
| `bridge_skirmish_map.png` | 488×488 | 20×20 celdas |

Las dimensiones siguen la geometría de 24 píxeles por celda más el margen técnico de 8 píxeles ya empleado por los mapas. Las filas lógicas se validan al inicializar `MapData` y se comprueban junto con el terreno y las rutas en la suite.

## Validación ejecutada

- `sh tests/run_tests.sh`: superado (23 grupos más pruebas adicionales), incluidos los nuevos checks de progreso entre fotogramas y cierre idempotente de la misión.
- `sh tests/run_tests_sanitized.sh`: superado con ASan/UBSan, incluido el estrés con 120 unidades.
- GCC `-fanalyzer` para todos los módulos de `src/`: superado.
- Integridad de recursos PNG verificada; archivo ZIP final se comprueba antes de entregar.

## Límite de la validación

No se ha generado una ROM con la toolchain real de SGDK ni ejecutado en BlastEm en esta sesión. Estas pruebas validan reglas y conectividad en host, no el renderizado del VDP real ni la experiencia jugable completa.
