# Edición completa · rendimiento, IA, combate y misiones

Esta edición parte del escenario de estrategia Mega Drive de 40 × 40 casillas y 120 unidades (60 por equipo), manteniendo la niebla incremental y las optimizaciones de scroll/sprites de la versión anterior. Su objetivo es integrar las mejoras de motor y convertir el prototipo en una partida jugable contra el equipo Rojo.

## 1. Rendimiento y diagnóstico

### Búsqueda de rutas

`src/pathfinder.c` usa una cola circular estática de 1.600 índices de 16 bits y una búsqueda ponderada por relajación. Solo vuelve a encolar una casilla cuando encuentra un coste menor; `NODE_QUEUED` impide que un índice ocupe dos posiciones de cola simultáneamente. La búsqueda respeta coste, agua, ocupación, unidad de origen y presupuesto de movimiento.

En el escenario de producción (40 × 40) se evita volver a recorrer todo el rectángulo alcanzable por cada nivel de coste. `Pathfinder_buildReachability()` conserva un fallback de barrido para pruebas o mapas de herramientas mayores de 1.600 casillas; `Pathfinder_findRoute()` está limitado al tamaño de la cola del escenario actual. Si se amplía el mapa de juego, habrá que aumentar de forma deliberada esa capacidad y revisar RAM.

La cola añade 3.200 bytes de RAM estática. El subtotal de mapa lógico (6.400 bytes), los tres buffers de pathfinding (4.800 bytes) y la cola (3.200 bytes) es de 14.400 bytes (14,06 KiB), antes de unidades, controladores, pila y otros sistemas. Las optimizaciones RAM A/B/C aplazadas siguen sin implementarse.

### Vista y sprites

- Las máscaras de borde se escriben como tiles opacos estáticos en BG_B; ya no crean 17 sprites hardware.
- Para el escenario de 120 unidades lógicas, se limita el renderizado simultáneo a 77 sprites de unidad y los tres objetos fijos (cursor, marcador y reserva de niebla), el peor caso usa 63 entradas SAT de 80, con 17 entradas disponibles. El límite por línea de pantalla sigue requiriendo una prueba visual en BlastEm.
- Se eliminan las declaraciones de recursos SGDK para los dos sprites de máscara que ya no se usan.
- La representación sigue usando caché en reposo, durante el scroll y cuando se anima una única unidad. Los gráficos compartidos de unidad se cargan una vez y las instancias fuera del viewport no se mantienen activas.

### Niebla y transferencias

La niebla no se recalcula indiscriminadamente por fotograma: se actualiza al cambiar movimiento o equipo. Los cambios contiguos del estado visible/desconocido se agrupan en tramos horizontales, con transferencias largas por fila visual en lugar de escrituras por cada casilla. El cambio de equipo conserva un redibujado completo puntual.

### Contadores de diagnóstico

`src/performance.c/.h` añade contadores por fase para scroll y píxeles desplazados, actualizaciones de SAT, llamadas/celdas de niebla, tramos de transferencia, búsquedas/nodos del pathfinder, construcciones de alcance, recorridos de registros de unidad y decisiones de IA. En la edición de perfilado el muestreo queda activo por defecto y cada informe representa 120 fotogramas de una sola actividad.

Para volver al modo normal, cambiar `PERF_DIAGNOSTICS` a `0` en `src/game_config.h`. Los contadores comparan carga de trabajo; **no miden directamente los ciclos del 68000 ni los FPS**. La medición real requiere ejecutar la ROM en SGDK/BlastEm y anotar el `FPS: NN` visible durante reposo, scroll continuo, movimiento y turno IA. Véase `INFORME_DIAGNOSTICO_RENDIMIENTO_120_UNIDADES.md`.

## 2. IA del equipo Rojo

`src/enemy_ai.c/.h` ejecuta una sola decisión por llamada. La animación de movimiento continúa en el bucle de juego, de modo que la IA no intenta animar una unidad de forma síncrona ni bloquea el fotograma.

