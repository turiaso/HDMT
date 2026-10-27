# Refactorización y auditoría del código

**Edición:** 25 de octubre de 2026  
**Base:** escenario de 40 × 40 casillas, 120 unidades lógicas, niebla persistente, IA roja, combate por clase y diagnóstico de rendimiento opcional.

## Objetivo y criterio

La meta de esta edición es que el código sea más fácil de leer, modificar y verificar sin reescribir los algoritmos tácticos ni cambiar intencionadamente las reglas del juego. Se priorizaron límites claros entre módulos, un punto de entrada legible, configuración centralizada, documentación coherente y verificaciones reproducibles.

## Cambios estructurales

- `main.c` mantiene la coordinación de la partida. El bucle `updateGameFrame()` está compuesto de pasos con nombres explícitos: procesar niebla pendiente, animar una unidad, tomar una decisión de IA o leer el turno del jugador, sincronizar el turno, procesar el trabajo aplazado y presentar el fotograma.
- `scenario_menu.c/.h` posee el selector de escenarios al arrancar. No crea el mapa lógico ni consume los grandes buffers de una partida.
- `game_screens.c/.h` posee la presentación de máscaras de viewport y el panel final. Recibe el estado de la partida y el índice de niebla como parámetros, en lugar de depender de los globales de `main.c`.
- Los parámetros de seguimiento de cámara de la IA y la duración de presentación del ataque están centralizados en `game_config.h`.
- `PERF_DIAGNOSTICS` está desactivado por defecto. Se activa explícitamente solo para comparar carga de trabajo; el contador `FPS` se conserva. Así la build normal no incluye logs diagnósticos innecesarios.
- Se añadió `.clang-format` como definición de estilo: cuatro espacios, llaves K&R, 100 columnas y sin ordenar inclusiones automáticamente. Los órdenes de inclusión importan con las cabeceras de tipos de SGDK.
- La raíz se simplificó: los informes de iteraciones anteriores están en `docs/historico/`; las páginas de referencia actual están separadas en guía, arquitectura, escenarios, unidades y esta auditoría.
- Se corrigieron rutas a informes archivados y se eliminó una entrada duplicada en la tabla de arquitectura.

## Mapa de módulos

| Módulo | Responsabilidad principal |
|---|---|
| `main.c` | Integración de subsistemas, ciclo de frame y estados de la sesión activa. |
| `scenario.c`, `scenario_menu.c` | Datos y selección de escenarios. |
| `game_screens.c`, `mission_result.c` | Presentación de pantallas y resolución de las entradas del panel final. |
| `map_data.c` | Datos lógicos por casilla, terreno, ocupación y línea de visión. |
| `unit_types.c`, `unit_controller.c` | Perfiles de clase y estado autoritativo de unidades/movimiento. |
| `pathfinder.c` | Rutas y alcanzabilidad ponderadas. |
| `cell_events.c` | Eventos definidos por perfiles compartidos. |
| `fog_of_war.c` | Visión por equipo, exploración persistente y lista de celdas sucias. |
| `combat.c`, `enemy_ai.c`, `game_turn.c` | Combate, decisiones rojas y reglas de turno/misión. |
| `camera_cursor.c` | Entrada, cursor, paneo y enfoque automático de cámara. |
| `unit_view.c`, `movement_glow.c` | Representación de unidades y actualización localizada de tiles. |
| `cell_hud.c`, `game_config.h` | Información táctica y parámetros editables. |
| `performance.c` | Contadores opcionales de trabajo por fase. |

## Invariantes protegidos

