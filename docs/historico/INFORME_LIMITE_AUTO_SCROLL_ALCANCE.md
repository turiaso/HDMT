# Corrección del límite de alcance durante el auto-scroll

## Diagnóstico

El cursor estaba restringido por la máscara a nivel de coordenadas lógicas, pero en modo de destino la cámara seguía desplazándose en incrementos de píxeles mientras el cursor permanecía fijo en pantalla. Por ejemplo, una cámara que avanzaba 2 píxeles convertía `cameraX + cursorX` de un múltiplo de 24 a un valor con resto 2. La esquina del sprite dejaba de coincidir con la esquina de la celda. Aunque la celda lógica calculada por división entera todavía fuera alcanzable, el sprite de 24x24 podía solaparse visualmente con una celda adyacente no alcanzable; además, la inspección de la celda bajo el sprite resultaba ambigua.

## Corrección

- El movimiento libre mantiene el cursor suave y la cámara con aceleración, frenado y márgenes configurables.
- Con una unidad seleccionada, el cursor salta una casilla y el auto-scroll avanza en incrementos de `CELL_PIXELS`.
- Antes de aceptar cada paso de cámara, se valida la posición mundial del cursor contra la máscara de alcance y se comprueba que sus coordenadas mundiales estén alineadas con la cuadrícula.
- `C + cruceta` usa la misma validación cuando hay una máscara activa; no existe una vía alternativa para desplazar la cámara fuera del alcance.
- En el borde físico del mapa puede quedar un último desplazamiento inferior a una celda. El cursor se compensa en sentido contrario por esa misma cantidad, de modo que la celda mundial bajo él no cambia ni pierde alineación.
- La cámara no recibe interpolación suave mientras la máscara está activa. Esta decisión es intencional: mantener la cámara pixel a pixel y fijar a la vez el cursor en pantalla rompe la alineación entre el sprite y las celdas.

## Pruebas nuevas

`tests/camera_cursor_test.c` incorpora regresiones para:

- Auto-scroll horizontal y vertical con máscaras lineales estrictas.
- Una cámara que entra en modo de destino con desplazamiento subcelda.
- Alineación exacta de `camera + cursor` y pertenencia a la máscara después de cada fotograma.
- Llegar a los bordes físicos derecho e inferior del mapa sin salir del área ni perder la alineación.
- Intentar continuar fuera de una máscara finita tanto con cruceta normal como con `C + cruceta`.
- Restauración del cursor suave cuando se desactiva la máscara.

## Verificación

- `sh tests/run_tests.sh`: las 15 secciones pasan.
- GCC `-fanalyzer` sin diagnósticos en los módulos revisados.
- Comprobaciones de espacios finales y líneas largas en los archivos C modificados.
- No se ha usado Clang.

Las pruebas de host no reemplazan la compilación real con SGDK ni una comprobación visual de la ROM en BlastEm.

## Limpieza del controlador

La revisión también ha aclarado la nomenclatura interna de `camera_cursor.c` para que las variables expresen su función: `reachableMask` y sus dimensiones describen el área navegable, mientras que `originCellX/Y` identifican la celda de la unidad seleccionada. Los auxiliares ahora se llaman según su responsabilidad (`isCellReachable`, `isAlignedWorldCellReachable`, `updateCursorAndResolveScroll` y `advanceTargetCameraByCell`).

Se ha documentado el contrato público en `camera_cursor.h`, se han expandido las ramas de control que estaban comprimidas en una línea y se han ajustado los nombres de las dimensiones iniciales para distinguir el mapa lógico del tamaño en píxeles del área visible. No se ha añadido estado dinámico por celda ni se han cambiado las reglas de movimiento.
