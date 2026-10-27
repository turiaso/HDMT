# Informe: IA táctica consciente del terreno y de la clase

## Resumen

Se ha mejorado `src/enemy_ai.c` para que la IA roja valore el terreno y las capacidades de la unidad que está tomando la decisión. Se conserva el movimiento animado en el bucle principal y las reglas autoritativas de pathfinding y combate.

## Cambios implementados

### Alcance propio de ataque

`strategicPositionScore()` usaba el alcance del enemigo objetivo al estimar a qué distancia debía aproximarse el atacante. Ahora recibe también la unidad que se mueve y calcula su distancia preferida usando el `attackRange` propio. Una infantería ya no debería quedarse a dos casillas simplemente porque su objetivo sea un blindado con alcance dos.

### Preferencias posicionales por clase

`classTerrainPositionPenalty()` añade un ajuste de puntuación, aplicado solo a destinos que ya son alcanzables:

- **Infantería:** prefiere posiciones con defensa de terreno.
- **Explorador:** recibe una preferencia ligera por cobertura de bosque y conserva su papel de flanqueo.
- **Ingeniero:** valora más la cobertura para proteger su función de apoyo.
- **Blindado:** favorece caminos y penaliza posiciones en bosque y, especialmente, piedras.

Estos ajustes no cambian la transitabilidad ni sustituyen `UnitType_getTerrainMoveCost()`. El pathfinder continúa siendo la fuente autoritativa de los costes y de la máscara de destinos legales.

### Línea de fuego

`firingLanePositionPenalty()` favorece las posiciones que, al acabar el movimiento, quedan dentro del alcance propio de la unidad con línea de visión despejada. Penaliza las posiciones que están a alcance geométrico, pero desde las que bosque o rocas bloquean el tiro. El ataque real y su predicción siguen validados mediante `combat.c` y `MapData_hasLineOfSight()`.

Los corredores de las misiones de captura/mantener conservan prioridad sobre estas preferencias opcionales mientras el objetivo no pertenezca a Rojo. Así la bonificación de cobertura o tiro no desvía al corredor del objetivo de misión.

### Ingeniero de apoyo

Si no se ha seleccionado un enemigo visible y el ingeniero conserva su acción de reparación, `findBestRepairApproachTarget()` elige un aliado herido una vez por decisión: mayor porcentaje de vida perdido, luego pérdida absoluta y, en empate, menor distancia. `selectBestDestination()` busca quedar dentro de `repairRange` de ese aliado. Al llegar, la reparación efectiva vuelve a pasar por `Combat_predictRepairUnits()`/`Combat_repair()`; el cálculo de movimiento no modifica vida ni consume acciones.

La IA sigue reparando primero a un aliado adyacente cuando la reparación ya es legal. No se hacen cálculos tácticos adicionales durante la animación.

## Pruebas añadidas

En `tests/enemy_ai_test.c` se añaden tres regresiones:

1. Una infantería roja se aproxima al alcance de su propio tipo y ataca desde distancia Manhattan uno, aunque el objetivo sea un blindado con alcance dos.
2. Un ingeniero se mueve hasta quedar junto a un aliado herido y lo repara en la siguiente decisión.
3. Un blindado elige el desvío por camino frente a una alternativa rocosa con progreso estratégico equivalente.

Se conservan las pruebas de corredor de captura, uso de cobertura, retirada por amenaza letal, concentración de fuego, posiciones de apoyo y línea de tiro bloqueada.

## Verificación realizada

- `sh tests/run_tests.sh`: 23/23 grupos y el test extra de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos y el test extra superados con ASan/UBSan.
- GCC `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: los 21 módulos C pasan sin diagnósticos.
- Orden de inclusiones SGDK: verificado por la suite.
- Se mantienen la simulación de turno de IA con 120 unidades y la prueba de pathfinding sobre 20.000 mapas.
- Documentación revisada; enlaces relativos válidos y sin espacios finales en los archivos añadidos/modificados.

Las pruebas host utilizan stubs de SGDK. No se ha generado una ROM con la instalación real de SGDK ni se ha ejecutado BlastEm en este entorno, de modo que este informe no afirma una medición de FPS ni una validación visual de hardware.
