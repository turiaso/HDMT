# IA táctica por terreno y clase

## Principios

`EnemyAI_update()` resuelve una sola acción por llamada. La cámara, la animación y la presentación siguen en el bucle de juego. Las preferencias descritas aquí solo se calculan cuando la IA decide qué hacer; nunca en cada fotograma de movimiento o scroll.

La IA solo puntúa casillas incluidas en la máscara de alcanzabilidad producida con el perfil de movimiento real de la unidad (`Pathfinder_*ForType`). Una preferencia táctica no vuelve transitable una casilla ni permite exceder los puntos disponibles.

La selección de objetivos se limita a enemigos vivos que el equipo Rojo ve en ese momento. Un enemigo explorado en el pasado pero actualmente oculto no puede ser elegido como blanco ni aportar amenaza táctica. La regresión `testAIDoesNotTargetHiddenEnemy` comprueba el caso de un enemigo adyacente oculto por la niebla y simula además un foco heredado: cuando el blanco deja de estar visible, el índice de foco debe invalidarse antes de seleccionar una acción.

## Contrato de turno y estado de la IA

`EnemyAI_update()` valida antes de planificar que el controlador use el mismo objeto `MapData` y que `activeTeam` coincida con el equipo activo de `UnitController`. Una llamada hecha en el turno equivocado devuelve `ENEMY_AI_IDLE` sin avanzar el índice del actor ni cambiar el estado táctico. Esto evita que una llamada de integración incorrecta consuma silenciosamente unidades del turno.

El equipo dirigido se pasa explícitamente a los cálculos tácticos, de visibilidad, de combate y reparación. No se usa una variable global mutable para inferir el equipo actual; así la consulta de cámara `EnemyAI_getNextActorIndex()` tampoco altera el contexto de otra instancia de IA.

La IA resuelve una única decisión por llamada. Las rutas, el movimiento interpolado, las validaciones finales de combate y las reparaciones siguen siendo responsabilidad de `UnitController` y `Combat`; la puntuación táctica no puede saltarse esas reglas.

## Seleccion del corredor de objetivo

Para misiones de captura/mantenimiento, el corredor se elige mediante una estimacion relativa de distancia dividida por el presupuesto maximo de movimiento de la unidad. Esto permite que una unidad mas movil (por ejemplo, un explorador con 8 puntos) sea preferida sobre otra algo mas cercana pero lenta. La salud perdida aporta una penalizacion secundaria y una unidad que ya gasto su orden de movimiento queda muy por detras de una que aun puede actuar. Es una heuristica de asignacion, no una ruta: el pathfinder sigue validando la ruta y los costes reales por terreno. La regresion `testObjectiveRunnerAccountsForUnitMobility` comprueba una situacion en la que el explorador esta una casilla mas lejos, pero es el corredor correcto por movilidad.

## Posicionamiento por clase

La función privada `classTerrainPositionPenalty()` aporta un ajuste a la puntuación del destino (menor puntuación es mejor):

- **Infantería:** recibe una preferencia adicional por terreno con bonificación defensiva para mantener posiciones.
- **Explorador:** obtiene un pequeño incentivo por cobertura de bosque sin sustituir su papel de flanqueo ni la ventaja de coste de movimiento.
- **Ingeniero:** valora más la cobertura porque su función principal es apoyar y reparar.
- **Blindado:** prefiere caminos y recibe penalizaciones tácticas en bosque y, sobre todo, piedras. Los costes de movimiento propios de esa clase siguen viniendo de `src/unit_types.c`.

Los ajustes se combinan con el progreso de la misión, el rol (corredor, escolta, guardia, pantalla, flanqueador o apoyo), el riesgo calculado y la cobertura. No deben reemplazar los costes de terreno ni ser consultados desde `main.c`.

Si una unidad tiene el rol de corredor de una misión de captura/mantener cuyo objetivo aún no es rojo, se omiten las preferencias opcionales de terreno y de línea de fuego para esa selección: el corredor no debe abandonar el objetivo por buscar una posición más cómoda.

## Alcance y línea de tiro

`strategicPositionScore()` utiliza el `attackRange` de la unidad que está considerando moverse, no el alcance del objetivo. Esto es importante cuando una infantería intenta alcanzar a un blindado o viceversa.

`firingLanePositionPenalty()` favorece posiciones que terminan dentro del alcance propio con línea de visión despejada y penaliza posiciones al alcance geométrico pero bloqueadas por bosque/roca. La validación final del ataque no se duplica: `Combat_predictUnits()` y `MapData_hasLineOfSight()` siguen siendo autoritativos.

