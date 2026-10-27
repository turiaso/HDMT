# Fase 4 · Rutas y costes de movimiento

> **Nota de evolución:** este documento conserva el registro histórico de la interfaz de la fase 4. En la versión actual de la fase 5, los marcadores que se describen aquí se han eliminado: al seleccionar una unidad, el alcance válido se muestra mediante el brillo pulsante de PAL1 y el cursor solo puede recorrer las casillas alcanzables. La ruta sigue calculándose internamente para desplazar la unidad. Para el comportamiento actual, consulta `docs/fase_5.md`.

## Incluido

- Dijkstra ponderado sobre las celdas del `MapData` activo.
- Costes actuales: camino = 1, bosque = 2, piedras = 2, agua = impasable.
- Cada `UnitStart` define `movementPointsPerTurn`, máximo de puntos que se recargan al comenzar el turno de su equipo. Los máximos predeterminados son infantería 6, explorador 8, ingeniero 5 y blindado 7; el valor puede sobrescribirse por instancia. La fase 5 descuenta estos puntos al entrar en cada casilla.
- La vista previa se actualiza cuando cambia la casilla bajo el cursor. Se dibujan marcos solo en las celdas intermedias que caben dentro del área jugable; el cursor principal marca el destino. Los marcadores fuera de vista o que invadirían las franjas enmascaradas se ocultan y se aparcan fuera de pantalla. El HUD muestra pasos y coste total frente al alcance.
- La primera pulsación de A sobre un destino válido prepara la ruta; una segunda pulsación de A sobre el mismo destino la confirma. Mover el cursor invalida la confirmación anterior. B cancela la planificación y deselecciona la unidad; la ruta y sus marcadores se limpian. B no interrumpe un desplazamiento ya iniciado.
- Al confirmar el destino, el sprite avanza 4 píxeles por fotograma y recorre la ruta una casilla cada 6 fotogramas. La ocupación lógica y las coordenadas de la unidad se actualizan al llegar exactamente al centro de cada nueva casilla; el sprite se representa desde esa posición lógica más el desplazamiento interpolado del tramo.
- Antes de iniciar cada tramo se comprueba que la unidad sigue ocupando su celda y que la siguiente está libre y es transitable. Si una casilla siguiente se bloquea entre tramos, la unidad se detiene centrada en su casilla actual, conserva su ocupación y queda seleccionada para poder recalcular una ruta.
- La vista detecta también la transición de movimiento activo a detenido. Cuando una ruta termina o se aborta, fuerza una pasada de sincronización para colocar el sprite en el centro de la casilla lógica válida; esto cubre tanto la llegada al destino como un obstáculo que aparece en el último fotograma del tramo. Esta pasada ocurre una sola vez por final de movimiento, no en cada fotograma.
- Mientras dura la animación no se aceptan selecciones, nuevas órdenes ni cancelaciones. El siguiente tramo se valida antes de empezar y se vuelve a validar al llegar. Solo entonces se ejecuta `beforeLeave -> leave -> beforeEnter -> enter`, con `CellEventContext.source = CELL_EVENT_SOURCE_UNIT`; el cambio de ocupación se hace entre `leave` y `enter`. Después se emiten `unitLeave` y `unitEnter`. Un paso rechazado no dispara `leave`, `beforeEnter`, `enter` ni los eventos específicos de unidad. Los callbacks de transición son notificaciones y no deben modificar terreno u ocupación durante esa transición.
- Una ruta evita agua, otras unidades y casillas que no puedan atravesarse. Los destinos que superan los puntos disponibles o no conectan con el origen se rechazan.

## Memoria en la versión actual

`PathfinderWorkspace` usa tres buffers de `width * height` bytes: costes/direcciones, dirección al predecesor y máscara de alcance. El buffer de costes se reutiliza para almacenar la ruta en orden inverso; la máscara de alcance permanece separada para no sobrescribir los datos que usa la animación. Para el escenario de 20×20 son 1.200 bytes de espacio de trabajo. El resumen de dos buffers de versiones anteriores queda obsoleto. Los mapas destinados a unidades deben proporcionar su propio workspace con capacidad suficiente.

## Controles históricos de la fase 4

Estos pasos describen la interfaz original de esta fase; para los controles vigentes, consulta `docs/fase_5.md`.

1. Pulsa A sobre una unidad para seleccionarla.
2. Mueve el cursor: aparece la ruta de menor coste si el destino está al alcance.
3. Pulsa A para preparar la ruta; el HUD indica que espera confirmación.
4. Pulsa A de nuevo en el mismo destino para mover la unidad. Si mueves el cursor, debes preparar el nuevo destino de nuevo.
5. Pulsa B para cancelar la planificación y la selección. Durante la animación no se aceptan órdenes nuevas.

## Pruebas

- `tests/pathfinder_test.c`: ruta ponderada; costes acumulados al entrar en bosque, camino y piedras; presupuesto exacto e insuficiente; agua, ocupación, límites de fila, coordenadas y capacidad.
- `tests/unit_controller_test.c`: tipos y alcances por defecto, preparación/confirmación/cancelación, interpolación, rechazo de órdenes, bloqueo inesperado entre tramos, ocupación lógica, llegada centrada y orden/contexto exactos de los eventos de cada transición. Verifica que el tramo rechazado no genera eventos de salida/entrada.
- `tests/unit_view_test.c`: sprite propio por tipo, visibilidad de marcadores, salida del viewport, bordes de máscaras, ausencia de escrituras redundantes y liberación idempotente ante fallos de inicialización.
- En la versión original de esta fase, cada tipo de unidad reservaba 9 tiles en VRAM. En el escenario vigente los sprites de unidad miden 16 × 16 px y reservan 4 tiles por tipo. El rango se sitúa entre el atlas de terreno y el cursor; `UNIT_VRAM_TILE_COUNT` se deriva del número de tipos.
- En cada transición válida, `beforeLeave` y `leave` observan todavía ocupada la casilla de origen. Después de `leave` se libera el origen; `beforeEnter` observa el destino libre; antes de `enter` se ocupa el destino y se actualizan las coordenadas lógicas. `unitLeave` y `unitEnter` se notifican después de la transición completa.
- `UNIT_MOVE_SPEED_PIXELS` debe dividir exactamente `CELL_PIXELS`; el compilador valida esta condición para evitar sobrepasar las esquinas de una ruta.
- El HUD tiene un refresco pasivo de la casilla que actualiza ocupación sin disparar eventos `stay`; durante la animación, el ciclo del cursor queda suspendido. Ejecutar `sh tests/run_tests.sh` o `tests\run_tests.bat`. La comprobación host no reemplaza la compilación con SGDK ni la inspección visual en BlastEm.

## Compatibilidad de cabeceras SGDK

`pathfinder.c` incluye primero `pathfinder.h`, que a su vez incluye `genesis.h`, y después incluye `string.h`. En SGDK, `string.h` declara funciones con tipos como `u16` y `s32`; por eso no debe incluirse antes de los tipos de SGDK. `tests/sgdk_include_order/string.h` emula esa dependencia para detectar regresiones en el orden de inclusión.
