# IA táctica coordinada para el equipo Rojo

## Principios de decisión

`EnemyAI_update()` recibe el `MissionObjective` completo. La IA sigue siendo determinista, pero ya no decide como si todas las unidades fueran iguales: al principio de cada turno asigna roles compactos y las puntuaciones de movimiento/ataque usan ese rol.

- **Capturar/mantener sin controlar el punto:** elige un corredor a partir de la distancia, los puntos de movimiento y una bonificación de movilidad para los exploradores. También asigna una escolta cercana que evita dejarlo aislado y prioriza amenazas cerca del corredor.
- **Defender un punto ya controlado:** designa hasta dos guardias cercanos y resistentes. Buscan mantenerse a pocas casillas del punto y aprovechar cobertura; el resto crea una pantalla alrededor del objetivo o usa sus roles de flanqueo/apoyo.
- **Eliminar unidades:** persigue únicamente enemigos visibles para Rojo. Si no ve ninguno, avanza hacia el punto de reunión conocido para explorar, sin consultar coordenadas ocultas.
- **Sobrevivir turnos:** si Rojo es el equipo que debe sobrevivir, crea guardias y pantalla alrededor de la zona defensiva; si debe sobrevivir Azul, Rojo persigue y ataca al equipo objetivo.

## Roles compactos por unidad

`EnemyAI` reserva una tabla de 128 bytes (`u8 tacticalRoles[ENEMY_AI_MAX_UNITS]`), con una entrada por unidad lógica; el escenario principal utiliza 120. La tabla se recalcula una vez al inicio de cada turno rojo, no en cada fotograma.

- **Corredor del objetivo:** puntúa más alto el progreso hacia el punto.
- **Escolta:** intenta mantener una distancia corta del corredor y prioriza amenazas cercanas a él. Si el corredor cae, deja de seguir sus coordenadas antiguas.
- **Guardia:** prefiere posiciones dentro de dos casillas del punto y recibe una bonificación por cobertura.
- **Pantalla:** mantiene un perímetro de hasta cinco casillas en misiones defensivas y busca terreno protegido.
- **Flanqueador:** los exploradores buscan una posición lateral del enemigo conocido en lugar de converger todos por la misma casilla. Al aproximarse a un objetivo, se reparten alrededor de él.
- **Apoyo de fuego:** blindados e ingenieros priorizan amenazas peligrosas; al moverse, los blindados favorecen posiciones que aprovechan su alcance de ataque.
- **Asalto:** las unidades de infantería continúan avanzando y combatiendo según la misión.

## Retirada de unidades críticas

Las unidades con un tercio de vida máxima o menos revisan el riesgo calculado desde enemigos que Rojo ve y que pueden alcanzar la casilla. Si la amenaza puede ser letal y la unidad no puede lograr una eliminación inmediata segura, intenta retirarse a una posición con menor riesgo, más distancia y cobertura. El movimiento se anima con el bucle común; al terminarlo, la unidad no vuelve a atacar durante ese turno, evitando que la IA cancele su propia retirada.

La puntuación de amenaza solo usa enemigos visibles para Rojo. No simula unidades ocultas, no añade un mapa de influencia por casilla ni requiere una búsqueda profunda. El índice de enemigos visibles se cachea por decisión en una tabla temporal con capacidad de 128 unidades. La selección de destinos se limita a la zona alcanzable de la unidad.

## Elección de ataques

Antes de atacar, la IA evalúa `Combat_predictUnits()` para estimar daño, baja inmediata y represalia. Los blancos peligrosos cerca del objetivo o del corredor reciben mayor prioridad para los roles que deben defenderlos; blindados y unidades de apoyo valoran objetivos ofensivamente fuertes. Se evita una represalia letal cuando no compensa y una baja segura suele valer más que un ataque débil.

### Coordinación adaptativa entre unidades

