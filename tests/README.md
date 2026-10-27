# Pruebas del proyecto

## Ejecutar

- Windows: `tests\run_tests.bat` (desde la raíz del proyecto también funciona).
- Linux/macOS: `sh tests/run_tests.sh`
- Linux/macOS con AddressSanitizer y UndefinedBehaviorSanitizer: `sh tests/run_tests_sanitized.sh`.

Las pruebas host usan GCC en `PATH`. El script de Windows no intenta recurrir a Clang. La suite no requiere SGDK para los tests unitarios, pero solo la compilación real de la ROM y su ejecución en BlastEm pueden confirmar el comportamiento visual y la compatibilidad final con SGDK.

## Cobertura

- `cell_events_test.c`: orden, contexto, perfiles y límites de los eventos.
- `camera_cursor_test.c`: movimiento suave del cursor en exploración, saltos de una celda solo con la máscara de destino activa, alineación al seleccionar unidad, restricciones por alcance, scroll automático y directo por celdas, límites del mapa y ausencia de desalineación durante el scroll.
- `map_data_test.c`: inicialización, dimensiones y conteo de terrenos.
- `pathfinder_test.c`: rutas ponderadas, matriz de costes por clase (bosque/piedras), presupuesto, agua, ocupación, coordenadas y capacidad del espacio de trabajo. `movement_range_test.c` valida el alcance ponderado, la consulta constante de coste/pasos del HUD y rutas de hasta 254 pasos sin desbordar su codificación.
- `unit_controller_test.c`: cuatro tipos, costes de terreno cobrados según la clase, puntos por defecto, ruta preparada, confirmación/cancelación, movimiento interpolado, ocupación, gasto de puntos y bloqueo tras agotarlos.
- `unit_view_test.c`: sprites distintos por tipo, paletas de equipo, ausencia de marcadores de ruta, visibilidad y liberación idempotente, y rollback ante fallos de inicialización parcial.
- `cell_hud_test.c`: texto del HUD, datos de casilla, estado de ruta, turno/equipo, progreso de misión, predicción de combate y redibujado.
- `team_palette_test.c`: líneas PAL2/PAL3, colores de equipo y compatibilidad del color de texto. `enemy_ai_test.c`: aproxima unidades visibles, prioriza el objetivo en misiones de captura, elige ataques según daño/bajas/represalia, mantiene posiciones defensivas, busca cobertura y rechaza llamadas de IA con mapa/equipo activo incoherentes sin consumir acciones.
- `game_turn_test.c`: alternancia de equipos, turnos realmente transcurridos, supervivientes por bando, motivo/éxito del objetivo, rechazo de órdenes tras el resultado, recarga de puntos, coste por terreno y las cuatro misiones (captura, mantener, sobrevivir y eliminar).
- `hud_vram_layout_test.c`: separación de tiles de terreno, fuente, unidad y cursor. `movement_glow_test.c`: duplicación/animación de PAL1, niveles de brillo con saturación, parcheo de filas/columnas del mapa y retirada del glow.

Las pruebas de host inspeccionan lógica C y texto enviado al VDP; no emulan los patrones de fuente ni la VRAM real.

`ai_full_turn_test.c` simula el turno completo de la IA roja con 120 unidades (60 por equipo) y niebla activa. `pathfinder_property_test.c` compara los costes y cadenas de padres con Dijkstra independiente en 20.000 mapas aleatorios reproducibles. `scenario_test.c` verifica los cuatro escenarios de 40×40/20×20, despliegues, terreno transitable, coordenadas únicas, equilibrio de equipos/tipos y presupuesto SAT. Comprueba además que todas las unidades tienen ruta al objetivo y ejecuta flujos de misión sobre los mapas reales para los dos bandos: captura inmediata en `Punto de control` y tres finales de turno propios para mantener los tres puentes/cruces. También comprueba el avance circular del selector y los textos de las cuatro misiones. El grupo de instrumentación compila la opción `PERF_DIAGNOSTICS=1`, y el test extra `performance_phase_test.c` valida los contadores aislados por fase y sus ventanas de 120 frames. El grupo final verifica la resolución de controles de la pantalla de resultado; `game_screens_test.c` comprueba el texto de victoria/derrota/empate, motivo, progreso, supervivientes y controles A/B/START, incluidos los límites de escritura del HUD. `mission_end_guard_test.sh` protege las entradas de órdenes para que se nieguen tras finalizar la partida. La instrumentación cuenta trabajo, no ciclos de CPU ni FPS.


## Turno de IA visible

`camera_cursor_test.c` verifica la cámara automática: centra suavemente un punto del mapa y permite volver al encuadre anterior. `enemy_ai_test.c` comprueba el índice de la última unidad roja que debe seguir la cámara. `movement_glow_test.c` comprueba que la vista compartida muestra las celdas visibles por cualquiera de los equipos y mantiene la niebla donde ambos desconocen el terreno; `unit_view_test.c` y `cell_hud_test.c` verifican la misma regla para sprites y datos del HUD. La validación visual integrada en el VDP aún requiere compilar una ROM y ejecutarla en BlastEm.

## Selector de misiones

Al arrancar, el menú se dibuja en BG_B con las paletas existentes. Arriba/abajo cambia la selección y A o START comienza. Las pruebas de host verifican la navegación circular y los datos que consume el menú; el aspecto y el cambio a la partida deben validarse en la ROM real.

## Fin de partida

`mission_result_test.c` comprueba la resolución pura de repetir la misión o volver al selector; `game_screens_test.c` registra las escrituras al VDP y los botones de la pantalla final. La limpieza de `Map`/motor de sprites y los controles físicos debe comprobarse también con la ROM en BlastEm.

## Simulador de batalla con ambos bandos

`ai_combat_stress_test.c` ejecuta turnos alternos para Rojo y Azul con 60 unidades por equipo, manteniendo una instancia táctica por bando. Comprueba la integridad espacial tras cada movimiento y ataque, y resume movimientos, ataques, reparaciones, bajas y turnos completados. Se ejecuta automáticamente desde `tests/run_tests.sh` y `tests/run_tests_sanitized.sh`.