## Apoyo del ingeniero

Primero se conservan las prioridades actuales: ataque válido y reparación inmediata de un aliado adyacente. Si no hay un enemigo visible seleccionado y el ingeniero conserva su acción de reparación, la IA identifica una única vez al aliado más necesitado (mayor porcentaje de vida perdida, luego mayor pérdida absoluta y, en empate, menor distancia).

`selectBestDestination()` prioriza entonces llegar a una casilla dentro de `repairRange` de ese aliado. Al terminar el movimiento, la reparación real debe volver a pasar por `Combat_predictRepairUnits()`/`Combat_repair()`; la puntuación de movimiento nunca concede PV ni consume una acción por sí misma.

## Rendimiento y contratos

- La selección del aliado a reparar es una pasada lineal por unidades por decisión, no por casilla candidata.
- La máscara `reachable` filtra destinos antes de ejecutar puntuación táctica o evaluación de riesgo.
- No se hacen cálculos de posición por fotograma.
- No se consulta información de enemigos que el equipo rojo no haya descubierto/vea como objetivo válido.
- No se modifica el sistema de eventos compartidos por celda ni los buffers de RAM aplazados.

## Pruebas de regresión

`tests/enemy_ai_test.c` incluye casos que verifican que el contexto de turno/mapa incoherente se rechaza sin consumir estado, que la infantería se aproxima según su alcance propio, que el ingeniero se mueve junto a un aliado herido y lo repara en la decisión posterior, que un blindado prioriza camino frente a roca cuando el progreso estratégico es equivalente, que el corredor mantiene prioridad del objetivo y que la IA abre una línea de fuego bloqueada.

Ejecuta `sh tests/run_tests.sh` y `sh tests/run_tests_sanitized.sh`; después compila la ROM con el SGDK real y valida la conducta/fluidez en BlastEm. Las pruebas host no garantizan los FPS de la ROM.

## Coordinación de posiciones, fuego y corredores

`friendlyTrafficPositionPenalty()` aplica preferencias suaves a posiciones que amontonan unidades o se colocan justo delante de una compañera que avanza hacia el mismo blanco/objetivo. La lista de aliadas cercanas se calcula una sola vez por decisión (`collectNearbyFriendlyIndices()`); dado que cada destino alcanzable está a distancia Manhattan como máximo igual al presupuesto, las aliadas más allá de `movimiento + 2` no pueden afectar ni a la separación de dos casillas ni a la detección de un bloqueo inmediato. Así no se recorre el ejército completo por cada candidato.

La penalización no vuelve ilegales las posiciones: en un puente de una casilla o un pasillo sin alternativa la IA puede formar una cola y espera a que se libere el siguiente paso. Cuando varias posiciones tienen la misma puntuación táctica, el desempate favorece la que se acerca más al objetivo antes de preferir el coste de ruta menor. El corredor de captura está exento de penalizaciones de tráfico para no sacrificar su prioridad real.

`firingLanePositionPenalty()` diferencia mejor una casilla dentro de alcance geométrico pero tapada de una posición con tiro abierto (penalización por tiro bloqueado: 28; bonus de línea despejada: 24). La validación real sigue siendo `MapData_hasLineOfSight()`/`Combat_predictUnits()`. La cobertura continúa siendo una preferencia, no una obligación: la infantería valora defensa de terreno cuando el progreso y la posibilidad de disparar son comparables; los blindados conservan su preferencia por caminos y posiciones laterales de tiro.

Las regresiones en `tests/enemy_ai_test.c` incluyen:

- `testInfantryUsesCoverWhenProgressIsEquivalent`: la infantería elige una posición rocosa defensiva cuando ambas opciones permiten tiro.
- `testTwoTanksChooseSeparatedFiringPositions`: dos blindados mantienen posiciones de tiro distintas.
- `testAILeavesFriendlyApproachLaneOpen`: en un mapa abierto, una unidad rodea a una compañera en vez de ocupar el siguiente paso de su corredor.
- `testNarrowBridgeQueuesAndReleasesUnitsSafely`: tres unidades forman cola ante un puente de una celda, no pisan agua/ocupaciones y avanzan cuando se libera el paso.

Estas preferencias no alteran presupuesto de movimiento, ocupación ni coste de terreno: el conjunto alcanzable del pathfinder sigue siendo la autoridad. Compáralas en ROM antes de ajustar los pesos; las pruebas host confirman lógica, no ciclos del 68000 ni FPS reales.