`focusTargetIndex` guarda el blanco prioritario del turno. Si una unidad identifica un enemigo visible adecuado para la ofensiva, las siguientes unidades pueden favorecer el mismo objetivo al aproximarse y atacar. El foco tiene una bonificación de puntuación, no una orden absoluta: una represalia potencialmente letal sigue penalizando el ataque, los roles de corredor conservan la prioridad de misión y los guardias solo se desvían hacia amenazas cerca del objetivo. Si el blanco muere, deja de estar visible o deja de ser válido, el foco se limpia y la IA vuelve a evaluar los candidatos visibles. Al sobrevivir al ataque, el objetivo conserva el foco para permitir concentrar daño en varios ataques.

La IA también recuerda el equipo que ocupa el objetivo. Si cambia de manos durante el turno (por ejemplo, el corredor entra en un punto de mantener), reasigna los roles en la siguiente decisión: corredor/escolta se sustituyen por guardias y pantalla según el nuevo control. No espera al turno siguiente para actualizar la estrategia.

`Combat_predictUnits()` refleja las banderas de baja previstas sin modificar la vida, ocupación o acciones de las unidades. `Combat_attack()` sigue aplicando el resultado real a través de la misma predicción.

## Rendimiento y memoria

La coordinación usa 60 bytes de roles, un índice de blanco coordinado (`s16`) y un byte para recordar el último propietario del objetivo, además del estado previo. Durante el reparto de guardias se usa una máscara temporal de 128 bytes para marcar unidades seleccionadas como guardias. Los candidatos de movimiento siguen limitados a la caja del presupuesto de movimiento; no se exploran las 1.600 casillas del mapa por cada unidad. El foco se recalcula por decisiones de unidad, no cada fotograma.

La IA procesa una decisión por llamada; el movimiento y el seguimiento de cámara continúan en el bucle común. La retirada se comunica con `ENEMY_AI_RETREAT_STARTED`, y el HUD presenta `IA ROJA: RETIRADA`.

## Pruebas

Dentro del grupo de IA de `tests/run_tests.sh`, `tests/enemy_ai_test.c` prueba:

1. aproximación/flanqueo hasta entrar en rango y posterior ataque;
2. prioridad del punto de captura frente a un enemigo lejano;
3. preferencia por una eliminación rentable;
4. defensa del objetivo y uso de una casilla rocosa;
5. asignación de corredor, escolta y apoyo;
6. retirada de una unidad crítica ante una amenaza letal, sin atacar después;
7. concentración de fuego en un objetivo que sobrevive al primer ataque, aunque la unidad siguiente tenga otra alternativa;
8. reasignación de corredor a guardia cuando la IA captura el punto de mantener durante el turno.

`tests/ai_full_turn_test.c` simula un turno completo con el despliegue de 120 unidades (60 por equipo). La suite completa tiene 23 grupos; se ejecutaron `sh tests/run_tests.sh` y `sh tests/run_tests_sanitized.sh`, ambos con los 23 grupos superados. GCC `-fanalyzer -Wall -Wextra -Werror -Wformat=2` tampoco genera diagnósticos en los 19 módulos C (usando `-Dmain=sgdk_main` para el punto de entrada de SGDK).

## Límites actuales

La coordinación es ligera y determinista: roles por turno, un blanco común y evaluación local de casillas alcanzables. No hace búsqueda minimax ni simula todas las respuestas del jugador a varios turnos. El combate y la visión actuales sí respetan oclusión de bosques y piedras mediante `MapData_hasLineOfSight()`; la IA no simula todas las posibles líneas de fuego futuras. La amenaza y el alcance de ataque siguen usando distancia Manhattan.

No se ha generado una nueva ROM SGDK ni ejecutado BlastEm desde este entorno. La compilación real y la revisión visual deben hacerse con la instalación local del toolchain.


## Actualización: maniobras por grupos

Ver `INFORME_MANIOBRAS_GRUPO.md` para planes por misión, sectores de guardia/pantalla, formación de escolta y posiciones laterales de tiro del apoyo.
