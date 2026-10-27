# Informe: ciclo completo de partida

## Qué cambia

La partida ya no termina dejando al jugador inmóvil sobre el mapa. Cuando `GameTurn_isFinished()` pasa a verdadero, `main.c` presenta un panel final en BG_B sobre el campo de batalla. Muestra el equipo vencedor, el nombre de la misión y el número de turno en que se resolvió.

Controles del panel:

- **A**: repetir la misión actual.
- **B** o **START**: volver al selector de escenarios.

La pantalla espera a que se suelten los botones antes de aceptar una orden, para que la acción que causó la victoria no se interprete como una orden del menú siguiente. El menú mantiene sus controles existentes: cruceta arriba/abajo para elegir y A/START para comenzar.

## Limpieza y reinicialización

Al abandonar el panel final:

1. `MAP_release(worldMap)` libera el mapa gráfico reservado por SGDK.
2. `SPR_end()` libera el motor de sprites y las referencias registradas en él.
3. Se limpian BG_A y BG_B y se sincroniza el VBlank.
4. Antes de la partida siguiente se invalidan las cachés del HUD y del alcance, y se reinicializan mapa lógico, controlador de unidades, turnos, niebla, IA, vista de unidades y cachés gráficas.
5. Los buffers grandes de celdas/pathfinding y almacenamiento de unidades se reutilizan; no se crean copias adicionales para la partida repetida.

La selección de **B/START** regresa al selector; **A** conserva la misión elegida y vuelve a inicializarla. Este ciclo evita un reinicio de la ROM para volver a jugar.

## Pruebas ejecutadas

- `tests/run_tests.sh`: 23/23 grupos superados.
- `tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-fanalyzer`, `-Wall -Wextra -Werror -Wformat=2`: sin diagnósticos en los 19 módulos C.
- `tests/mission_result_test.c`: verifica esperar, repetir, volver al selector y prioridad de menú si se reciben dos órdenes al mismo tiempo.
- Sintaxis integrada de todos los módulos y ambos valores de `ACTIVE_MAP_INDEX` comprobada con stubs de SGDK.

## Pendiente

Las pruebas host no ejecutan el motor de VDP ni sustituyen la compilación con SGDK. Hay que compilar la ROM y probar en BlastEm o en la consola el panel final, volver al selector, repetir una misión, y cambiar de una misión a otra varias veces. Debe confirmarse que no quedan sprites fantasma, mapas antiguos, colores incorrectos ni estados de visión heredados.
