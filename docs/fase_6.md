# Fase 6 · Atributos, combate y niebla de guerra

## Estado

La implementación lógica y las pruebas de host están integradas. **La fase no se considera validada visualmente hasta compilar con la instalación real de SGDK y jugar la ROM en BlastEm.** Las pruebas actuales se compilan con GCC y stubs; no comprueban DMA, CRAM ni renderizado real del VDP.

## Escenario ampliado

El mapa actual tiene 40 × 40 casillas (1.600 en total) y conserva las dimensiones lógicas por instancia. El mapa gráfico mide 968 × 968 px: 40 × 40 celdas de 24 px más el margen técnico de 8 px que usa el scroll. Cuatro cuadrantes basados en el mapa original se conectan con un cruce central de caminos. El objetivo está en `(20,20)`.

El despliegue inicial contiene 60 unidades por bando, 120 en total. Cada equipo tiene 16 infanterías, 16 exploradores, 14 ingenieros y 14 blindados. `src/scenario.c` contiene las posiciones editables; `tests/scenario_test.c` valida ID único, terreno transitable, ausencia de solapamientos, equilibrio de tipos y presupuesto de sprites. Los cuatro recursos de unidad son imágenes de 16 × 16 px (2 × 2 tiles), centradas dentro de una celda lógica de 24 × 24 px. La edición completa dibuja las máscaras de borde con tiles estáticos de BG_B, liberando las 17 entradas SAT que antes ocupaban. El motor conserva 120 unidades lógicas y limita el renderizado a 77 sprites de unidad, además de tres sprites fijos. El límite por línea debe verificarse en BlastEm.

## Atributos por tipo

Los perfiles se editan en un único lugar: `src/unit_types.c`, mediante `UnitTypeInfo`.

| Tipo | Mov. | PV máximos | Ataque | Defensa | Alcance ataque | Visión | Alcance reparación | PV reparados |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Infantería | 6 | 10 | 4 | 2 | 1 | 3 | — | — |
| Explorador | 8 | 7 | 3 | 1 | 1 | 4 | — | — |
| Ingeniero | 5 | 8 | 2 | 2 | 1 | 2 | 1 | 3 |
| Blindado | 7 | 14 | 6 | 3 | 2 | 3 | — | — |

Los valores vigentes viven en `src/unit_types.c` y se copian al estado de la unidad durante el despliegue. El movimiento es un presupuesto de coste de terreno por turno. El HUD muestra un resumen como `INF HP 10/10 AT 4 DF 2 AR1 V3`; en el ingeniero añade `REP1/3`, es decir, alcance de reparación 1 y hasta 3 PV restaurados por acción.

## Controles de combate

1. Selecciona una unidad de tu equipo con `A`.
2. Mueve el cursor hasta un enemigo visible.
3. Pulsa `A` para atacar. El ataque se ejecuta inmediatamente, sin un diálogo de confirmación adicional.
4. Si el defensor sigue vivo y su alcance permite responder, realiza un contraataque automático.

`B` sigue cancelando la selección/planificación; `START` cambia de turno cuando no hay movimiento en curso. Los ataques no consumen puntos de movimiento, por lo que una unidad puede moverse y atacar en el mismo turno según el orden de las acciones. Cada unidad dispone de una acción de combate por turno: atacar o reparar, nunca ambas; al comenzar su siguiente turno se restablece la acción. Una represalia no consume el ataque propio del defensor.

### Reparación del ingeniero

- Selecciona un ingeniero y pulsa `A` sobre un aliado herido adyacente.
- La reparación restaura hasta 3 PV, sin superar la vida máxima. No permite repararse a sí mismo ni reparar a distancia.
- Reparar consume la misma acción que atacar, pero no gasta puntos de movimiento.
- El HUD del cursor indica si el aliado está fuera de alcance, a vida completa o si el ingeniero ya actuó. Los aliados reparables usan el resaltado de alcance; los enemigos atacables conservan el rojo.
- La IA roja repara al aliado adyacente con mayor pérdida proporcional de PV cuando no dispone de un ataque válido. Si tiene un ataque válido, conserva la prioridad de atacar.

### Resolución

- El alcance se mide por distancia Manhattan: `abs(dx) + abs(dy) <= attackRange`.
- Daño base: `max(1, attack - defense)`.
- Los PV nunca bajan de cero y el daño reportado no supera los PV que le quedaban al objetivo.
- Si una unidad llega a cero PV, se elimina del estado activo y se libera su casilla en la ocupación lógica. No puede ser seleccionada, moverse, revelar visión ni ganar la misión.
- Se aplica cobertura defensiva del terreno y línea de visión: las casillas intermedias de bosque o piedras bloquean visión y disparo. Siguen fuera de alcance el azar, los críticos, la experiencia y las habilidades especiales por tipo.

## Niebla de guerra

La niebla se mantiene de forma independiente para Azul y Rojo:

- **No explorada:** tile oscurecido; el HUD no revela el terreno ni la ocupación.
- **Explorada, pero fuera de visión:** se conserva el terreno recordado; se oculta la ocupación actual y no se dibujan enemigos.
- **Visible:** se muestra el terreno real y se pueden ver las unidades enemigas en las celdas visibles.

Cada unidad tiene un cuadrado de alcance centrado en su posición, con radio `visionRange` medido por distancia Chebyshev (`max(abs(dx), abs(dy))`). Dentro de ese cuadrado solo se revela una casilla si existe línea de visión desde la unidad: bosque y piedras bloquean las casillas que quedan detrás, pero se puede ver la propia casilla de bosque o piedras. La visión se recalcula después de mover una unidad o eliminarla; las casillas exploradas se conservan. El recorrido `MapData_hasLineOfSight()` usa enteros y es simétrico. Si el rayo pasa exactamente por una esquina, avanza en diagonal: las celdas laterales que solo toca por el borde no bloquean, mientras que una celda opaca atravesada por el interior sí lo hace. Esta regla permite mantener corredores diagonales; está cubierta por una prueba específica.

