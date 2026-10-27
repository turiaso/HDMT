# Fase 3 · Primera unidad controlable (registro histórico)

## Alcance incluido

- En su versión original, esta fase desplegaba una unidad con ID `0` en `(6, 4)`. La extensión de fase 4 añade explorador, ingeniero y blindado; consulta `docs/unidades.md` para el despliegue vigente.
- Muestra un sprite de 24×24 px, cuyo dibujo ocupa el centro de la casilla lógica.
- A selecciona la unidad; el estado de selección se indica en el HUD. No se deja un segundo cursor gráfico sobre la unidad. A sobre la misma unidad la deselecciona.
- Con la unidad seleccionada, A en una celda ortogonal adyacente mueve la unidad si esa celda es transitable y está libre. La selección se conserva tras un movimiento válido.
- Los destinos fuera del mapa, ocupados, sobre agua o no adyacentes se rechazan.
- La ocupación de `CellData` se actualiza junto con la coordenada de la unidad. Cada movimiento válido despacha `unitLeave` en origen y `unitEnter` en destino.
- El texto de estado se muestra en la quinta línea del HUD.

## Controles

1. Sitúa el cursor sobre la unidad y pulsa **A**.
2. Mueve el cursor a una casilla adyacente horizontal o verticalmente.
3. Pulsa **A** para mover la unidad.
4. Pulsa **A** sobre la unidad seleccionada para cancelar la selección.

La cruceta y C + cruceta conservan sus funciones de cursor y cámara.

## Límites intencionados

Esta fase no incorpora puntos de movimiento, coste de terreno, cálculo de rutas, animación de movimiento de unidad, combate, turnos ni múltiples tipos de unidad. El coste de terreno se usará en la fase 4. La estructura `UnitController` admite varios datos de unidad por escenario, aunque el escenario de ejemplo solo despliega una.

## Pruebas

Ejecuta `tests/run_tests.bat` en Windows o `sh tests/run_tests.sh` en una shell compatible. `unit_controller_test.c` verifica el despliegue, la selección, el movimiento adyacente, las restricciones de destino y los eventos de unidad. La prueba de anfitrión no reemplaza la compilación con SGDK ni la inspección visual de la ROM en BlastEm.
