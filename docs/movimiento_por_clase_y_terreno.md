# Movimiento por clase y tipo de terreno

## Regla vigente

Moverse a una casilla cuesta según **la clase de la unidad y el terreno de destino**. El movimiento se calcula con el mismo perfil desde la selección hasta la llegada: alcance resaltado, coste de ruta, validación de cada paso, puntos descontados al llegar y área de movimiento que descubre inicialmente la niebla.

| Terreno | Infantería | Explorador | Ingeniero | Blindado |
|---|---:|---:|---:|---:|
| Agua | No transitable | No transitable | No transitable | No transitable |
| Bosque | 2 | 1 | 3 | 3 |
| Camino | 1 | 1 | 1 | 1 |
| Piedras | 2 | 1 | 2 | 4 |

Los costes son enteros positivos; `0` significa que ese tipo no puede entrar. Los puntos de movimiento siguen siendo el presupuesto disponible por turno, no un número de celdas. El coste se descuenta al confirmar la entrada al centro de cada celda. El terreno de origen no se vuelve a cobrar.

## Fuente única de verdad

- `src/unit_types.c`: campo `terrainMoveCosts` dentro de `UnitTypeInfo` y función `UnitType_getTerrainMoveCost()`.
- `src/pathfinder.c/.h`: `Pathfinder_findRouteForType()` y `Pathfinder_buildReachabilityForType()` utilizan el tipo del actor; el movimiento de juego no debe usar las APIs de compatibilidad que asumen infantería.
- `src/unit_controller.c`: valida el paso siguiente y descuenta el coste de su perfil al confirmar la entrada.
- `src/fog_of_war.c`: cada snapshot conserva el tipo de la unidad, así que la exploración inicial y los recálculos de área móvil siguen los mismos costes.
- `src/cell_hud.c`: muestra el coste propio de la clase seleccionada; si no hay unidad seleccionada, muestra el valor de referencia del terreno.

`TerrainInfo.moveCost` se conserva como valor de referencia equivalente a infantería para la inspección sin selección y para utilidades genéricas del mapa. No se debe utilizar para calcular rutas o descontar puntos de una unidad concreta.

## Pruebas

`tests/pathfinder_test.c` compara costes y presupuestos de las clases en bosque y piedras, además de comprobar caminos y agua. `tests/unit_controller_test.c` confirma que el movimiento real resta el coste específico al llegar. `tests/fog_of_war_test.c` confirma que el explorador descubre bosque más lejano gracias a su perfil sin convertir ese terreno automáticamente en visión táctica. `tests/cell_hud_test.c` verifica el coste mostrado y que solo se repinte la fila que cambia. `tests/scenario_test.c` comprueba rutas al objetivo usando el perfil real de cada unidad desplegada.

Para ejecutar todo:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
```

En Windows, `tests\run_tests.bat` contiene los mismos enlaces a `unit_types.c`. La ROM aún requiere compilación con SGDK real y validación visual en BlastEm; las pruebas host no sustituyen esa comprobación.

## Añadir una nueva clase o terreno

Al añadir una clase, rellena todos los costes de `terrainMoveCosts`, incluido cada terreno existente. Usa `0` para prohibir la entrada. Añade una prueba que compruebe el alcance, la ruta, el gasto final de puntos y la exploración de niebla con esa clase. Si se añade un terreno nuevo, aumenta `TERRAIN_COUNT`, define su `TerrainInfo` y añade una columna equivalente en todos los perfiles, además de probar recursos y escenarios.
