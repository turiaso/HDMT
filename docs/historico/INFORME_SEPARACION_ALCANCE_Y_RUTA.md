# Separación estricta entre alcance y ruta

## Qué quedó fijado

La revisión del código ha reforzado una distinción que debe mantenerse explícita:

- **Al seleccionar una unidad:** se calcula una sola vez el conjunto de casillas alcanzables con sus puntos actuales. Ese cálculo de alcance es imprescindible para iluminar las casillas correctas con PAL1 y restringir el cursor.
- **Al mover el cursor por el área:** `UnitController_updatePreview()` solo consulta la caché de alcance mediante `Pathfinder_getReachableInfo()`. No invoca `Pathfinder_findRoute()`, no recalcula el alcance y no modifica los búferes de `PathfinderWorkspace`.
- **Primera A sobre el destino:** consulta la máscara ya calculada y prepara el destino; no construye una ruta.
- **Segunda A en el mismo destino:** `UnitController_activate()` ejecuta `Pathfinder_findRoute()` una vez sobre el estado actual del mapa y empieza la animación si la ruta sigue siendo válida.

## Corrección de la implementación

Se eliminó `ensureMovementRange()` de las consultas del cursor. Si la caché no es válida, la consulta devuelve “no disponible” en vez de iniciar una búsqueda perezosa desde el hover. El alcance se construye de forma explícita al seleccionar la unidad; el controlador ahora exige el búfer `reachable` para evitar rutas de ejecución que no puedan respetar esta separación.

La revisión también precisa un diagnóstico anterior: no era exacto afirmar que el camino normal de `UnitController_updatePreview()` llamaba directamente a `Pathfinder_findRoute()` en cada celda. El problema de diseño era permitir una recalculación implícita cuando el alcance se consideraba inválido y mantener un fallback de búsqueda para workspaces sin máscara. Ambos caminos se han eliminado del flujo normal de preview.

## Prueba de regresión añadida

`testCursorHoverNeverRunsPathfinder()`:

1. Selecciona una unidad y guarda los búferes de coste, predecesores y alcance.
2. Invalida artificialmente la caché y mueve el cursor; comprueba que no se restaura por sí sola ni se modifica ningún búfer.
3. Restablece el alcance explícitamente, recorre varias celdas y verifica que todos los búferes siguen intactos.
4. Confirma que la primera A prepara el destino sin alterar los búferes.
5. Confirma que la segunda A inicia el cálculo de la ruta concreta.

## Verificación

- `sh tests/run_tests.sh`: todas las secciones pasan, incluido el nuevo caso de regresión.
- GCC `-fanalyzer`: sin diagnósticos en los módulos afectados.
- No se ha usado Clang.
- La suite de host no sustituye una compilación SGDK ni la prueba visual de la ROM en BlastEm.