La IA táctica recibe el `MissionObjective` completo y asigna roles al inicio de cada turno. En captura/mantener define un corredor y una escolta; cuando Rojo controla un punto de mantener o debe sobrevivir, selecciona hasta dos guardias resistentes y un anillo de pantalla. Los exploradores flanquean, blindados e ingenieros apoyan desde su rango y las unidades con vida crítica buscan retirarse de amenazas letales si no pueden conseguir una baja inmediata segura.

Para atacar, la IA estima daño, bajas inmediatas, valor táctico del blanco y represalia. Para moverse, pondera progreso de misión, rol, amenaza de los enemigos visibles, distancia de arma y defensa del terreno. Las amenazas visibles para Rojo se enumeran una vez por decisión en una tabla de índices de 60 bytes, y los destinos se limitan al alcance de movimiento. La nueva coordinación mantiene un blanco común para concentrar fuego en objetivos que sobreviven al primer ataque, sin permitir que esa preferencia anule la penalización de un contraataque letal. Además, detecta cambios del equipo que controla el objetivo y reasigna corredor/escolta/guardias durante el mismo turno en vez de esperar al siguiente.

La coordinación es deliberadamente ligera: roles por turno y evaluación local de casillas; no hace búsqueda minimax ni simula todas las respuestas de varios turnos del jugador. `AI_TEAM` está definido como `TEAM_RED` en `src/enemy_ai.c`. Para detalles y pruebas, consulta [`INFORME_IA_TACTICA.md`](INFORME_IA_TACTICA.md).

## 3. Combate y terreno

`Combat_predict()` y `Combat_predictUnits()` validan un ataque y calculan daño/represalia sin modificar el estado de las unidades. `Combat_attack()` aplica esa misma predicción; por tanto, el HUD y el resultado real comparten la fórmula. Cuando el cursor está sobre un enemigo visible y seleccionado hay un atacante válido, el HUD presenta el daño estimado y la represalia esperada.

`TerrainInfo` contiene `defenseBonus`:

| Terreno | Coste de entrada | Defensa adicional |
|---|---:|---:|
| Agua | Intransitable | 0 |
| Bosque | 2 | +1 |
| Camino | 1 | 0 |
| Piedras | 2 | +2 |

Daño bruto: `max(1, ataque − defensa de la unidad − defensa del terreno)`, limitado a la vida actual del objetivo. La represalia solo ocurre si el defensor sobrevive, puede alcanzar al atacante con su alcance Manhattan y tiene línea de visión despejada. Las casillas intermedias de bosque y piedras bloquean el disparo.

## 4. Tipos de misión

Cada `ScenarioDefinition` define su `MissionObjective` en `src/scenario.c`. La edición incluye Punto de control y Defensa del puente; se eligen desde el selector de arranque con la cruceta y A/START. `ACTIVE_MAP_INDEX` solo fija la selección predeterminada; consulta `docs/escenarios.md`.

- `MISSION_CAPTURE_POINT`: el equipo que entra en la casilla del objetivo gana inmediatamente.
- `MISSION_HOLD_POINT`: el equipo que ocupa el punto acumula finales de turno consecutivos; perderlo o cambiar de ocupante reinicia la racha. `requiredTurns` define la cantidad necesaria.
- `MISSION_ELIMINATE_ALL`: gana el equipo que deja al rival sin unidades vivas.
- `MISSION_SURVIVE_TURNS`: `targetTeam` gana al completar `requiredTurns` de sus propios turnos, siempre que siga vivo. Si ese equipo es eliminado antes, gana el rival.

El HUD muestra el progreso de “mantener” y “resistir”. El marcador de punto solo se dibuja para captura/mantener. En modos no asociados a un punto, las coordenadas del `MissionObjective` deben seguir apuntando a una celda existente para que la configuración sea válida, aunque el marcador no se muestre.

## 5. Pruebas realizadas

