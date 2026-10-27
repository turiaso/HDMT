# HUD y feedback de acciones

Este documento describe la presentación actual del HUD y dónde ampliar sus mensajes sin mezclar presentación con reglas de juego.

## Distribución

- La fila superior muestra el diagnóstico de FPS.
- Las cuatro filas de información describen la celda bajo el cursor: coordenadas, terreno, coste/transitabilidad y ocupación/visión.
- La fila de estado de unidad se reutiliza para mostrar atributos, ruta, predicción de ataque, reparación o resultado de una acción.
- La fila de turno muestra solo el equipo activo y sus puntos de movimiento cuando hay una unidad seleccionada.
- Una fila independiente muestra el objetivo y su progreso, para que la información de misión no compita con los puntos de movimiento.
- La fila de ocupación muestra tipo, equipo y PV de una unidad solo cuando la celda está visible para el equipo consultante.

El HUD sigue en `BG_B` y usa `HUD_TEXT_WIDTH` como límite horizontal. No se añaden planos ni tiles permanentes; la caché evita volver a escribir líneas idénticas. `HUD_TEXT_LINES` incluye ahora las tres filas inferiores (estado de unidad, turno y objetivo).

## Unidad seleccionada y movimiento

Cuando el cursor está sobre la unidad seleccionada, la fila de estado conserva una ficha compacta con el prefijo `SEL` y los atributos de combate. Al mover el cursor a otra casilla, la misma fila pasa a mostrar la ruta prevista; así se evitan los mensajes redundantes de una ruta de cero pasos sin perder acceso a los datos de la unidad.

Al inspeccionar una casilla ocupada y visible, la fila de ocupación usa el controlador lógico para presentar, por ejemplo, `OCUPACION: BLD ROJO HP9/14`. Una casilla explorada pero no visible muestra `OCUPACION: SIN VISION`; una desconocida muestra `OCUPACION: DESCONOCIDA`. Esta comprobación precede a la consulta de la unidad, para no filtrar la posición ni los PV actuales de un enemigo oculto. Consulta [Inspección de ocupación en el HUD](hud_inspeccion_ocupacion.md) para el contrato y sus pruebas.

La vista de ruta usa `RUTA 6 PASOS / COSTE 6 / MP 6` o `SIN RUTA VALIDA`. Si la acción compartida ya se gastó, el mensaje distingue entre `ATAQUE YA REALIZADO ESTE TURNO` y `REPARACION YA REALIZADA ESTE TURNO`; cuando solo se agotó el presupuesto, indica `MOVIMIENTO AGOTADO`. El contador `MP` de la fila de turno sigue siendo la fuente compacta del presupuesto disponible.

## Estado de turno y objetivo

`CellHud_setTurnInfo()` actualiza dos filas separadas. La primera contiene el turno y los MP:

- `T2 AZUL | MP 4/6`
- `T4 ROJO`

La segunda explica el objetivo sin mezclarlo con los puntos de movimiento:

- Captura: `OBJ: CAPTURAR X21,Y21`
- Mantener: `OBJ: PUENTE - AZUL 1/3` o `OBJ: PUENTE - SIN CONTROL 0/3`
- Sobrevivir: `OBJ: RESISTIR 2/4`
- Eliminación: `OBJ: ELIMINAR TODO`

El contador de mantenimiento representa los finales de turno válidos del equipo que controla el puente. Cuando la partida termina, la fila de turno muestra el resultado y la fila del objetivo se limpia para no dejar progreso obsoleto en pantalla.


## Predicción y resultado del combate

`CellHud_setCombatPreview()` recibe el daño previsto, la represalia y los PV actuales/máximos del objetivo. Calcula los PV que quedarían después del ataque sin modificar la unidad y muestra, por ejemplo, `ATQ -3 / OBJ HP 3/8 / REP -2`. Si el golpe sería letal, muestra `OBJ HP 0/8`.

Después de un ataque real del jugador, `showCombatResult()` informa del daño, los PV restantes del enemigo y la represalia. Los mensajes de eliminación siguen teniendo prioridad. La presentación de ataques de la IA no inventa PV restantes si el resultado no expone la identidad del defensor.

## Reparación de ingenieros

`Combat_predictRepairUnits()` no modifica el aliado, pero devuelve `healthRestored` con la recuperación prevista, limitada por los PV que falten. El HUD presenta `REPARAR +3 HP -> 8/10` al apuntar a un aliado reparable y `REPARADO +3 HP -> 8/10` después de confirmar. Los errores se expresan con mensajes breves y distintos para alcance, objetivo sin daños y acción ya consumida.

## Mensajes largos

Los constructores `HudLine` detectan el truncamiento. Si una línea supera el ancho, `finishHudLine()` busca un límite de palabra y termina con `...`. Esto evita mostrar mensajes cortados a mitad de una palabra. Los mensajes directos a `CellHud_setUnitStatus()` usan la misma regla cuando exceden el ancho.

## Pruebas

`tests/cell_hud_test.c` comprueba las filas independientes de turno y objetivo, los cuatro tipos de misión con y sin unidad seleccionada, la ficha `SEL`, el tipo/equipo/PV de ocupantes visibles, la ocultación por niebla, los textos de ruta, la recuperación prevista/real, la vida restante prevista, la represalia y la truncación de mensajes largos. `tests/combat_test.c` verifica que predecir una reparación devuelve los PV esperados sin cambiar la salud ni consumir la acción. Ejecuta:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
sh tools/check_source_quality.sh
```

Estas pruebas verifican el texto generado y las escrituras del HUD en los stubs host; no sustituyen a una inspección visual con la ROM compilada en SGDK y ejecutada en BlastEm.
