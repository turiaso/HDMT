# Unidades del escenario de ejemplo

Vista de los cuatro sprites con la paleta de su equipo: [`docs/previews/unidades.png`](previews/unidades.png).

## Tipos incluidos

| Tipo | Mov. | PV | Ataque | Defensa | Alcance ataque | Visión | Alcance reparación | Reparación (PV) | Sprite |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `UNIT_TYPE_INFANTRY` | 6 | 10 | 4 | 2 | 1 | 3 | — | — | Infantería |
| `UNIT_TYPE_SCOUT` | 8 | 7 | 3 | 1 | 1 | 4 | — | — | Explorador |
| `UNIT_TYPE_ENGINEER` | 5 | 8 | 2 | 2 | 1 | 2 | 1 | 3 | Ingeniero |
| `UNIT_TYPE_TANK` | 7 | 14 | 6 | 3 | 2 | 3 | — | — | Blindado |

Los valores se mantienen centralizados en `src/unit_types.c` (`UnitTypeInfo`) y se copian a cada unidad al iniciar el escenario. El alcance de ataque usa distancia Manhattan (`abs(dx) + abs(dy)`): infantería, explorador e ingeniero atacan a distancia 1; el blindado llega a distancia 2. El ingeniero tiene menos ataque que la infantería para compensar su capacidad de apoyo.

Solo el ingeniero repara aliados heridos a una celda Manhattan, restaura hasta 3 PV y nunca supera la vida máxima del objetivo. No puede repararse a sí mismo ni gastar más de una acción por turno: atacar o reparar consume la misma acción. Los aliados que se pueden reparar se resaltan con la paleta de alcance; los enemigos atacables conservan el rojo. El HUD del ingeniero muestra `REP1/3` (rango 1, hasta 3 PV restaurados). La IA repara inmediatamente cuando un aliado válido está adyacente y, si no hay enemigos visibles, puede aproximarse a otro aliado herido para repararlo en una decisión posterior. Entre aliados elegibles prioriza el mayor porcentaje de vida perdido, luego la pérdida absoluta y después la menor distancia.

Los puntos de movimiento son un presupuesto de coste, no un número fijo de casillas. El coste de entrada depende de la combinación entre terreno y clase; esta misma tabla gobierna el pathfinder, el alcance resaltado, la ruta confirmada, el gasto de puntos al llegar y la exploración inicial por movimiento.

| Terreno | Infantería | Explorador | Ingeniero | Blindado |
|---|---:|---:|---:|---:|
| Agua | Intransitable | Intransitable | Intransitable | Intransitable |
| Bosque | 2 | 1 | 3 | 3 |
| Camino | 1 | 1 | 1 | 1 |
| Piedras | 2 | 1 | 2 | 4 |

El explorador atraviesa terreno agreste con mayor facilidad; el ingeniero avanza más despacio por el bosque debido a su equipo; el blindado es especialmente lento en bosque y piedras, pero mantiene movilidad por los caminos. Los costes se centralizan en `terrainMoveCosts` de `src/unit_types.c`. El coste genérico de `TerrainInfo` queda como referencia de infantería para la inspección de una casilla sin unidad seleccionada; con una unidad seleccionada el HUD muestra el coste propio de esa clase.

Al seleccionar una unidad, los enemigos que puede atacar legalmente ahora se resaltan en rojo bajo su sprite. Ese resaltado reutiliza la predicción real del combate: solo marca objetivos vivos, del equipo rival, visibles para el equipo activo, dentro del alcance y con línea de visión despejada. No se marca un enemigo oculto ni un disparo bloqueado por bosque o roca. Si el cursor apunta a un enemigo fuera de alcance o con la línea bloqueada, el HUD lo indica. La máscara se calcula al cambiar selección/acción, no durante cada fotograma de cámara o movimiento.

## Despliegue inicial

El array `worldUnitStarts` está en `src/scenario.c`; sus dimensiones y el objetivo central se declaran en `src/scenario.h`. Hay 60 unidades por equipo (120 en total). Azul empieza en el cuadrante noroeste y Rojo en el sureste, con una mezcla idéntica por bando: 16 infanterías, 16 exploradores, 14 ingenieros y 14 blindados.

Cada entrada sigue el orden `id`, `x`, `y`, `movementPointsPerTurn`, `type`, `team`. Las coordenadas son lógicas y empiezan en cero. Con `movementPointsPerTurn = 0` se usa el presupuesto predeterminado del tipo; cualquier valor no nulo lo sobrescribe. `worldUnitStarts`, el almacenamiento lógico y las cachés se dimensionan para 120 unidades.

Las posiciones iniciales se eligen sobre celdas transitables y no se solapan. `UnitController_init()` valida IDs, equipo, tipo, puntos, límites, transitabilidad y ocupación antes de ocupar las casillas. `tests/scenario_test.c` comprueba el despliegue de ambos equipos y la ubicación transitable del objetivo.

## Turnos y equipos

- El equipo Azul empieza en el turno 1 y usa la línea de paleta `PAL2`; el equipo Rojo usa `PAL3`.
- Los turnos de equipo alternan: Azul turno 1, Rojo turno 2, Azul turno 3, etc.
- Solo se pueden seleccionar y mover unidades del equipo activo.
- Al comenzar un turno, las unidades de ese equipo recuperan sus puntos máximos; las unidades del equipo inactivo tienen cero puntos disponibles. Repetir la inicialización del mismo equipo y número de turno se rechaza y no recarga las acciones.
- Una unidad puede ejecutar una sola orden de movimiento por turno. Al completar la ruta se consumen los puntos restantes, aunque el coste de la ruta sea menor que el presupuesto inicial.
- Al completar cada casilla, se descuentan los puntos del terreno de destino. Una ruta cuyo coste supere los puntos disponibles se rechaza. Si una ruta se bloquea antes de abandonar el origen lógico, la unidad conserva su acción y puede elegir otra ruta; si ya llegó a una o más celdas nuevas, la acción queda consumida y no puede encadenar otra orden ese turno.
- `START` termina el turno, siempre que no haya una unidad en plena animación.

