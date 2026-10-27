# Corrección del cursor y optimización del alcance

## Modos de cursor separados

El comportamiento depende de si hay una unidad seleccionada:

- **Exploración libre (`movementMask == NULL`)**: el cursor vuelve a desplazarse suavemente hacia la siguiente celda lógica (3 px por fotograma); la cámara conserva su aceleración, márgenes y scroll suave. La repetición de la cruceta vuelve al intervalo anterior de 8 fotogramas.
- **Selección del destino (`movementMask != NULL`)**: al seleccionar una unidad, el cursor se alinea con su casilla y salta exactamente una celda (24×24 px) por paso. No puede abandonar la máscara de alcance ni su casilla de origen. El auto-scroll también avanza por celdas completas y valida cada destino antes de mover la cámara; la corrección de borde está documentada en `INFORME_LIMITE_AUTO_SCROLL_ALCANCE.md`. La repetición del cursor en este modo es independiente (4 fotogramas).
- Al retirar la máscara, se limpia el estado de repetición y se recupera el movimiento suave libre. La cámara solo cambia al desplazamiento por celdas durante el modo de destino: así el sprite, la cuadrícula y el alcance permanecen alineados.

## Causa de la lentitud y correcciones

Había tres trabajos que afectaban al modo de selección:

1. **Separar hover de búsqueda.** El HUD lee el alcance precalculado mediante `Pathfinder_getReachableInfo()` en O(1). Se eliminó cualquier búsqueda perezosa desde las consultas del cursor: al recorrer casillas no se ejecuta `Pathfinder_findRoute()` ni se reconstruye el alcance. La ruta concreta se calcula al confirmar con la segunda A.
2. **Búsqueda de alcance con demasiados escaneos.** La implementación anterior buscaba el menor coste recorriendo el mapa por cada nodo. Ahora se procesan niveles de coste en orden y el escaneo queda restringido al rectángulo situado a `maxCost` celdas de cada eje desde el origen. Como cada entrada a terreno cuesta al menos 1, ninguna celda alcanzable dentro del presupuesto puede quedar fuera de ese rectángulo. Los arrays se limpian como antes, pero el trabajo de búsqueda evita explorar celdas lejanas que no pueden entrar en el alcance. No se utiliza memoria dinámica ni se añade ningún búfer.
3. **HUD redundante.** Solo se vuelven a dibujar filas de información de celda y estados cuyo texto realmente cambió.

Se mantuvieron los costes de terreno, los obstáculos, la ocupación, los eventos, la confirmación/cancelación y la actualización del glow. **Corrección posterior:** cuando el cursor está restringido por una máscara, la cámara deja de usar scroll suave y avanza por celdas; mantener ambos movimientos suaves permitiría desalinear el cursor y las casillas. `INFORME_LIMITE_AUTO_SCROLL_ALCANCE.md` documenta el diagnóstico, los límites y las regresiones añadidas.

## Verificación

- `sh tests/run_tests.sh`: pasan las 15 secciones disponibles, incluidos cursor libre suave, cursor restringido, mapa, rutas, presupuesto, HUD, unidades y glow.
- GCC `-fanalyzer`: sin diagnósticos en `camera_cursor.c`, `pathfinder.c`, `unit_controller.c`, `movement_glow.c` y `main.c`.
- Prueba diferencial aleatoria sobre 500 mapas: coincide con la implementación anterior en alcanzabilidad, costes mínimos, longitud/coste de las vistas previas y direcciones de las rutas. También pasó con AddressSanitizer y UndefinedBehaviorSanitizer.
- Casilla límite de 254 pasos: el valor `pasos + 1` llega a 255 sin desbordarse.
- El test de cámara comprueba que la exploración libre anima píxel a píxel, que el modo destino salta de casilla en casilla y que el cursor queda restringido a la máscara.

### Benchmark de host

Comparación de `Pathfinder_buildReachability()` con GCC `-O2`, usando la versión del proyecto anterior a esta corrección como base. Los tiempos son para la función en host, no una medida directa de fotogramas en Mega Drive; cada fila usa el número de iteraciones mostrado y se compara el tiempo medio por llamada.

| Mapa | Presupuesto | Versión anterior (iteraciones / tiempo total) | Versión corregida (iteraciones / tiempo total) | Mejora aproximada por llamada |
|---|---:|---:|---:|---:|
| 100×100 | 8 | 80 / 67,585 ms | 5000 / 26,165 ms | 161× |
| 100×100 | 40 | 80 / 1613,606 ms | 500 / 172,201 ms | 59× |
| 200×200 | 8 | 80 / 277,733 ms | 2000 / 12,503 ms | 555× |

No se ha usado Clang. Las pruebas se ejecutaron con GCC y stubs de host. La ROM todavía debe compilarse con tu SGDK y comprobarse visualmente en BlastEm o en la consola; estas mediciones no prueban la presentación del VDP.


## Revisión de separación (versión actual)

La vista previa es ahora estrictamente de solo lectura respecto a `PathfinderWorkspace`: no hay búsqueda perezosa si la caché falta. La máscara se construye una vez al seleccionar la unidad; `Pathfinder_findRoute()` solo se llama al confirmar el destino con la segunda A. Ver `INFORME_SEPARACION_ALCANCE_Y_RUTA.md`.
