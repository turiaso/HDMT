# Fase 5 · Equipos, turnos y objetivos

> Nota de alcance: este documento conserva el registro de las decisiones de la fase 5. Atributos de combate, ataques y niebla de guerra se incorporan posteriormente en [`fase_6.md`](fase_6.md).

## Objetivo de la fase

Convertir el desplazamiento de la fase 4 en un ciclo básico de partida: dos equipos con turnos alternos, presupuesto de movimiento por unidad, consumo según terreno y un objetivo de misión que termina la partida.

## Equipos y paletas

| Equipo | Identificador | Línea de paleta | Unidades iniciales |
|---|---|---|---|
| Azul | `TEAM_BLUE` | `PAL2` | Infantería y explorador |
| Rojo | `TEAM_RED` | `PAL3` | Ingeniero y blindado |

Los sprites de unidad utilizan índices de píxel 3 y 4. Las líneas PAL2/PAL3 les aplican tonos azules o rojos sin duplicar los recursos gráficos. PAL1 es una copia animada de la paleta de terreno PAL0 y se asigna solo a tiles alcanzables; PAL3 mantiene el blanco necesario para el texto del HUD. En la edición actual, las máscaras de viewport se dibujan como tiles estáticos en BG_B, no como sprites hardware. No se cambia la inicialización de fuente SGDK.

## Ciclo de turnos

1. La partida empieza con Azul en el turno 1.
2. Solo pueden seleccionarse unidades del equipo activo.
3. `START` termina el turno si no hay una unidad en animación. Una ruta solo se completa o se interrumpe por validación antes de permitir terminar el turno.
4. El siguiente equipo pasa a estar activo y aumenta el número de turno: Azul 1 → Rojo 2 → Azul 3 → Rojo 4.
5. Las unidades del equipo que comienza recuperan `maxMovementPoints`; las del otro equipo tienen cero puntos disponibles. Se limpian la selección y la ruta pendiente al cambiar de turno.
6. `turnEnd` se notifica para el equipo saliente y `turnStart` para el entrante mediante `CellEventContext`, que incluye equipo y número de turno.

El número es el identificador de cada turno de equipo, no un contador de rondas de dos equipos. Si alcanza `0xFFFF`, se rechaza el siguiente cambio en vez de desbordarlo.

## Puntos de movimiento

Cada unidad tiene:

- `maxMovementPoints`: máximo configurado por tipo o por instancia.
- `movementPoints`: presupuesto que queda en el turno actual.
- `UnitStart.movementPointsPerTurn`: `0` usa el valor por defecto de su tipo; un valor positivo lo sobrescribe.

Valores iniciales por tipo: infantería 6, explorador 8, ingeniero 5 y blindado 7. En la implementación original de esta fase todos usaban un coste de terreno común. La regla vigente posterior es por clase y se centraliza en `src/unit_types.c`; véase [`unidades.md`](unidades.md) para la tabla actual. El presupuesto se expresa en costes de terreno, no en casillas. Dijkstra calcula la ruta con los puntos actuales. El gasto se aplica al llegar al centro de cada casilla, descontando el coste de entrada en esa casilla. Un destino que exceda los puntos disponibles se rechaza; una unidad con cero puntos no puede emitir otra orden de movimiento hasta su próximo turno.

Regla vigente de acción: cada unidad puede completar una sola orden de movimiento por turno. Al completar una ruta se agotan los puntos restantes, aunque el coste real sea inferior al presupuesto inicial. Si una ruta se aborta tras avanzar al menos una celda lógica, también se consume la acción; si se aborta antes de abandonar el origen, se reconstruye el alcance y se puede volver a planificar sin reseleccionar la unidad. Una segunda llamada para iniciar de nuevo el mismo turno del mismo equipo se rechaza para evitar recargar accidentalmente sus puntos.

Estado histórico al cerrar esta fase: las clases aún compartían las mismas reglas de terreno. Las versiones posteriores añadieron costes de movimiento por combinación de clase y terreno; la tabla vigente está documentada en [`unidades.md`](unidades.md).

## Misión del ejemplo

En la versión actual, el `MissionObjective` llamado `PUNTO DE CONTROL` está en las coordenadas lógicas `(20, 20)` —el HUD muestra `21,21` porque usa coordenadas de base uno—. El registro original de la fase 5 usaba `(10,10)`; el objetivo se trasladó al cruce central al ampliar el escenario a 40 × 40. Se representa con el sprite de marco de casilla y permanece en su posición mundial mientras se desplaza la cámara.

La condición es captura: el primer equipo que coloque una unidad en el objetivo gana inmediatamente. La comprobación ocurre al completar cada casilla, por lo que, si una ruta pasa por `(20, 20)` aunque su destino quede más lejos, la partida termina en ese punto y la unidad se detiene ahí. El HUD muestra explícitamente la captura y el equipo ganador/derrotado. No se aceptan más órdenes después de terminar la misión.

Para otro escenario, define otro `MissionObjective` y pásalo a `GameTurn_init()`. El objetivo debe estar dentro del mapa y sobre terreno transitable. El primer prototipo admite un único objetivo de captura.

## Controles

