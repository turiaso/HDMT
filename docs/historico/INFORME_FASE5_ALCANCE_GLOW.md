# Fase 5 · Alcance de movimiento con glow

## Resultado

Se ha sustituido la representación de la ruta mediante marcadores por una vista de **todas las casillas alcanzables**. La ruta de menor coste sigue calculándose internamente para mover la unidad tras la confirmación; lo que desaparece es su representación visual.

## Comportamiento

- Seleccionar una unidad del equipo activo calcula el alcance a partir de sus puntos de movimiento restantes, los costes de entrada a cada terreno y la ocupación del mapa.
- Cada casilla alcanzable se ilumina utilizando PAL1. La casilla ocupada por la propia unidad se mantiene sin glow, pero el cursor puede regresar a ella.
- El cursor avanza una casilla lógica por cada paso (24 píxeles), y no entra en casillas fuera del alcance. Sin selección, el cursor vuelve a moverse libremente por el mapa.
- Sin unidad seleccionada, la cámara conserva el movimiento suave. Con la máscara de alcance activa, el auto-scroll avanza por celdas completas y valida cada posición antes de aplicarla; en el borde físico puede compensar un resto menor que una celda con el cursor para mantener la alineación. Esta corrección posterior está detallada en `INFORME_LIMITE_AUTO_SCROLL_ALCANCE.md`.
- La primera pulsación de A prepara la orden; la segunda, en la misma casilla, la confirma. B retira la selección y el glow. El path no se dibuja.
- Al comenzar el movimiento o cambiar de turno se retira la máscara. Cuando la orden termina correctamente, la unidad se deselecciona y el glow permanece apagado; si un bloqueo interrumpe la ruta, se conserva la selección para poder planificar de nuevo.

## Implementación

### Máscara de alcance

`Pathfinder_buildReachability()` reutiliza la búsqueda ponderada de Dijkstra y genera una máscara de un byte por casilla. No utiliza simplemente distancia Manhattan: tiene en cuenta costes, agua, ocupación y presupuesto restante. El origen no se marca como destino alcanzable porque ya está ocupado.

El búfer de alcance es independiente del búfer utilizado para calcular y guardar la ruta. Así una vista previa no destruye la máscara y el alcance no sobrescribe una ruta durante la animación.

### Paleta animada

`MovementGlow_init()` copia los 16 colores de la paleta base PAL0 a PAL1. `MovementGlow_update()` modula las intensidades RGB con un ciclo de seis niveles y un intervalo configurable. El efecto cambia la paleta, no el gráfico de los tiles ni los sprites.

El callback de `MAP_setDataPatchCallback()` modifica únicamente el campo de paleta de los atributos de tile: pone PAL1 en los tiles cuyas coordenadas pertenecen a casillas marcadas en la máscara y PAL0 en los demás. Conserva el índice del tile, prioridad y bits de volteo. El callback se aplica también a las filas y columnas que SGDK prepara durante el scroll. Cuando cambia la máscara se fuerza un redibujado del área visible para actualizar las celdas ya cargadas. La documentación de SGDK describe el callback como un parche del tilemap antes de enviarlo al VDP y documenta `MAP_scrollToEx(..., TRUE)` para forzar el redibujado: https://stephane-d.github.io/SGDK/map_8h.html

### Asignación de paletas

- PAL0: terreno original.
- PAL1: copia animada del terreno, usada solo por tiles de casillas alcanzables.
- PAL2: equipo Azul, cursor y máscaras de viewport.
- PAL3: equipo Rojo y color blanco usado por el HUD.

No se ha cambiado la inicialización de la fuente SGDK ni se han añadido escrituras directas a VRAM para simular el brillo.

## Módulos modificados

- `src/pathfinder.c/.h`: construcción de la máscara de alcance ponderada.
- `src/unit_controller.c/.h`: refresco del alcance usando puntos actuales y estado de la unidad.
- `src/camera_cursor.c/.h`: movimiento del cursor por casillas y restricción por máscara.
- `src/movement_glow.c/.h`: copia y pulso de PAL1, parcheo de atributos de tiles y refresco visible.
- `src/main.c`: coordinación del estado de selección, máscara, redibujado y animación de la paleta.
- `src/unit_view.c/.h`: retirada de los sprites marcadores de ruta; la vista solo administra unidades.
- `src/team.c`: documentación de la asignación actual de paletas.
- `tests/`: pruebas de máscara, navegación restringida, copia/pulso de paleta, parcheo por filas/columnas e integración.

## Nota de estabilidad: transferencias de paleta y fin de misión

La versión anterior actualizaba PAL1 con `PAL_setPalette(..., DMA)` desde `MovementGlow_update()`, que se llama en el bucle principal antes de `SYS_doVBlankProcess()`. Ese método hace una transferencia DMA inmediata; escribir CRAM durante la zona activa de pantalla encaja con los artefactos conocidos como *CRAM dots*. Ahora todas las llamadas de paleta del proyecto usan `DMA_QUEUE_COPY`, que copia los datos de origen y encola la transferencia para el siguiente VBlank. El test de glow comprueba el método de transferencia y el script de pruebas falla si detecta una escritura inmediata de una paleta.

El pulso de PAL1 tampoco genera transferencias si la máscara no contiene ninguna casilla alcanzable. El bucle principal lo deja de actualizar cuando termina la misión. Se ha añadido una prueba que confirma la otra condición que puede parecer un bloqueo: si una ruta atraviesa el objetivo `(10, 10)`, la victoria es inmediata y la ruta se detiene en esa casilla. El HUD ahora muestra `VICTORIA: OBJETIVO CAPTURADO` además del resultado final.

Esta es una corrección validada en el código y con stubs de GCC; no se ha podido compilar ni ejecutar la ROM real en SGDK/BlastEm en este entorno. Si el bloqueo observado no coincide con la captura del objetivo, habrá que verificar esa ubicación en la ROM tras aplicar este parche para distinguir una parada de misión de un bloqueo hardware.

## Verificaciones realizadas

Ejecutado con GCC, sin Clang:

- `sh tests/run_tests.sh`: las 15 secciones pasan, incluida la prueba de captura a mitad de ruta y la guardia contra `PAL_setPalette(..., DMA)` inmediato.
- `GCC -fanalyzer`: sin diagnósticos en los módulos de movimiento, turno, paleta y bucle principal revisados.
- Sin espacios finales en código, documentación y scripts de prueba.
- Se verifican tanto los cambios de atributos de paleta como la conservación de índice de tile, prioridad y bits de tile.

## Pendiente de verificación en hardware/emulador

Las pruebas host usan stubs de SGDK: no prueban la transferencia DMA real ni el resultado visual final. Hace falta compilar con la instalación real de SGDK y revisar en BlastEm que el brillo se vea bien, que el área visible no pierda PAL1 durante el scroll y que el HUD y las paletas de los equipos permanezcan intactos. No se declara esa prueba como realizada.
