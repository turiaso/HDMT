# Informe — escenarios configurables

## Resultado

El motor consume una definición de escenario en lugar de tener el mapa gráfico, el despliegue y el objetivo fijados dentro de `main.c`. Se incluyen dos escenarios y ahora se pueden elegir desde el menú de arranque con la cruceta y A/START:

- **Punto de control** (`ACTIVE_MAP_INDEX = 0`): mapa original 40×40, 60 unidades por equipo, captura en `(20,20)`.
- **Defensa del puente** (`ACTIVE_MAP_INDEX = 1`): mapa 40×40 nuevo con río norte-sur, tres pasos de puente, rutas de suministro, charcas y posiciones defensivas de roca. Usa 12 unidades por equipo (4 infanterías, 3 exploradores, 2 ingenieros y 3 blindados por lado). El objetivo neutral en `(20,20)` debe mantenerse durante tres finales de turno.

## Archivos clave

- `src/scenario.h/.c`: `ScenarioDefinition`, registro de escenarios, objetivos y despliegues.
- `src/map_data.h/.c`: terreno lógico de cada mapa; el segundo es `bridgeMapData`.
- `res/gfx/bridge_map.png`: nuevo mapa gráfico.
- `res/resources.res`: declaración del segundo recurso `MAP`.
- `src/game_config.h`: `ACTIVE_MAP_INDEX` define la misión resaltada inicialmente, no bloquea la selección en ejecución.
- `docs/escenarios.md`: guía de selección desde el juego y de cómo añadir escenarios.
- `docs/previews/bridge_map.png`: vista previa.

## RAM

Los dos descriptores `MapData` comparten deliberadamente los buffers estáticos de celdas y de pathfinding para no duplicar alrededor de 14 KiB. El juego inicializa solamente el escenario seleccionado. No deben usarse ambos `MapData` como partidas simultáneas.

## Verificación automática

- `tests/run_tests.sh`: 23/23 grupos superados.
- `tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-fanalyzer` con advertencias estrictas: sin diagnósticos en la integración del escenario 0 y del escenario 1.
- Las pruebas verifican nombres, recursos, mapas lógicos diferentes, objetivo, composición, IDs, terreno transitable, ausencia de solapamientos y rutas desde las 24 posiciones de inicio del escenario del puente hasta el objetivo.
- Se comprobó cada tile de 8×8 de `bridge_map.png` frente a `terrain_tiles.png`: no hay patrones gráficos fuera del atlas del que depende SGDK.

## Limitación

No se ha generado una ROM de esta edición en este entorno porque no está configurado el toolchain SGDK. La compilación y la comprobación en BlastEm siguen siendo el siguiente paso externo. El menú permite elegir cualquiera de los dos escenarios en la propia ROM; `ACTIVE_MAP_INDEX` solo decide cuál aparece resaltado inicialmente.


Actualización del ciclo de partida: tras una victoria o derrota, pulsa A para repetir el escenario actual o B/START para regresar al selector. La pantalla de resultados y su política de botones se prueban en `mission_result_test.c`.
