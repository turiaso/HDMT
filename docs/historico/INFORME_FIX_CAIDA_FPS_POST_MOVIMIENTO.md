# Corrección de bajadas a 5 FPS tras completar un movimiento

## Causa identificada

La revisión anterior repartía una búsqueda completa de alcance por fotograma para cada unidad aliada marcada como candidata. Cada búsqueda además podía descubrir casillas y solicitar su actualización visual. Al atravesar una ruta se marcaban como candidatas las aliadas cercanas a cada celda que quedaba libre; en formaciones densas eso podía producir decenas de fotogramas consecutivos con pathfinding y refrescos de niebla.

Esa cola era redundante para movimientos azules: `revealPlayerMovementAreas()` enmascara temporalmente los ocupantes azules durante cada cálculo, de modo que una unidad azul no bloquea el cálculo del alcance de sus compañeras. Moverla no cambia los resultados de alcance de las demás.

## Cambio

- Eliminada la notificación que encolaba candidatas por cada celda intermedia del movimiento.
- Un movimiento azul recalcula una sola vez el alcance completo de la unidad que ha cambiado de posición, también si ha agotado sus puntos de movimiento.
- Se conserva el recálculo localizado de unidades azules cercanas cuando una unidad roja abandona una casilla y puede desbloquearles rutas.
- Se conserva el recálculo completo de seguridad para cambios simultáneos de topología y para el cambio de contexto de visión.
- Añadida una regresión de prueba que comprueba que el movimiento de una aliada explora su nuevo rango y no deja búsquedas redundantes pendientes.

## Verificación

- `tests/run_tests.sh`: 23/23 comprobaciones completadas.
- `tests/run_tests_sanitized.sh`: 23/23 comprobaciones completadas con ASan/UBSan cuando están disponibles.
- Se han ejecutado pruebas con 20.000 mapas aleatorios y una simulación de un turno de IA con 120 unidades (60 por equipo).

No se ha compilado ni ejecutado una ROM en BlastEm en este entorno; por tanto, no se afirma una cifra de FPS medida en emulador.
