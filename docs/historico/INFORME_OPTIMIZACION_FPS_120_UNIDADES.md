# Optimización de rendimiento para 120 unidades

## Objetivo

Reducir el trabajo redundante en el escenario 40×40 con 60 unidades por equipo (120 lógicas), sin reducir unidades, modificar las reglas tácticas ni cambiar el presupuesto de sprites de Mega Drive.

## Cambios incluidos

### 1. Vista y seguimiento de cámara

`UnitView` mantiene una lista compacta de los índices que realmente tienen un sprite SGDK activo. Durante un desplazamiento normal de cámara, el motor recorre esa lista (máximo 77 unidades) en vez de inspeccionar los 120 registros lógicos. Las unidades inmóviles actualizan su posición de pantalla usando el delta de cámara sobre la última posición cacheada.

Para detectar unidades que entran en el viewport se usa un índice espacial por filas y columnas. Cuando se cruza un borde, solo se consultan las celdas de esa fila/columna que acaba de entrar; en el mapa de producción de 40×40 no se recorre el ejército completo por cada umbral. Si una prueba o herramienta usa dimensiones fuera del índice espacial, queda un fallback de compatibilidad, no usado por los escenarios de 40×40. Se ha corregido además la detección de umbral para incluir el extremo: si la cámara vuelve de `x=20` a `x=19`, la unidad que vuelve a entrar se registra inmediatamente.

La cola de candidatos del borde se organiza ahora como un conjunto de bits por prioridad y por ID lógico. Elegir la mejor prioridad consulta un máximo fijo de 32 palabras de 16 bits (4 niveles de prioridad útiles × 8 palabras para 128 unidades); quitar o reclasificar un candidato actualiza un bit en tiempo constante y no desplaza una lista. Las prioridades son: unidad que se mueve, seleccionada, equipo observador y equipo rival. Al procesar un candidato se comprueba que siga vivo, visible y dentro del viewport; se descartan como máximo 8 entradas obsoletas por fotograma y se admite como máximo un sprite nuevo por fotograma para evitar picos cuando entran muchas unidades a la vez.

Las pruebas verifican que la lista compacta coincide con los sprites activos, no contiene duplicados, nunca supera el presupuesto de 77 sprites de unidad, responde a la reentrada exacta por el borde y escoge primero la unidad seleccionada entre varios candidatos que entran simultáneamente.

### 2. Niebla de guerra al finalizar movimientos

En el caso habitual de un solo cambio de posición, `FogOfWar_update()` actualiza únicamente el snapshot de esa unidad. Antes recorría de nuevo todos los registros y reconstruía el índice de ambos equipos tras cada movimiento, aunque la composición de los equipos no hubiese cambiado.

El índice se reconstruye solo si cambia su membresía: alta/baja de unidades, muerte/resurrección o cambio de equipo. Los cambios simultáneos siguen el camino conservador que actualiza todos los snapshots modificados y mantiene la corrección de la visión compartida. Esto no altera la regla de exploración persistente.

Para un movimiento normal con 120 unidades, las iteraciones principales de registros pasan de comparar 120 + copiar 120 + reconstruir 120 a comparar 120 + actualizar un snapshot, sin reconstrucción del índice. Esto reduce aproximadamente de 360 a 121 los pasos de esos recorridos específicos (no es una medición de FPS y excluye el trabajo geométrico de la niebla).

Las áreas de movimiento pendientes de Azul también se acumulan solo para unidades que podrían beneficiarse de una casilla que un Rojo visible acaba de abandonar. No se recalculan indiscriminadamente las 60 áreas al cambiar al turno azul, ni se vuelve a drenar trabajo pendiente en cada fotograma del turno rojo.

### 3. Decisiones de la IA

La IA construye la lista de enemigos visibles una sola vez por decisión y la reutiliza para seleccionar blancos, evaluar retiradas y puntuar destinos. Las unidades que no están críticamente debilitadas no ejecutan el cálculo de riesgo de retirada. La lista de amenazas se acota por la distancia Manhattan máxima desde la que un enemigo podría amenazar alguna casilla del movimiento actual, evitando comprobaciones de línea de visión que no pueden afectar a esa decisión.

Estas optimizaciones mantienen el conocimiento limitado a enemigos que Rojo ve. No se añade procesamiento a la animación por fotogramas.

## Verificación realizada

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: sin diagnósticos en la compilación sintáctica de los módulos del juego con stubs SGDK.
- La prueba del buscador sigue verificando 20.000 mapas aleatorios; la simulación de turno de IA sigue completándose con 120 unidades.
- La prueba específica de `UnitView` comprueba la consistencia entre la lista compacta y los sprites activos, el reingreso exacto de una unidad por el borde y la prioridad de candidatos sin desplazar una cola lineal.

## Limitaciones

En este entorno no está configurada la toolchain SGDK y no se ha compilado una ROM ni ejecutado BlastEm. Por tanto, aquí no se ha medido una mejora real de FPS. Las cifras de iteraciones anteriores describen exclusivamente trabajo de código reducido; la validación final requiere comparar la ROM con el contador de FPS durante scroll, movimiento de una unidad y un turno completo de IA.

### 4. Seguimiento de cámara durante el turno rojo

El seguidor anterior usaba `CameraCursor_isWorldPixelFocused()` para bloquear la siguiente acción de IA hasta centrar exactamente a cada actor. Con 60 unidades rojas, eso provocaba panorámicas largas entre acciones incluso cuando el siguiente actor ya estaba en pantalla. Además, la cámara seguía a la unidad a 4 px/frame, igual que la velocidad de movimiento, de modo que apenas recuperaba margen y podía desplazar el mapa durante casi toda la ruta.

Ahora la IA puede actuar cuando el centro de la unidad está en una zona segura de una celda de margen; en el borde real del mapa se acepta el margen que la cámara no puede alcanzar. La cámara solo empieza a seguir cuando un actor cruza ese margen y usa histéresis: mientras ya está siguiendo una unidad en movimiento, deja de desplazar el mapa al regresar a una zona interior de tres celdas. El auto-encuadre avanza a 6 px/frame frente a los 4 px/frame de la unidad, lo que permite recuperar margen en vez de quedarse pegado a ella. Los actores que ya están visibles no fuerzan un recentrado exacto.

La prueba de cámara cubre la zona interior, la decisión de recentrado cerca del borde y los casos de borde inicial/final del mapa.

### 5. Presupuesto SAT al entrar unidades en el viewport

Cuando la cámara cruzaba un umbral y varias unidades entraban en pantalla, la reserva de cada sprite nuevo volvía a buscar el sprite de menor prioridad recorriendo hasta 120 registros. Esa selección ahora recorre el índice compacto de sprites realmente activos, limitado a 77 entradas. Se conserva el orden de prioridad visual de la unidad móvil, la seleccionada, el equipo observador y el equipo rival.
