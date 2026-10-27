# Estrategia Mega Drive · SGDK

Juego de estrategia por turnos escrito en C para SGDK. El estado lógico del mapa y de las unidades es independiente de la representación gráfica y de los sprites del VDP.

## Estado actual

- Mapa principal de **40 × 40 celdas** y **60 unidades por equipo** (120 lógicas).
- Tres escenarios: **Punto de control**, **Defensa del puente** y **Puente del desfiladero** (20×20, 6 unidades por equipo).
- Cuatro clases de unidad, costes de movimiento por combinación de clase y terreno, selección restringida, confirmación/cancelación de órdenes y eventos compartidos por perfil de celda.
- Una orden de movimiento por unidad y turno; las rutas interrumpidas tras avanzar consumen la acción y los puntos solo se restauran al comenzar un nuevo turno válido del equipo.
- Turnos alternos, IA roja coordinada, cuatro condiciones de misión, combate con predicción de daño/represalia, cobertura y línea de visión. Los ingenieros pueden reparar un aliado adyacente hasta 3 PV por turno; reparar y atacar comparten acción.
- Niebla de guerra por equipo, exploración persistente y vista espectadora durante el turno rojo.
- Renderizado acotado por el presupuesto del VDP: máximo de 77 sprites de unidad simultáneos, más tres sprites fijos.

## Empieza aquí si vas a editar el código

1. Lee [`docs/README.md`](docs/README.md) para acceder al índice de documentación actual.
2. Empieza por [`docs/guia_desarrollo.md`](docs/guia_desarrollo.md) y [`docs/arquitectura.md`](docs/arquitectura.md) para conocer los contratos y quién es dueño de cada responsabilidad.
3. Usa [`docs/escenarios.md`](docs/escenarios.md) y [`docs/unidades.md`](docs/unidades.md) para editar misiones y clases. Para el comportamiento táctico de la IA, consulta [`docs/ia_tactica_terreno.md`](docs/ia_tactica_terreno.md) y la [guía de posicionamiento y congestión](docs/posicionamiento_ia_y_congestion.md).
4. Para modificar la presentación del HUD, los mensajes de acciones y la pantalla final de misión, consulta [`docs/hud_jugabilidad.md`](docs/hud_jugabilidad.md), [`docs/hud_inspeccion_ocupacion.md`](docs/hud_inspeccion_ocupacion.md), [`docs/pantalla_resultado_y_flujo_partida.md`](docs/pantalla_resultado_y_flujo_partida.md) y [`docs/ciclo_partida_y_reinicio.md`](docs/ciclo_partida_y_reinicio.md). Para las auditorías, consulta [`docs/refactorizacion_y_auditoria.md`](docs/refactorizacion_y_auditoria.md), el [informe de mantenibilidad](docs/INFORME_REFACTORIZACION_MANTENIBILIDAD.md), la [corrección de objetivos de misión](docs/correccion_objetivos_mision.md), la [validación del progreso de objetivos por turnos](docs/validacion_turnos_objetivos.md) y los [casos límite de resultados de misión](docs/casos_limite_resultados_mision.md). Ejecuta las pruebas después de cada cambio. Los informes antiguos se han movido a [`docs/historico/`](docs/historico/README.md) para no mezclar bitácoras con fuentes. Pueden describir estados intermedios; esta guía, la arquitectura y las cabeceras de `src/` son la referencia actual.

## Compilar con SGDK

En Linux o macOS, configura `GDK` con la ruta de tu instalación de SGDK y asegúrate de tener Java y el compilador cruzado `m68k-elf-gcc` en `PATH`:

```sh
export GDK="/ruta/a/SGDK"
export PATH="$GDK/bin:$PATH"
tools/build_sgdk.sh
```

El script comprueba las dependencias, compila los recursos y el código y verifica que la ROM resultante no esté vacía. Para limpiar antes de compilar, usa `tools/build_sgdk.sh clean` y después `tools/build_sgdk.sh`.

Para ejecutar la ROM en BlastEm, una vez generada:

```sh
blastem out/rom.bin
```

Ajusta el nombre si tu versión/configuración de SGDK genera `out/rom.gen` en lugar de `out/rom.bin`. Si la compilación falla, revisa primero el primer error real de C o `rescomp`; los errores posteriores suelen ser consecuencia de ese fallo.

## Pruebas host

En Linux o una shell compatible, ejecuta la auditoría host completa:

```sh
tools/audit_host.sh
```

El script comprueba declaraciones y PNG de `res/resources.res`, ejecuta la suite normal y ASan/UBSan, verifica estilo y pasa GCC `-fanalyzer`. Si prefieres ejecutarlas por separado, usa `sh tests/run_tests.sh`, `sh tests/run_tests_sanitized.sh`, `python3 tools/check_resources.py` y `sh tools/check_source_quality.sh`.

La suite cubre lógica C, rutas comparadas con Dijkstra en 20.000 mapas aleatorios, niebla, combate, eventos, IA y el turno simulado de 120 unidades. Los stubs host **no** sustituyen la compilación SGDK ni validan por sí solos el VDP, la memoria final de ROM o los FPS de BlastEm.

## Diagnósticos de rendimiento

`PERF_DIAGNOSTICS` está desactivado por defecto para no añadir contadores y logs a la build normal. Actívalo temporalmente con `-DPERF_DIAGNOSTICS=1` o cambiando la definición en `src/game_config.h`. `SYS_getFPS()` se consulta cada fotograma y el valor se pinta en el HUD cada 8 fotogramas; los registros `PERF` miden cantidades de trabajo, no ciclos exactos del 68000.

## Restricciones importantes

- `UnitController` mantiene la posición lógica, ocupación, selección y ruta autoritativas. `UnitView` nunca debe cambiar reglas del juego.
- `PathfinderWorkspace` se reutiliza: no ejecutes una nueva búsqueda mientras el controlador consume una ruta guardada en ese espacio de trabajo.
- `FogOfWar` modifica el estado lógico de visión y publica celdas sucias; `MovementGlow` aplica los cambios al tilemap.
- Los dos `MapData` del registro comparten buffers grandes. Solo un escenario se inicializa como partida activa.
- No aumentar unidades, celdas ni memoria estática sin actualizar los límites, el presupuesto de RAM y sus pruebas.
- Mantener las optimizaciones de RAM A/B/C y el sistema compartido de eventos tal como están hasta que se apruebe explícitamente su rediseño.

La compilación de ROM y la prueba visual deben hacerse con la instalación local de SGDK/BlastEm. No se debe dar por validado el rendimiento de hardware basándose únicamente en los tests host.
## Optimización de niebla y presentación

Consulta [`docs/historico/INFORME_OPTIMIZACION_NIEBLA_Y_REDIBUJADO.md`](docs/historico/INFORME_OPTIMIZACION_NIEBLA_Y_REDIBUJADO.md) para los cambios incrementales de visión, parcheo de tiles y frecuencia del overlay de FPS.
