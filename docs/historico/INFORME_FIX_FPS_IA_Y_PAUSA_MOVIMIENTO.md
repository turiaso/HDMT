# Corrección de coste por fotograma en turno de IA y final de movimiento

## Cambios realizados

### IA táctica
- `selectBestDestination()` descarta primero las casillas no alcanzables y la propia casilla de origen.
- La puntuación estratégica se calcula después de confirmar que la casilla es alcanzable.
- Las puntuaciones tácticas específicas del rol solo se calculan para candidatos que superan los filtros anteriores.
- En la puntuación normal, se usa una cota inferior segura: como el riesgo nunca resta puntuación, si ni siquiera el resultado ideal puede superar la mejor candidatura, se omiten las comprobaciones de amenazas y línea de visión para esa casilla. El desempate se conserva usando una comparación estricta.
- Esto evita evaluar una gran parte del rectángulo de movimiento que nunca podía ser seleccionada y reduce análisis LOS que no podrían cambiar la decisión.

### Pathfinder
- `runCostSearch()` inicializa `costs` y `parentDirections` solo en el rectángulo delimitado por el presupuesto de movimiento, en vez de limpiar las 1.600 entradas para cada búsqueda.
- Se mantiene la limpieza de `reachable` en toda la cuadrícula cuando se construye un alcance, porque los clientes pueden consultar cualquier casilla.
- Se elimina una limpieza duplicada de `reachable` al construir alcance.
- `Pathfinder_findRoute()` rechaza antes de buscar los destinos cuya distancia Manhattan excede el presupuesto. Es una condición necesaria: cada paso cuesta al menos un punto.

### Refresco de niebla tras mover una unidad
- `revealPlayerMovementAreas()` sigue usando el pathfinder real, la ocupación temporal y la persistencia de la exploración.
- Al mezclar el alcance resultante con la niebla, recorre solo el rectángulo máximo posible según los puntos de movimiento de cada unidad, en vez de escanear las 1.600 casillas del mapa por cada unidad recalculada.
- La envolvente inicial y la semántica de casillas exploradas/visibles se mantienen.

## Verificación

Ejecutados en la carpeta del proyecto:
- `tests/run_tests.sh`: las 23 comprobaciones pasan.
- `tests/run_tests_sanitized.sh`: las 23 comprobaciones pasan con ASan/UBSan.
- La prueba de 20.000 mapas aleatorios sigue pasando.
- La simulación de turno completo de IA con 60 unidades mantiene sus resultados: 727 frames de simulación y 30 movimientos.

## Limitaciones

No se ha compilado una ROM con la instalación real de SGDK ni medido FPS en BlastEm o hardware. Las pruebas demuestran que la lógica y las rutas siguen siendo coherentes; el impacto exacto en FPS debe medirse con el contador del juego.
