# Informe de implementación · Fase 6

## Resultado

La fase 6 incorpora atributos básicos por clase, combate táctico inmediato y niebla de guerra por equipo. Se conserva la arquitectura existente de mapa lógico, controlador de unidades, pathfinding, turnos, HUD y renderizado SGDK. Las estadísticas se definen en tablas de datos, no con condiciones dispersas por el controlador.

## Implementado

- Perfiles de infantería, explorador, ingeniero y blindado con movimiento, puntos de vida, ataque, defensa, alcance y visión.
- Ataque a una unidad enemiga visible, validado por equipo, acción disponible y alcance Manhattan.
- Daño determinista `max(1, ataque - defensa)`, limitado a los PV actuales.
- Contraataque automático cuando el defensor sobrevive y alcanza al atacante; la represalia no consume su ataque propio.
- Eliminación lógica de unidades derrotadas y liberación de la ocupación.
- Restablecimiento del ataque disponible al empezar el siguiente turno de la unidad/equipo.
- Visión individual de cada equipo, exploración persistente y estados empaquetados por celda.
- Ocultación de enemigos fuera de visión, oscurecimiento de terreno no explorado y supresión de datos ocultos en HUD.
- Filtrado temporal de enemigos ocultos al generar alcance y rutas, sin modificar de forma permanente la ocupación real.
- Pruebas de host nuevas y ampliadas para combate, niebla, HUD, render del tilemap y ocupación oculta.
- Rendimiento: la niebla recalcula solo regiones afectadas por las unidades modificadas, el tilemap refresca celdas descubiertas individualmente y la vista de unidades omite escrituras de posición/visibilidad redundantes.

## Decisiones explícitas

- El radio de visión se define mediante distancia Chebyshev y revela un cuadrado alrededor de cada unidad.
- El alcance de ataque se mide mediante distancia Manhattan.
- Ni la visión ni el ataque aplican oclusión; el terreno no bloquea la visión ni la línea de ataque.
- El daño no usa azar ni modificadores de terreno.
- Atacar no gasta puntos de movimiento, por lo que movimiento y ataque pueden combinarse dentro del mismo turno. Solo se puede iniciar un ataque por unidad y turno.
- La represalia es una reacción gratuita si el defensor sobrevive y alcanza al atacante.
- `CellData` pasa a 4 bytes por celda al añadir el estado de visión. No se han hecho las optimizaciones de RAM previamente aplazadas.

## Archivos principales

Nuevos: `src/combat.c/.h`, `src/fog_of_war.c/.h`, `tests/combat_test.c`, `tests/fog_of_war_test.c`, `docs/fase_6.md` y el recurso `res/gfx/fog_tile.png`.

Añadidos para la ampliación del escenario: `src/scenario.c/.h` y `tests/scenario_test.c`.

Modificados: `src/unit_types.c/.h`, `src/unit_controller.c/.h`, `src/map_data.c/.h`, `src/unit_view.c/.h`, `src/movement_glow.c/.h`, `src/cell_hud.c/.h`, `src/game_turn.c`, `src/main.c`, los scripts de prueba y documentación principal.

## Verificación realizada

- `sh tests/run_tests.sh`: las 18 secciones pasan, incluidas las nuevas de escenario, combate, niebla incremental y redibujado localizado.
- GCC 14.2 con `-Wall -Wextra -Werror -fanalyzer`: sin diagnósticos en los módulos C del juego revisados.
- Comprobaciones de formato: sin espacios finales y sin líneas C/H de más de 120 caracteres en los archivos modificados.
- Integridad del ZIP de entrega comprobada tras empaquetar.

## Corrección de compilación SGDK (PNG de niebla)

Durante la primera compilación en Windows, SGDK rechazó `res/gfx/fog_tile.png` porque era un PNG RGB de 8 × 8 y el conversor RGB de sprites exige un formato de imagen distinto para extraer la paleta. Se convirtió el recurso a PNG indexado de 8 bits con una paleta de 16 colores. Mantiene las dimensiones de 8 × 8 y sigue sirviendo como reserva de un único patrón; `src/main.c` sustituye los datos del patrón con `VDP_fillTileData()` al inicializar el mapa. La declaración `SPRITE fog_tile_reservation ... 1 1 NONE` no cambia.