El HUD oculta la información de las celdas no exploradas y la ocupación de las celdas exploradas fuera de visión. La vista de unidades oculta enemigos no visibles para el equipo activo. Durante el cálculo de alcance y ruta, el controlador enmascara temporalmente ocupantes enemigos ocultos y restaura enseguida la ocupación autoritativa. Al ejecutar la ruta, una unidad real sigue bloqueando el paso; la niebla no cambia las reglas reales de colisión.

## Implementación por módulo

- `src/unit_types.c/.h`: perfiles con movimiento, PV, ataque, defensa, alcance de ataque, visión y capacidad de reparación.
- `src/unit_controller.c/.h`: estado de combate por unidad, acciones de ataque/reparación usadas en el turno, eliminación de la unidad y ocultación de enemigos no visibles durante el cálculo de movimiento.
- `src/combat.c/.h`: validación compartida de ataque/reparación; predicción de daño/represalia, curación limitada y eliminación.
- `src/fog_of_war.c/.h`: actualización y consulta del estado de visión por equipo.
- `src/map_data.c/.h`: almacena el estado de visión empaquetado en cada celda, define qué terrenos bloquean visión y ofrece `MapData_hasLineOfSight()` para comprobar oclusión.
- `src/unit_view.c/.h`: muestra solo las unidades que el equipo activo puede ver.
- `src/movement_glow.c/.h`: muestra tiles oscuros en celdas no exploradas y restaura el terreno recordado en las exploradas.
- `src/cell_hud.c/.h`: oculta datos desconocidos y muestra los atributos de combate de la unidad seleccionada.
- `src/main.c`: coordina ataque, actualizaciones de niebla, HUD y vista.

## Optimización de rendimiento de la niebla

La versión inicial hacía trabajo proporcional a todo el mapa al actualizar la visión durante un movimiento. Ahora la visión inicial se construye una vez y `FogOfWar` almacena una instantánea compacta de las unidades. Al detectar un cambio, solo recorre la unión del área de visión antigua y nueva de las unidades modificadas. Si las unidades no han cambiado, `FogOfWar_update()` devuelve sin recorrer celdas.

`FogOfWar` expone la lista de celdas cuyo estado cambió y marca aparte si una celda pasó de desconocida a conocida (o al contrario), que es el único caso que exige volver a escribir el terreno gráfico. `main.c` parchea solo las celdas marcadas que intersectan el viewport mediante `MovementGlow_refreshFogCell()`, en vez de forzar `MAP_scrollToEx(..., TRUE)` en cada paso. Cambiar el equipo visible sigue haciendo un redibujado completo puntual, ya que cambia la perspectiva de la partida.

`UnitView_update()` sigue comprobando la vida y la visibilidad de las unidades, pero cachea su posición de pantalla y no llama a `SPR_setPosition()` ni repite el cambio de visibilidad si no han cambiado. El cursor y el objetivo también evitan escrituras repetidas de posición cuando sus coordenadas son idénticas.

La prueba de niebla incluye un movimiento que cambia más de 255 estados y una actualización estable posterior; esto protege el contador de lista frente a desbordamientos. La prueba de render asegura que una celda visible solo provoca tres transferencias de fila (las tres filas de tiles de la celda), mientras que una celda fuera de pantalla no envía ninguna. Ejecutar `sh tests/run_tests.sh`.

Estas pruebas demuestran reducción del trabajo lógico y del número de escrituras de tilemap en escenarios de host; no miden FPS reales. La compilación con SGDK local y el perfil visual en BlastEm siguen pendientes. Detalles y límites: [`historico/INFORME_OPTIMIZACION_RENDIMIENTO.md`](historico/INFORME_OPTIMIZACION_RENDIMIENTO.md).

## Uso de RAM

`CellData` ocupa 4 bytes por celda: terreno, ocupante, perfil de eventos y `visionState`. Los dos bits bajos almacenan el estado de Azul y los siguientes dos bits el de Rojo; los valores restantes quedan reservados. El mapa actual de 40 × 40 usa 6.400 bytes para `CellData`. Los tres búferes de pathfinding suman otros 4.800 bytes y la cola circular de la búsqueda 3.200 bytes; mapa, workspace y cola suman 14.400 bytes (14,06 KiB), antes de contar unidades y demás sistemas. No se ha aplicado ninguna de las optimizaciones de RAM A/B/C que se dejaron aplazadas.

## Pruebas

Ejecuta desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
```

En Windows, con GCC disponible en `PATH`:

```bat
tests\run_tests.bat
```

`tests/combat_test.c` cubre perfiles, daño, represalia, eliminación, alcance, equipo, máscara de reparación, límite de una acción por turno, reparación parcial y objetivo a vida completa. `tests/enemy_ai_test.c` comprueba que la IA repara al aliado adyacente con mayor daño; `tests/movement_glow_test.c` verifica el resaltado visual de aliados reparables. `tests/fog_of_war_test.c` cubre visión separada por equipo, exploración persistente, celdas ocultas, filtrado de objetivos de combate y ausencia de filtraciones de ocupación en las consultas de movimiento. Las pruebas de glow y HUD también comprueban la ocultación visual/lógica.

## Fuera de alcance de esta fase

La línea de visión y la cobertura por terreno se añadieron después de la fase original. Siguen pendientes reglas de armas y habilidades especiales, animaciones de daño, economía y equilibrio final de las clases. Tampoco se afirma que la ROM se haya compilado o probado visualmente: esa verificación necesita la instalación real de SGDK y BlastEm.
