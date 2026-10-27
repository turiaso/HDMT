# Informe: selector de misiones en pantalla

## Cambios

- Se añade una pantalla de selección al arrancar, antes de inicializar el escenario.
- La cruceta arriba/abajo recorre las misiones en forma circular. A o START confirma la selección.
- El menú presenta el nombre, una descripción del objetivo y el número de unidades por bando.
- La pantalla usa el texto de SGDK en BG_B y las paletas de equipo existentes; no añade imágenes, sprites ni buffers grandes de RAM.
- `ScenarioDefinition` contiene `menuDescription` y `menuSummary`, así el menú se amplía al registrar nuevas misiones sin una rama por escenario en `main.c`.
- `ACTIVE_MAP_INDEX` queda como selección predeterminada resaltada, no como una selección fija de ROM.
- Después de confirmar, espera a que se suelte A/START para no interpretar ese mismo botón como acción dentro de la partida.

## Escenarios

1. **Punto de control**: 120 unidades, 60 por equipo; capturar el objetivo central.
2. **Defensa del puente**: 24 unidades, 12 por equipo; mantener el puente durante tres turnos.

## Pruebas

- `tests/run_tests.sh`: 23 grupos completados.
- `tests/scenario_test.c` comprueba la navegación circular (arriba desde el primero, abajo desde el último), normalización de índice inválido, y que cada escenario tiene nombre, descripción y resumen para el menú.
- Compilación sintáctica de `main.c` con `-Wall -Wextra -Werror -Wformat=2`, para los valores predeterminados y `ACTIVE_MAP_INDEX=1`, ejecutada dentro de la suite.

## Pendiente de validación externa

Este entorno no tiene el toolchain SGDK. La ROM debe recompilarse con la instalación local y probarse en BlastEm para verificar visualmente las líneas del menú, los colores y el paso limpio a la partida.


### Pantalla de resultado y navegación

Después de una victoria o derrota, aparece una pantalla con el equipo vencedor, el nombre de la misión y el turno final. Pulsa A para repetir la misión actual; B o START devuelve al selector. El cambio limpia el campo de batalla, libera el `Map` y el motor de sprites, e invalida las cachés antes de crear la siguiente partida.
