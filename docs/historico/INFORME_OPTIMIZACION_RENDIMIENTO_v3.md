# Informe de optimización de rendimiento v3

## Alcance

Esta pasada revisa el recorrido por fotograma, la actualización de sprites, el scroll del mapa, el parcheo de tiles, la niebla, la previsualización de rutas y los eventos de casilla en el escenario de 40×40 casillas y 60 unidades.

El objetivo ha sido quitar trabajo que no modifica el resultado visual o lógico. Se mantiene la arquitectura de datos de las casillas; no se implementan las propuestas aplazadas de optimización de RAM.

## Cambios implementados

### 1. Vista de unidades (`src/unit_view.c` / `src/unit_view.h`)

- Un fotograma quieto sin movimiento, cambios de cámara ni invalidaciones sale de `UnitView_update()` sin leer los registros de las unidades.
- Durante un scroll sin cambios de estado se usa la caché de posiciones lógicas, vida y visibilidad. Los sprites activos se reposicionan únicamente si cambió su coordenada de pantalla.
- Las unidades que aún no tienen sprite solo se comprueban para crear su sprite cuando la cámara ha cruzado un límite por el que podrían entrar en el viewport.
- Las unidades fuera de pantalla o no visibles liberan su instancia de sprite. Las unidades visibles que entran se crean bajo demanda.
- Los patrones gráficos de los tipos de unidad se precargan una sola vez en VRAM. Las instancias se crean con `SPR_addSpriteEx` sin `SPR_FLAG_AUTO_TILE_UPLOAD`, evitando que cada reaparición pida volver a subir los mismos patrones.
- Se añade `UnitView_invalidate()` para invalidar explícitamente la vista tras combate/eliminaciones, incluso cuando el cambio de vida no altera el mapa de niebla.
- El movimiento subcelda actualiza solo la unidad que está animándose cuando no hay cambios simultáneos de cámara o niebla.

### 2. Parcheo del mapa durante el scroll (`src/movement_glow.c`)

- `patchMapTileData()` procesa los tiles por tramos que pertenecen a una misma casilla lógica (3 tiles por eje) y obtiene el estilo de la casilla una vez por tramo, no una vez por tile.
- El estilo normal, alcanzable o no explorado se obtiene de la caché de estilo por casilla cuando esta está disponible.
- Las actualizaciones de niebla de casillas contiguas se agrupan en tiras horizontales mediante `MovementGlow_refreshFogCellRun()`. Se respetan los límites de plano circular y, cuando hay vuelta al inicio del plano, la tira se divide correctamente.
- No se añade una tabla de 1600 entradas adicional para las transferencias agrupadas; el buffer temporal permanece acotado al ancho visible.

### 3. Niebla de guerra (`src/fog_of_war.c`, `src/main.c`)

- La niebla sigue recalculándose por cambios pendientes y no como una operación incondicional en cada fotograma.
- El cálculo de visibilidad recorre el índice de unidades vivas del equipo en lugar de las 60 unidades globales.
- Para un movimiento de una única unidad, las zonas comunes entre los cuadrados de visión anterior y nuevo conservan su resultado. Solo se recalculan las bandas que pueden cambiar. En la prueba con radio de visión 4 y movimiento horizontal, el número de comprobaciones costosas cae de 90 a 18 dentro del rectángulo de actualización; el resto de celdas no necesita volver a probarse.
- Si cambian varias unidades, se conserva la ruta general de actualización de región.
- Las casillas contiguas cuyo tile de niebla realmente cambia se agrupan para reducir escrituras pequeñas al plano de vídeo.

### 4. Alcance y cursor (`src/unit_controller.c`)

- `getMovementRangeInfo()` ya no oculta temporalmente a los enemigos no visibles para luego restaurarlos antes de consultar el alcance. `Pathfinder_getReachableInfo()` lee una tabla de alcance ya calculada y no consulta la ocupación, así que esos barridos no afectaban al resultado.
- La ocultación de enemigos se conserva en las operaciones que sí reconstruyen una ruta o el alcance usando la ocupación del mapa.
- El hover sigue usando la caché de previsualización; buscar una ruta se reserva para la confirmación de la orden.

### 5. Eventos de casilla (`src/cell_events.c`)

- La actualización de hover evita búsquedas y despacho de eventos STAY en casillas que no tienen callback STAY registrado.
- Cuando hay un callback STAY, se mantiene su ejecución normal; no se cambia la semántica de los perfiles que usan ese evento.

### 6. Tabla de sprites y HUD (`src/main.c` y módulos de vista)

- `SPR_update()` solo se ejecuta cuando una operación ha marcado la tabla de sprites como modificada. El fotograma quieto conserva la tabla SAT existente.
- El HUD y el alcance mantienen sus cachés para no redibujar textos o el mapa si sus datos no han cambiado.

## Nota sobre `MAP_scrollTo`

La función de scroll no hace una reconstrucción completa del tilemap en cada píxel: el código fuente de SGDK compara las coordenadas de metatile (desplazamiento dividido entre 16) y solo repone filas o columnas al cruzar esos límites. Aun así, cada reposición activa el callback de parcheo, por lo que ese callback sí debía minimizar cálculos repetidos. Esta pasada optimiza ese camino caliente y mantiene la llamada de scroll necesaria para el desplazamiento suave.

Referencia consultada: https://raw.githubusercontent.com/Stephane-D/SGDK/master/src/map.c

## Verificación realizada

- `sh tests/run_tests.sh`: las 18 pruebas/grupos pasan, incluidas las pruebas de vista de unidades, niebla, parcheo/alcance, combate e integración del escenario 40×40 con 60 unidades.
- GCC con `-Wall -Wextra -Werror -fanalyzer -fsyntax-only` sobre `unit_view.c`, `movement_glow.c`, `unit_controller.c`, `fog_of_war.c` y `cell_events.c`: sin diagnósticos.
- GCC con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer -fsyntax-only` sobre `main.c` usando los stubs de prueba SGDK: sin diagnósticos.
- Integridad del archivo ZIP: comprobada tras generarlo.

## Limitación importante

En este entorno `GDK` no está configurado. Por tanto, no se ha ejecutado una compilación con el toolchain SGDK real ni una prueba cronometrada en BlastEm o en una Mega Drive. Las pruebas de host y el análisis estático verifican aspectos funcionales y de código, pero no proporcionan una cifra real de FPS. El rendimiento percibido debe confirmarse en la build SGDK del usuario.
