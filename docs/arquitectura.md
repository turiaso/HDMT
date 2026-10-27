# Arquitectura actual

## Flujo de una orden

```text
Entrada (main.c)
  -> UnitController (selección, presupuesto, ruta y ocupación)
  -> Pathfinder (alcance o ruta usando workspace reutilizable)
  -> CellEvents (transiciones de celda)
  -> FogOfWar (visión lógica y celdas sucias)
  -> MovementGlow / UnitView / CellHud (presentación localizada)
  -> SPR_update + SYS_doVBlankProcess (publicación del frame)
```

`main.c` coordina los sistemas y los estados de turno. No debería alojar algoritmos del pathfinder, reglas de combate ni cálculos internos de niebla. `updateGameFrame()` está dividido en pasos identificables para el trabajo previo, la acción del equipo, la sincronización de turno, el trabajo aplazado y la presentación.

## Módulos y propietario de cada dato

| Módulo | Responsabilidad / fuente autoritativa |
|---|---|
| `main.c` | Bucle principal, inicialización, integración de subsistemas, turno espectador y presentación de la partida. |
| `scenario_menu.c` | Selector de escenarios al arrancar; navegación y confirmación sin inicializar buffers de partida. |
| `scenario.c` | Registro de mapas, despliegues, nombre/ayuda de menú y objetivo de misión. |
| `game_screens.c` | Máscaras de viewport y pantalla de resultado; recibe el estado por parámetro. |
| `game_config.h` | Geometría de tiles, posiciones de VRAM, parámetros de cámara/HUD y límites de hardware. |
| `map_data.c` | Terreno, ocupación, datos de celda, transitabilidad, defensa y línea de visión. No dibuja. |
| `unit_types.c` | Estadísticas por clase: coste de movimiento, vida, ataque, defensa, alcance, visión y capacidad de reparación. |
| `unit_controller.c` | Estado autoritativo de unidades, IDs, selección, presupuesto, órdenes, movimiento lógico y ocupación. |
| `pathfinder.c` | Búsqueda ponderada, alcance y consulta de ruta. No decide órdenes ni dibuja. |
| `cell_events.c` | Despacho de eventos de celda/unidad/turno a partir de perfiles compartidos. |
| `game_turn.c` | Equipo activo, recarga de puntos, progreso de misión y resultado (victoria o empate). |
| `combat.c` | Valida ataques y reparaciones, predice daño/represalia sin mutar el estado y aplica las acciones confirmadas. Atacar o reparar consume la misma acción de turno. |
| `enemy_ai.c` | Una decisión táctica roja por llamada; solo usa rivales visibles para Rojo. |
| `fog_of_war.c` | Visión separada por equipo, exploración persistente, snapshots y lista de celdas sucias. |
| `camera_cursor.c` | Movimiento de cursor/cámara; acepta una máscara de alcance y no depende de unidades concretas. |
| `unit_view.c` | Traduce unidades a sprites, gestiona la SAT, cachés, índice espacial y prioridades visuales. |
| `movement_glow.c` | Estilos de tile para alcance/ataque/niebla, animación de paleta y parcheo del mapa gráfico. |
| `cell_hud.c` | Presentación de textos/atributos; evita volver a escribir líneas que no cambian. |
| `performance.c` | Contadores opcionales de trabajo para diagnosticar coste por fase. |
| `mission_result.c` | Resolución pura de la entrada en la pantalla final. |

## Contratos que no deben romperse

### Lógica y representación

- `MapData` y `UnitController` son autoritativos. Un sprite no puede decidir si una celda está ocupada ni corregir una posición lógica.
- La celda mide 24 × 24 px; cada unidad gráfica mide 16 × 16 px. La posición de render se calcula a partir de la posición lógica y la cámara.
- El VDP admite 80 entradas SAT en el modo previsto por el proyecto. Tres están reservadas para cursor, objetivo y niebla; quedan 77 instancias de unidad. Las 120 unidades siguen existiendo aunque no todas tengan sprite asignado.
- El límite horizontal de sprites por línea es independiente del límite total de SAT y debe probarse visualmente en BlastEm.

### Pathfinding

- El escenario de juego tiene como máximo 1.600 celdas. No cambiar ese límite sin revisar `MapData`, `PathfinderWorkspace`, `FogOfWar`, `MovementGlow`, `UnitView` y las pruebas.
- El workspace se reutiliza para no duplicar buffers grandes de RAM. Una ruta confirmada se consume usando el resultado guardado; no iniciar otra búsqueda sobre el mismo workspace hasta acabar de consumirla.
- El hover solo consulta la caché de alcance. El cálculo de alcance se hace al seleccionar/actualizar la selección, y la ruta concreta al confirmar la orden.

### Niebla y gráficos

