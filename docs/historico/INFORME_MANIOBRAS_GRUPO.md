# Maniobras tácticas de grupo para la IA roja

## Qué cambia

### 1. Plan global de misión explícito

`EnemyAI` ahora registra un `EnemyAITacticalPlan` compacto:

- `AI_PLAN_OBJECTIVE_ASSAULT`: corredor y escolta avanzan al punto; las unidades de apoyo cubren el eje de aproximación.
- `AI_PLAN_OBJECTIVE_DEFENSE`: después de capturar un punto de mantener, se reasignan guardias y pantallas para defender sectores diferentes.
- `AI_PLAN_OBJECTIVE_SECURED`: registra que un objetivo de captura ya pertenece a Rojo.
- `AI_PLAN_ELIMINATION_SWEEP`: asalto y concentración de fuego contra enemigos visibles.
- `AI_PLAN_SURVIVE_DEFENSE` / `AI_PLAN_SURVIVE_COUNTERATTACK`: defiende el área o contraataca, según el equipo que deba sobrevivir.

El plan se vuelve a calcular cuando cambia el dueño del objetivo o cambia el modo táctico derivado de la misión. La reasignación ocurre en la siguiente decisión de IA, nunca dentro del movimiento por fotograma.

### 2. Grupo de asalto y escolta

El corredor conserva prioridad de misión. La escolta mantiene una distancia de formación de 2–3 casillas alrededor del corredor, en lugar de intentar ocupar siempre la casilla adyacente más cercana. Las unidades de asalto siguen puntuando el avance hacia el objetivo en misiones de captura/mantener y convergen en blancos útiles durante eliminación.

### 3. Grupo defensivo por sectores

Los guardias reciben posiciones preferidas alrededor del objetivo y las pantallas un anillo exterior. El sector se determina de forma estable a partir del ID de unidad, de modo que las unidades no puntúan todas como mejor destino el centro del mismo punto. Las bonificaciones de cobertura siguen contando; la preferencia sectorial no obliga a entrar en una casilla no alcanzable u ocupada.

### 4. Apoyo desde una posición de tiro

Cuando hay un enemigo visible, tanque/ingeniero de apoyo puntúa:

- distancia al alcance de ataque preferido;
- una posición lateral alrededor del blanco, derivada del ID de unidad;
- penalización a quedarse en el lado incorrecto de esa posición.

El punto lateral se construye para mantener una distancia Manhattan de ataque útil cuando la geometría y los límites del mapa lo permiten. Si aún no hay blanco visible durante un asalto al objetivo, el apoyo cubre un punto de la línea media entre el corredor y el objetivo. Durante la defensa ocupa sectores exteriores.

La preferencia de blanco del apoyo también cambia con el plan: en la maniobra de captura da prioridad a las amenazas próximas al corredor o al objetivo, y en defensa protege la zona en vez de perseguir enemigos lejanos.

### 5. Coste acotado

El plan ocupa un byte adicional en `EnemyAI`. La lógica de sector y la posición de tiro usan cálculos escalares; no se crea una tabla de influencia ni se recorre una lista de 60 aliados para cada casilla candidata. La selección de posición sigue explorando únicamente la caja correspondiente a los puntos de movimiento, y los enemigos utilizados para el peligro siguen filtrados por la visión de Rojo.

## Pruebas

En `tests/enemy_ai_test.c`:

- la IA publica el plan de asalto al empezar a capturar y cambia a defensa después de que el punto pase a Rojo;
- la unidad de apoyo adopta una casilla lateral a distancia de ataque, y ataca desde ella;
- se mantienen las pruebas de foco de fuego, escolta, guardias, cobertura y retirada de unidades críticas.

## Validación realizada

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-fanalyzer -Wall -Wextra -Werror -Wformat=2`: 19 módulos C compilados sin diagnósticos.
- Se conservan las pruebas de pathfinding sobre 20.000 mapas y la simulación del turno de IA con 60 unidades.

Las pruebas host usan stubs de SGDK. No sustituyen la compilación real de la ROM ni la comprobación visual/performance en BlastEm; esta edición de código requiere una nueva compilación para llegar a la ROM.