El error posterior `No rule to make target 'out/debug/_deps/src/camera_cursor.d'` apunta a dependencias de compilación generadas obsoletas en `out/`, no a un archivo fuente requerido por el proyecto limpio. Tras aplicar el ZIP corregido, elimina la carpeta `out` y vuelve a compilar. El error `cannot stat out/debug/res/resources.d` aparece como consecuencia del fallo previo al generar los recursos. Se ha añadido el procedimiento de compilación limpia al README.

## Pendiente antes de dar por cerrada la fase

No se ha podido compilar la ROM con SGDK real ni probarla visualmente en BlastEm en este entorno, porque `GDK` no está configurado. En ese entorno hay que revisar la carga del recurso de niebla, el índice de tile reservado, el contraste de las casillas no exploradas, las paletas del HUD/unidades y el comportamiento al atacar y eliminar sprites. Las pruebas host se ejecutan contra stubs de SGDK y no sustituyen esa validación.

## Ampliación del escenario a 40 × 40

- **Mapa:** 1.600 celdas lógicas; cuatro cuadrantes del terreno original conectados por dos filas y columnas centrales de camino. El objetivo de captura está en `(20,20)`. El recurso gráfico mide 968 × 968 px (960 px de superficie de celdas y 8 px de margen técnico).
- **Despliegue:** 60 unidades por bando, 120 en total. Cada equipo contiene 16 infanterías, 16 exploradores, 14 ingenieros y 14 blindados. `src/scenario.c` contiene posiciones editables; la prueba verifica IDs únicos, terreno transitable, ausencia de solapamientos y balance de tipos.
- **Sprites de unidad:** PNG indexados de 16 × 16 px (2 × 2 tiles), con el índice 0 transparente. Se dibujan centrados en la celda lógica de 24 × 24 px.
- **RAM principal del mapa:** `CellData` ocupa 6.400 bytes y el workspace de pathfinding, 4.800 bytes. Total: 11.200 bytes (10,94 KiB), antes de contar unidades y otras estructuras.
- **Sprites hardware:** el presupuesto SAT es de hasta 80 entradas: 77 sprites de unidades y tres objetos fijos (cursor, objetivo y reserva de niebla). Las máscaras de borde se dibujan en BG_B. Las unidades sin entrada disponible permanecen lógicamente activas, pero no se dibujan hasta que haya hueco; se debe revisar el límite por línea.
- **Pruebas:** `sh tests/run_tests.sh` pasa las 18 secciones. También se ha actualizado `tests/run_tests.bat` para compilar `scenario.c` y ejecutar la nueva prueba en Windows.

La compilación real con la instalación local de SGDK y la validación visual en BlastEm siguen pendientes. Las pruebas host y la revisión estática no validan DMA, CRAM, streaming gráfico ni el límite de sprites por línea en hardware.

## Optimización posterior para el escenario 40×40

La visión inicial se calcula en `FogOfWar_init()`. En las actualizaciones posteriores se comparan snapshots de hasta 128 unidades lógicas y se recalculan únicamente los rectángulos combinados de la visión anterior/nueva de las unidades que cambian. La lista de estados modificados utiliza un contador de 16 bits para soportar más de 255 cambios en una sola actualización. Solo las celdas que cambian entre desconocidas/conocidas provocan transferencia de tiles, y solo cuando intersecan el viewport. El cambio de equipo mantiene un redibujado global puntual. `UnitView` cachea las posiciones de pantalla para omitir escrituras redundantes de sprite, al igual que cursor y marcador de objetivo.

Pruebas de regresión añadidas: estado estable procesa cero celdas; una transición de visión con más de 255 celdas se limpia correctamente en la siguiente actualización; la visión que comparten dos unidades no se pierde cuando una se aleja; un refresco de celda en pantalla envía tres filas y uno fuera de pantalla no envía ninguna.

Ver `INFORME_OPTIMIZACION_RENDIMIENTO.md`. La mejora de trabajo lógico está demostrada por las pruebas host, pero no se declaran valores de FPS hasta medir la ROM compilada con SGDK en BlastEm o hardware real.