- Cada equipo tiene su propio estado de visión. `EXPLORED` no implica `VISIBLE`; una unidad enemiga no visible no puede ser seleccionada ni atacada con información oculta.
- `FogOfWar` publica los índices de celdas sucias. Las celdas contiguas se agrupan cuando se actualizan tiles. No introducir reconstrucciones completas de mapa/sprites en un bucle por frame sin una razón medida.
- El modo espectador presenta la unión de información visible por ambos equipos, pero no fusiona sus estados lógicos persistentes.

### Combate e IA

- Jugador, IA, predicción y HUD deben usar `Combat_predict*()` y `MapData_hasLineOfSight()` compartidos; no dupliques reglas de rango/daño en la interfaz.
- `EnemyAI_update()` debe realizar una sola acción por llamada. La animación continúa en el bucle común y no debe convertirse en un bucle bloqueante.
- La IA usa el alcance de ataque de la unidad que se mueve, nunca el alcance del objetivo. La puntuación de disparo solo favorece una casilla si el blanco está dentro del alcance propio y hay línea de visión.
- La puntuación táctica por terreno solo compara casillas ya alcanzables; no sustituye el coste de entrada calculado por `Pathfinder_*ForType`. Los corredores de captura mantienen prioridad sobre las preferencias opcionales por cobertura/tiro.
- Si no ha seleccionado un enemigo visible, el ingeniero puede desplazarse hacia el aliado más necesitado de reparación. La reparación real sigue validándose en `combat.c` al llegar.
- La IA no puede usar posiciones de enemigos que Rojo no tenga visibles.

### Eventos y RAM

- Los eventos por celda usan índices a perfiles compartidos, no punteros guardados en `CellData`. Mantener esta estructura.
- Los escenarios comparten buffers de `MapData` y pathfinding de forma deliberada; solo una partida se inicializa a la vez.
- No aplicar las propuestas de optimización de RAM A/B/C sin decisión explícita. Si se cambia una estructura, documentar el coste de RAM estática y actualizar las pruebas de capacidad.

## Contrato de capacidad y memoria

`MapData` es un contenedor lógico reutilizable y valida dimensiones, filas y capacidad de su buffer; no impone por sí mismo el tamaño de la interfaz. Los subsistemas de la partida que reservan memoria estática comparten `GAMEPLAY_MAP_MAX_SIDE` y `GAMEPLAY_MAP_MAX_CELLS`, declarados en `map_data.h`. Antes de ampliar el tamaño de una misión hay que revisar, como conjunto, el workspace de búsqueda, la niebla, el glow, el índice espacial y sus pruebas. El límite es una decisión de memoria/hardware del juego, no una regla de terreno.

`GameTurn_checkObjective()` cuenta los supervivientes de ambos bandos en una sola pasada y resuelve el estado de ambos equipos vacíos como empate. `GameTurn_endTurn()` despacha `CELL_EVENT_TURN_END` antes de evaluar y conceder objetivos temporizados. `GameTurn.activeTurnEndDispatched` evita que un cierre o una victoria duplique el evento. `requiredObjectiveTurns()` centraliza el valor predeterminado de una ronda para los objetivos temporizados; no dupliques esa regla en la presentación. Ver [corrección del sistema de objetivos](correccion_objetivos_mision.md) para el contrato completo y sus pruebas.

## Geometría y VRAM

- Tile físico: 8 × 8 px; casilla lógica: 3 × 3 tiles = 24 × 24 px.
- Pantalla: 320 × 224 px; área útil alineada a celdas: 312 × 216 px.
- Los mapas visuales actuales miden 968 × 968 px: 40×40 celdas más un margen técnico de 8 px.
- `TERRAIN_VRAM_TILE_INDEX` debe mantenerse alineado con el atlas de terreno y con la reserva de la fuente de SGDK. No sustituirlo por `TILE_USER_INDEX` sin revisar asignación de VRAM.
- La cabecera del módulo debe garantizar la inclusión de `genesis.h` antes de `<string.h>`; el `string.h` de SGDK usa tipos como `u16` y `s8`.

## Perfiles de rendimiento

`PERF_DIAGNOSTICS` es opt-in y está desactivado por defecto. Al activarlo, el overlay `VDP_showFPS` permanece en pantalla y los logs `PERF` agregan contadores por fase. Los logs cuantifican trabajo y no ciclos de CPU; imprimir puede producir un pico puntual. No comparar builds con y sin diagnóstico como si fueran equivalentes.


## Costes de movimiento por clase

`src/unit_types.c` define `terrainMoveCosts` junto al resto del perfil de cada clase. Un valor cero declara ese terreno intransitable para la clase; los valores positivos son el coste de entrar en la celda de destino. `UnitController` usa ese coste al validar y consumir cada paso, `Pathfinder_*ForType` lo usa al calcular ruta y alcance, `FogOfWar` incluye el tipo en cada snapshot para explorar su rango correcto y el HUD consulta el mismo perfil al inspeccionar terreno con una unidad seleccionada. El coste genérico `TerrainInfo.moveCost` representa el perfil de infantería y solo debe usarse como referencia cuando no hay una clase seleccionada.
