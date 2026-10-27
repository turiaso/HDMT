# HUD: información de unidades en casillas ocupadas

## Comportamiento

Al colocar el cursor sobre una casilla ocupada y visible, la cuarta fila del HUD muestra clase, equipo y puntos de vida de la unidad. Ejemplos:

- `OCUPACION: BLD ROJO HP9/14`
- `OCUPACION: ING AZUL HP5/8`

La información procede de la unidad lógica consultada mediante `UnitController_getUnitAt()`, no de los sprites ni de su posición interpolada. La vista no deduce los datos a partir del identificador del ocupante.

## Protección de niebla de guerra

La comprobación de visión se realiza antes de buscar la unidad:

- Celda desconocida: `OCUPACION: DESCONOCIDA`.
- Celda explorada pero no visible: `OCUPACION: SIN VISION`.
- Celda visible: muestra clase, equipo y PV actuales.

Por tanto, inspeccionar una celda recordada no revela la posición actual ni los PV de un enemigo oculto. Si el mapa contiene un ID sin una unidad viva correspondiente en el controlador, se conserva el identificador como fallback de diagnóstico.

## Actualización de estado

`CellHud_setUnitController()` conecta el HUD con el controlador de unidades de la partida. La fila vuelve a consultarse al refrescar la celda. Tras un ataque o una reparación, `main.c` llama a `CellHud_refresh()` para evitar que el HUD mantenga los PV anteriores.

La caché de filas evita escrituras redundantes en el VDP cuando la información no cambia.

## Validación

`tests/cell_hud_test.c` verifica el formato de una unidad ocupante y que la información queda oculta en celdas exploradas pero fuera de visión y en celdas desconocidas. También verifica que la baja/liberación de una casilla actualiza el HUD a `OCUPACION: LIBRE`.

Las pruebas host no sustituyen la comprobación visual con una ROM real de SGDK en BlastEm.