- `UnitController` y `MapData` son la fuente autoritativa de posición, ocupación y terreno. La vista solo representa el estado.
- El workspace de pathfinding se reutiliza. No debe iniciarse otra búsqueda mientras se consume una ruta guardada en ese workspace.
- La vista previa consulta el alcance precalculado; no debe ejecutar pathfinding al mover el cursor.
- `EXPLORED` no significa `VISIBLE`. El terreno descubierto persiste, pero los ocupantes enemigos ocultos no pueden filtrarse a través del HUD, los blancos de ataque o la IA.
- El jugador, la IA y la predicción de combate comparten la validación de alcance y línea de visión.
- La IA toma una decisión por llamada; nunca debe animar una ruta dentro de un bucle bloqueante.
- El juego mantiene 120 unidades lógicas, pero el renderizado se limita a 77 sprites de unidad y tres sprites fijos por el presupuesto de la SAT. El límite independiente por línea exige validación visual.
- Los buffers grandes de mapas/alcance se comparten entre escenarios; solo se inicializa una partida activa.
- Las propuestas aplazadas de optimización de RAM A/B/C y el diseño del sistema compartido de eventos se mantienen sin cambios.

## Validación ejecutada

| Comprobación | Resultado |
|---|---|
| `sh tests/run_tests.sh` | **23 grupos y prueba adicional de perfilado superados**. Incluye rutas contra Dijkstra en 20.000 mapas y turno de IA con 120 unidades. |
| `sh tests/run_tests_sanitized.sh` | **La suite completa terminó correctamente en una pasada de esta auditoría.** En reintentos posteriores, el proceso del contenedor quedó interrumpido durante la compilación de un ejecutable de prueba, sin un informe ASan/UBSan; los mismos ejecutables, compilados/ejecutados por separado, pasaron. |
| GCC `-Wall -Wextra -Werror -Wformat=2 -fanalyzer` | Los 21 módulos C se compilaron sin diagnósticos, con stubs SGDK. |
| Orden de cabeceras SGDK | Todos los módulos pasan la prueba que exige que los tipos base estén definidos antes de la versión SGDK de `string.h`. |
| Recursos | Las 10 referencias de recursos locales existen. Los 11 PNG de `res/gfx/` son indexados; tamaños comprobados: mapas 968 × 968, atlas 96 × 24, unidades 16 × 16, cursor 24 × 24 y reserva de niebla 8 × 8. |
| Documentación | 63 archivos Markdown revisados; cero enlaces relativos rotos. |
| Formato C/H | 66 archivos revisados; sin espacios al final ni líneas de más de 100 columnas. |
| Integridad del paquete | Verificada con `unzip -t` al generar el ZIP. |

## Lo que no se puede certificar en este entorno

La toolchain real de SGDK de Windows y BlastEm no están disponibles en esta sesión. Por tanto, no se declara que esta edición haya producido una ROM, que el VDP real se haya validado visualmente o que se haya medido una cifra concreta de FPS. Las suites host verifican la lógica C con stubs, pero no sustituyen la compilación cruzada ni la emulación.

Compilar desde la raíz en Windows:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" clean
```

La salida esperada es `out\rom.bin`. Tras compilar, revisar en BlastEm el arranque, los cuatro escenarios, selección, movimiento, niebla, ataques, turnos completos de IA, pantalla final, filas con muchos sprites y el contador FPS durante paneo y turno rojo.

## Cómo continuar editando

Antes de cambiar una regla, consulta [`guia_desarrollo.md`](guia_desarrollo.md) para encontrar el módulo propietario y sus tests. Mantén los cambios pequeños y ejecuta ambas suites de host. Si el cambio toca recursos, VDP, scroll, sprites, HUD o rendimiento, recompila y comprueba la ROM en BlastEm antes de darlo por terminado.

## Continuación: mantenibilidad del código (26 de octubre de 2026)

Se centralizaron las constantes de capacidad de los subsistemas de gameplay en `map_data.h`, manteniendo `MapData` genérico para mapas de prueba mayores. `GameTurn_checkObjective()` ahora obtiene en una sola pasada el número de supervivientes de ambos equipos para las misiones que lo necesitan, y el valor por defecto de duración está centralizado. Las cabeceras documentan propiedad de memoria y estado autoritativo. Para repetir las comprobaciones de estilo y análisis estático, ejecutar `sh tools/check_source_quality.sh`.

Detalle de cambios y resultados: [`INFORME_REFACTORIZACION_MANTENIBILIDAD.md`](INFORME_REFACTORIZACION_MANTENIBILIDAD.md).