Los gráficos de unidad usan los índices de píxel 3 y 4 para que la línea de paleta del equipo determine el color. PAL3 conserva el blanco usado por el HUD en los índices de texto.

## Añadir una unidad de un tipo existente

1. Añade una entrada a `worldUnitStarts` con ID único, coordenadas libres/transitables, presupuesto opcional y `TEAM_BLUE` o `TEAM_RED`.
2. Usa `0` en `movementPointsPerTurn` para el presupuesto del tipo, o un valor explícito para sobrescribirlo.
3. Ejecuta `sh tests/run_tests.sh` con GCC y compila la ROM con SGDK.

## Añadir un tipo nuevo

1. Añade su identificador y actualiza `UNIT_TYPE_COUNT` en `src/unit_types.h`.
2. Añade nombre corto/largo, rol, presupuesto de movimiento, vida máxima, ataque, defensa, alcance de ataque, radio de visión, alcance/cantidad de reparación y un coste por terreno en `terrainMoveCosts` de `src/unit_types.c`. El coste 0 significa que esa clase no puede entrar en ese terreno. Usa cero para alcance y cantidad de reparación si la clase no repara.
3. Crea un PNG indexado de exactamente 16×16 píxeles (2×2 tiles), con el índice 0 transparente y los píxeles de unidad en los índices 3 y 4, y declara `SPRITE` con dimensiones `2 2` en `res/resources.res`.
4. Añade el caso de recurso en `getUnitSpriteDefinition()` de `src/unit_view.c` y conserva `UNIT_SPRITE_SIZE_PIXELS` en 16 para que el dibujo quede centrado en la celda.
5. Comprueba el espacio entre `UNIT_VRAM_TILE_INDEX` y `CURSOR_VRAM_TILE_INDEX`. `UNIT_VRAM_TILE_COUNT` se calcula a partir de `UNIT_TYPE_COUNT`.
6. Añade pruebas de selección de sprite y presupuesto de movimiento.

El VDP de Mega Drive admite hasta 80 entradas de sprite. Las máscaras del viewport se dibujan en BG_B con tiles estáticos; no ocupan entradas SAT. El motor mantiene hasta 120 unidades lógicas, pero solo crea como máximo 77 sprites activos para dejar tres entradas a cursor, objetivo y reserva de niebla. Si hay más unidades visibles que entradas disponibles, se priorizan la unidad en movimiento, la seleccionada y las del equipo activo. El límite de sprites por línea debe validarse en el emulador.

## Selección y órdenes

- Pulsa `A` sobre una unidad aliada para seleccionarla.
- Mueve el cursor para consultar el coste y los pasos del destino en el HUD. Esa consulta lee el alcance precalculado; no busca una ruta. La ruta concreta se calcula al confirmar con la segunda A.
- Pulsa `A` en el destino para prepararla; pulsa `A` otra vez sobre el mismo destino para confirmarla.
- Si mueves el cursor, se invalida la confirmación pendiente. `B` cancela la planificación y la selección.
- Durante la animación no se aceptan órdenes nuevas. Si una casilla de la ruta queda bloqueada antes del siguiente tramo, la unidad se detiene centrada en su casilla actual.

## Combate y niebla de guerra (fase 6)

- Durante tu turno, selecciona una unidad aliada y pulsa `A` sobre un enemigo visible. El ataque solo funciona a distancia Manhattan menor o igual que `attackRange` y si existe línea de visión. Con un ingeniero seleccionado, pulsa `A` sobre un aliado herido adyacente para reparar hasta 3 PV; los aliados reparables se resaltan con la paleta de alcance. Bosques y piedras bloquean los disparos cuando están entre atacante y objetivo; la casilla objetivo puede ser bosque o piedras y seguir siendo atacable.
- El daño es `max(1, ataque del atacante - defensa del defensor)`, limitado a los puntos de vida que le quedan al objetivo. Una unidad derrotada se elimina de la ocupación lógica y deja de aparecer en pantalla.
- Cada unidad dispone de una acción por turno: atacar o reparar. Un ingeniero no puede repararse a sí mismo, reparar a distancia ni superar la vida máxima del aliado. Si el defensor sobrevive y su alcance le permite responder, realiza un contraataque automático, que no consume la acción propia del defensor. Atacar o reparar no gasta puntos de movimiento.
- `visionRange` define un cuadrado de alcance por distancia Chebyshev; dentro de ese cuadrado, bosque y piedras bloquean las celdas que quedan detrás. La casilla del propio obstáculo sigue siendo visible. Cada equipo mantiene su mapa explorado de forma independiente y conserva el terreno descubierto al perder visión.
- Las casillas desconocidas se oscurecen y el HUD no revela sus datos. Las casillas ya exploradas pero fuera de visión conservan el terreno recordado y ocultan la ocupación actual. Los enemigos solo se dibujan si están en visión.
- La visión y los disparos respetan la oclusión por bosque y piedras. La IA enemiga está implementada con coordinación táctica ligera; siguen pendientes planificación profunda de varios turnos y balance avanzado. Consulta [`fase_6.md`](fase_6.md) y [`fase_7.md`](fase_7.md).
