# Escenarios configurables

El motor elige un `ScenarioDefinition` del registro en `src/scenario.c`. Cada entrada agrupa el nombre, recurso gráfico (`MAP` SGDK), mapa lógico (`MapData`), despliegue (`UnitStart[]`), almacenamiento de unidades y objetivo (`MissionObjective`). La inicialización del motor consume esa estructura; no hay una rama particular de cada misión dentro de `main.c`.

## Escenarios incluidos

### 0 — Punto de control

- Mapa lógico de 40×40.
- 60 unidades Azul y 60 unidades Rojo (120 en total).
- Mezcla por bando: 8 infanterías, 8 exploradores, 7 ingenieros y 7 blindados.
- Objetivo de captura inmediata en `(20,20)`. `requiredTurns` es `0` porque este tipo de misión no utiliza contador de turnos.
- Recurso gráfico `world_map`.

### 1 — Defensa del puente

- Mapa lógico y gráfico distintos, de 40×40.
- Río norte-sur que bloquea el paso, con puentes en tres ejes horizontales.
- Bosques como terreno predominante, rutas de suministro y posiciones de roca defensivas.
- 12 unidades por bando: 4 infanterías, 3 exploradores, 2 ingenieros y 3 blindados.
- Objetivo neutral en `(20,20)`: gana el primer equipo que lo mantenga durante 3 finales de turno.
- Recurso gráfico `bridge_map`, creado a partir del atlas existente `terrain_tiles.png` para que sus patrones sigan siendo válidos para SGDK.

### 2 — Puente del desfiladero

- Mapa lógico y gráfico propio de **20×20 celdas**.
- Un río atraviesa el mapa de norte a sur; el puente central conecta ambas orillas y obliga a disputar un paso estrecho.
- Bosques en las orillas y pequeñas posiciones rocosas que dan cobertura sin cerrar las rutas al objetivo.
- 6 unidades por bando: 2 infanterías, 1 explorador, 1 ingeniero y 2 blindados.
- Objetivo de control en `(10,9)`: mantener el puente durante 3 finales de turno.
- Recurso gráfico `bridge_skirmish_map`, derivado del atlas existente y alineado con las 20×20 celdas lógicas.

## Seleccionar escenario desde el juego

Al arrancar se muestra una pantalla de selección antes de reservar las estructuras de mapa y unidades.

- **Cruceta arriba/abajo**: cambia la misión resaltada entre las tres opciones.
- **A o START**: comienza la misión elegida.

El selector muestra nombre, objetivo resumido y número de unidades. `ACTIVE_MAP_INDEX` en `src/game_config.h` solo establece la selección inicial predeterminada (0 = Punto de control; 1 = Defensa del puente; 2 = Puente del desfiladero). La elección del jugador se usa para inicializar el único `MapData` activo, así que no se duplican los grandes buffers de RAM.

## Añadir un escenario

1. Definir sus filas lógicas (`W`, `F`, `P`, `R`) en `src/map_data.c` y exponer un `MapData` descriptor que reutilice los buffers comunes. El motor admite dimensiones de escenario variables; los incluidos son 40×40 y 20×20.
2. Crear una imagen de mapa compatible con `terrain_tiles.png` y declararla con `MAP nombre "gfx/nombre.png" terrain_tiles BEST 0` en `res/resources.res`. El recurso debe seguir el tamaño de 3×3 tiles de 8×8 por celda lógica y el margen técnico existente.
3. Declarar un `UnitStart[]` con IDs consecutivos desde cero, unidades dentro del mapa, casillas transitables y sin solapamientos.
4. Añadir una entrada a `scenarioDefinitions` en `src/scenario.c`: nombre, recurso gráfico, mapa lógico, despliegue y objetivo.
5. Añadir/ajustar pruebas en `tests/scenario_test.c` para validar despliegue, terreno y misión.

## RAM y limitación intencionada

Los tres descriptores (`worldMapData`, `bridgeMapData` y `bridgeSkirmishMapData`) comparten el mismo buffer de 1.600 celdas y los mismos buffers de pathfinding. Evita inicializar dos instancias en paralelo: seleccionar un escenario llama a `MapData_init` y reemplaza el contenido del almacenamiento compartido. Se mantiene así el presupuesto de RAM de la Mega Drive.

## Contrato de objetivos

- `MISSION_CAPTURE_POINT`: el primer equipo que ocupa el objetivo gana inmediatamente. No usa `requiredTurns`.
- `MISSION_HOLD_POINT`: el equipo que ocupa el objetivo debe conservarlo durante el número configurado de finales de sus propios turnos. Los turnos del rival no incrementan el progreso; un objetivo vacío o controlado por el rival rompe la racha.
- Ambos objetivos son disputables por Azul o Rojo. `targetTeam` no restringe la victoria en captura/mantenimiento.
- Los tres despliegues iniciales se validan por casillas únicas, terreno transitable y ruta a su objetivo. Las pruebas de integración comprueban la captura inmediata para ambos bandos y las tres rondas de mantenimiento para cada equipo en los dos mapas de puente.


## Después de una misión

Cuando una condición de victoria o derrota se cumple, aparece la pantalla de resultado. **A** repite la misión elegida y **B/START** devuelve al selector de escenarios. No hace falta reiniciar la ROM ni recompilar.