- Cruceta: mover cursor; `C` + cruceta: desplazar cámara. En exploración libre, cámara y cursor se mueven suavemente; al elegir destino, el cursor y el auto-scroll avanzan por celdas completas para mantenerlo alineado con la máscara.
- `A`: seleccionar una unidad aliada; el mapa ilumina las casillas alcanzables, y el cursor queda restringido a ese conjunto (con posibilidad de volver a la casilla de la unidad).
- `A` sobre una casilla alcanzable prepara la orden; `A` otra vez en la misma casilla la confirma. El HUD muestra el coste/los pasos, pero no se dibuja la ruta. Al completarse el movimiento, la unidad se deselecciona automáticamente y el glow queda apagado.
- `B`: cancelar la planificación y quitar la selección; el brillo desaparece. No interrumpe un movimiento ya iniciado.
- `START`: finalizar el turno del equipo activo, cuando no hay animación en curso.

El HUD muestra el equipo/turno activos, el objetivo en coordenadas de base uno y, cuando hay una unidad seleccionada, sus puntos actuales y máximos. La vista previa muestra el coste de la ruta frente a los puntos que quedan.

## Módulos

- `src/team.c/.h`: identificadores de equipo, metadatos y carga de paletas.
- `src/game_turn.c/.h`: alternancia de turnos, recarga de puntos, eventos de turno y condición de misión.
- `src/unit_controller.c/.h`: selección por equipo, presupuesto, gasto por casilla y limpieza automática de la selección al completar una orden. Al seleccionar una unidad calcula explícitamente el alcance una vez; el hover y el HUD solo consultan la caché y jamás reconstruyen el alcance. La ruta concreta se calcula una única vez al confirmar el destino con la segunda A. Si una ruta se bloquea, conserva la selección; la acción de movimiento solo queda disponible para replanificar cuando la unidad no había abandonado su celda de origen. Si ya avanzó a otra celda, los puntos se agotan hasta el siguiente turno propio.
- `src/unit_view.c/.h`: sprites de unidades, selección de PAL2/PAL3 según equipo y sincronización con cámara. Ya no crea marcadores de ruta.
- `src/movement_glow.c/.h`: duplica la paleta base del terreno en PAL1, mantiene un pulso de brillo y parchea el atributo de paleta de los tiles al actualizar el mapa. El callback convierte coordenadas de tile de 8×8 a casillas lógicas de 24×24; al cambiar la máscara hace un redibujado forzado para actualizar también los tiles que ya estaban visibles.
- `src/pathfinder.c/.h`: separa el cálculo de alcance (una vez al seleccionar) de la ruta concreta (solo al confirmar destino). La máscara tiene en cuenta costes ponderados, agua, ocupación y presupuesto. Las consultas de coste/pasos reutilizan el resultado en O(1), sin ejecutar búsquedas al mover el cursor. El origen no se ilumina, aunque el cursor puede regresar a él.
- `src/cell_hud.c/.h`: estado de turno y presupuesto visible; las filas de datos de la celda solo se escriben si ha cambiado su texto.

En el comportamiento del cursor hay dos modos: sin unidad seleccionada conserva el movimiento suave por píxeles y la cámara suave; con una unidad seleccionada salta una celda por paso y queda limitado a su alcance. En el modo destino, el auto-scroll también avanza por celdas completas (cada `CAMERA_TARGET_SCROLL_INTERVAL` fotogramas) y valida la celda que quedaría bajo el cursor antes de mover la cámara. Si solo queda un resto menor que una celda al alcanzar el borde físico del mapa, se compensa con el cursor para conservar su celda lógica. Así, la esquina del cursor permanece alineada con las celdas y no puede verse o seleccionar una casilla fuera del alcance mediante el scroll. La cadencia del scroll en modo destino es independiente de la exploración libre.

## Verificación

`tests/camera_cursor_test.c` comprueba también la alineación en cada fotograma mientras se desplaza automáticamente por una máscara horizontal y vertical, los límites de la máscara y ambos bordes físicos del mapa, incluyendo cámaras que arrancan con un desplazamiento subcelda. `tests/unit_controller_test.c` incluye una regresión que verifica que mover el cursor por muchas celdas, incluso con la caché invalidada de forma artificial, no inicia una búsqueda ni modifica los búferes del buscador; la primera A tampoco sobrescribe esos búferes y la ruta se calcula al confirmar con la segunda A. `tests/team_palette_test.c` comprueba los metadatos y diferencias de las paletas. `tests/game_turn_test.c` comprueba el cambio de equipo/turno, los eventos, recarga y agotamiento de puntos, el coste de camino/bosque, la selección de unidades enemigas y la victoria al alcanzar el objetivo. `tests/movement_range_test.c` comprueba el alcance ponderado, el agua y el bloqueo por ocupación; `tests/camera_cursor_test.c` comprueba que el cursor solo entra en celdas permitidas y lo hace en saltos completos; `tests/movement_glow_test.c` comprueba la copia/animación de PAL1 y el parcheo correcto por filas y columnas. `tests/unit_controller_test.c` comprueba además que la máscara se limpia durante el movimiento, que una orden completada borra automáticamente la selección y que los puntos restantes solo vuelven a usarse tras seleccionar de nuevo la unidad; los bloqueos intermedios conservan la selección para permitir replanificar.

Ejecuta `sh tests/run_tests.sh` con GCC o `tests\run_tests.bat` en Windows. Las pruebas host no sustituyen la compilación con tu instalación real de SGDK ni la comprobación visual de paletas, fuente y HUD en BlastEm.
