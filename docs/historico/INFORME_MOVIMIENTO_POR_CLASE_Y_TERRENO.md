# Entrega: movimiento por clase y terreno

## Resumen

Se incorpora un perfil de coste de entrada de terreno para cada clase de unidad. El mismo dato es consumido por el pathfinder, el controlador de movimiento, el HUD y la exploración de área de movimiento de la niebla. Esto evita que una casilla aparezca alcanzable con un coste y después cobre otro al ejecutar la orden.

## Costes vigentes

| Terreno de destino | Infantería | Explorador | Ingeniero | Blindado |
|---|---:|---:|---:|---:|
| Agua | Intransitable | Intransitable | Intransitable | Intransitable |
| Bosque | 2 | 1 | 3 | 3 |
| Camino | 1 | 1 | 1 | 1 |
| Piedras | 2 | 1 | 2 | 4 |

Los valores son puntos de movimiento, no cantidad de celdas. Un valor `0` significa que la clase no puede entrar en ese terreno. Se cobra el terreno de destino al completar la entrada a la celda; la celda de origen no vuelve a cobrarse.

## Fuente de verdad y flujo

- `src/unit_types.c`: matriz `terrainMoveCosts` por clase y función `UnitType_getTerrainMoveCost()`.
- `src/pathfinder.c/.h`: las APIs `Pathfinder_findRouteForType()` y `Pathfinder_buildReachabilityForType()` calculan rutas y alcance con el perfil de la unidad. Las APIs históricas sin `ForType` se conservan por compatibilidad de pruebas/herramientas y suponen infantería; el código de juego debe utilizar las APIs tipadas.
- `src/unit_controller.c`: valida el movimiento y descuenta el coste de entrada usando el mismo perfil.
- `src/fog_of_war.c`: los snapshots guardan el tipo de unidad para explorar el área de movimiento con el coste correcto.
- `src/cell_hud.c`: muestra el coste del terreno para la clase seleccionada; sin una unidad seleccionada presenta el valor de referencia de infantería.

## Cómo ajustar el equilibrio

Editar únicamente la tabla de `src/unit_types.c` para cambiar los costes existentes. Al agregar una clase, especificar un valor para cada terreno. Al agregar un terreno, actualizar `TERRAIN_COUNT`, la información del terreno, cada perfil de clase y las pruebas de pathfinding, movimiento, HUD, niebla y escenarios.

## Verificación realizada

- `sh tests/run_tests.sh`: 23/23 grupos más la prueba adicional de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos más la prueba adicional superados con ASan/UBSan.
- GCC 14.2 con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: 21 módulos C compilados con stubs SGDK sin diagnósticos.
- Estilo de fuente: 62 archivos C/H revisados; sin espacios finales ni líneas por encima de 100 columnas.
- Documentación: 67 archivos Markdown; cero enlaces relativos rotos.
- Recursos: se inspeccionaron los 11 PNG de `res/gfx`; permanecen indexados y con las dimensiones esperadas.
- Las pruebas incluyen matriz de costes, gasto real al llegar, rango de niebla por clase, coste en HUD y rutas a objetivo para los tipos de unidad de ambos escenarios.

## Pendiente de validación externa

Este entorno no tiene la toolchain real de SGDK del usuario ni BlastEm. Por ello, no se afirma que esta entrega haya generado una ROM ni que su rendimiento visual haya sido medido. Después de compilar, conviene probar una ruta de cada clase que cruce bosque, camino y piedras, y comprobar el coste del HUD y el área revelada por la niebla.
