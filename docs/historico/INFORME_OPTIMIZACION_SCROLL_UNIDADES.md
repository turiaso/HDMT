# Corrección del coste de cola durante el scroll (120 unidades)

## Síntoma

El scroll seguía mostrando caídas de FPS al cruzar bordes en zonas con muchas unidades. Aunque el renderizado ya evitaba recorrer los 120 registros en cada píxel, la cola de unidades que entraban al viewport se reexaminaba y reordenaba al intentar admitir sprites mientras la cámara se movía. Con muchos candidatos pendientes, ese coste crecía con la cantidad de entradas acumuladas.

## Cambio realizado

- La cola lineal de candidatos se sustituyó por buckets de bits ordenados por prioridad y por ID de unidad.
- Elegir el mejor candidato inspecciona un número acotado de palabras: cuatro prioridades útiles por ocho palabras de 16 bits, para un máximo lógico de 128 unidades.
- Retirar un candidato y actualizar su prioridad cambia bits/flags en tiempo constante; no desplaza el contenido de la cola.
- Las candidatas nuevas siguen proviniendo del índice espacial de la fila/columna que entra en el viewport; el scroll normal reposiciona únicamente la lista compacta de sprites activos.
- En un fotograma se crea como máximo un sprite nuevo. Si el paneo vuelve a dejar candidatos obsoletos fuera del viewport, se descartan como máximo ocho por fotograma para evitar otro pico de trabajo.
- La prioridad no cambia: unidad en movimiento, unidad seleccionada, equipo observador, equipo rival; se conserva el desempate por ID menor.
- Si quedan candidatos pendientes tras la limpieza acotada, el flag de refresco se mantiene activo para continuar en el siguiente fotograma.

## Prueba nueva

`testCameraCandidateBitsetKeepsPriorityWithoutQueueShifts()` coloca varias unidades en una columna que entra a la vez en pantalla. Comprueba que la seleccionada se renderiza primero, que el siguiente nivel de prioridad se procesa antes que los rivales, que los candidatos restantes no se duplican y que los índices de sprites activos siguen coherentes.

## Verificación

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: ejecutar de nuevo antes de empaquetar.

## Límite de medición

Este entorno no tiene la toolchain SGDK ni BlastEm configurados, por lo que no se ha generado ni perfilado una ROM real. El cambio elimina el coste lineal y los desplazamientos de la cola de candidatos, pero no demuestra por sí solo que el juego mantenga una tasa concreta de FPS en hardware/emulador.