Ejecutado desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
```

Resultado de la validación ampliada posterior: **23 grupos de comprobación superados**, tanto en la suite normal como bajo ASan/UBSan. Se añadieron una comparación de propiedades en 20.000 mapas aleatorios y una simulación del turno completo de la IA con las 120 unidades. Incluye pruebas de rutas ponderadas, escenario 40 × 40, niebla, sprites y HUD, las cuatro condiciones de misión, predicción/cobertura del combate con banderas de bajas previstas, IA consciente del objetivo, selección de ataques, defensa de posiciones y uso de cobertura, y compilación de sintaxis con `PERF_DIAGNOSTICS=1`. El lanzador `tests/run_tests.bat` se ha actualizado también para Windows.

Las pruebas host usan GCC y stubs de SGDK. No se ha ejecutado una compilación real de la ROM porque esta sesión no dispone de `GDK` configurado; tampoco se ha medido FPS ni validado la apariencia en BlastEm. En la primera prueba real conviene comprobar especialmente: las máscaras oscuras de borde sobre BG_B, el HUD y la fuente, la vuelta automática de Rojo a Azul, el orden de planos, las filas con muchos sprites y las transferencias de niebla durante el scroll.

## 6. Archivos principales

- `src/pathfinder.c/.h`: búsqueda de ruta y alcance con cola.
- `src/performance.c/.h`: contadores diagnósticos opcionales.
- `src/enemy_ai.c/.h`: decisiones del equipo Rojo.
- `src/combat.c/.h`: predicción/aplicación de combate y defensa del terreno.
- `src/game_turn.c/.h`: progreso de misiones.
- `src/cell_hud.c/.h` y `src/main.c`: HUD de misión/combate, IA por turnos y máscaras en BG_B.
- `tests/enemy_ai_test.c`, `tests/combat_test.c`, `tests/game_turn_test.c`, `tests/cell_hud_test.c`: cobertura adicional.


## Ajuste de niebla durante el turno de IA (actualización posterior)

Durante la vista espectadora del turno rojo, el renderizado ahora usa la unión de visión actual de ambos equipos. Una casilla se muestra si Azul o Rojo la ve; si no, se conserva el estado explorado si al menos uno lo conoce; solo permanece completamente bajo niebla cuando ambos la desconocen. Se ha mantenido independiente el estado lógico por equipo. Ver `INFORME_TURNO_IA_VISIBLE.md` para las pruebas.


## Ciclo completo de partida

La pantalla final muestra el resultado de la misión, el nombre del objetivo y el número del turno. A repite la misma misión; B o START vuelve al selector. Antes de la siguiente partida, se libera el mapa de SGDK y se termina el motor de sprites, para evitar fugas o referencias a sprites antiguos. Los buffers grandes se reutilizan. Se ha añadido `src/mission_result.c/.h` y una prueba específica del mapeo de botones.


## Actualización: maniobras tácticas de grupo

La IA ahora expone un plan global derivado de la misión y del propietario del objetivo, y lo vuelve a evaluar cuando cambia el control. Durante el asalto, corredor y escolta avanzan juntos y el apoyo busca cubrir la línea de aproximación; durante la defensa, guardias y pantallas prefieren sectores diferentes; en combate, las unidades de apoyo buscan un puesto de tiro lateral a distancia útil. Las decisiones siguen restringidas a enemigos visibles y destinos alcanzables. El detalle y las pruebas están en `INFORME_MANIOBRAS_GRUPO.md`.

Validación host de esta edición: 23/23 grupos normales, 23/23 con ASan/UBSan y 19 módulos C sin diagnósticos de GCC `-fanalyzer`. Como en las versiones anteriores, queda pendiente compilar la ROM con la instalación real de SGDK y probar las maniobras visualmente en BlastEm.


## Actualización: línea de visión y oclusión de terreno

`MapData_hasLineOfSight()` usa recorrido supercover entero y simétrico. Bosque y piedras bloquean la visión y los ataques cuando son casillas intermedias; la casilla objetivo no se considera un obstáculo. La niebla aplica LOS al revelar y recalcular estados, el combate devuelve `COMBAT_RESULT_BLOCKED_LINE_OF_SIGHT`, y la IA evita estimar amenazas a través de obstáculos. Tests añadidos en `map_data_test.c`, `fog_of_war_test.c` y `combat_test.c`. Ver `INFORME_LINEA_VISION_TERRENO.md`.
