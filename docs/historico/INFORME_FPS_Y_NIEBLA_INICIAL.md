# FPS en pantalla y cobertura inicial de niebla

## FPS
- Se muestra `FPS: NN` en la esquina superior derecha de la zona de juego, en la fila superior.
- Se utiliza `VDP_showFPS(FALSE, 33, 0)` de SGDK, que actualiza el contador una vez por frame.
- La máscara de viewport tapa las columnas a partir de la 35; por eso la etiqueta ocupa las columnas 28–32 y los dos dígitos las columnas 33–34. No se dibuja bajo la máscara.
- No se hace muestreo adicional ni se ejecuta un cálculo de FPS propio en el bucle; se aprovecha el contador de SGDK.

## Niebla al inicio
- Se conserva el cálculo real de visión (radio y oclusión por terreno) como estado `VISIBLE`.
- Al iniciar, se unen las casillas que cada unidad azul puede alcanzar con sus puntos de movimiento, sin filtrar por línea de visión. Las casillas de movimiento se marcan `EXPLORED`, nunca `VISIBLE`.
- Durante este cálculo exploratorio, las unidades aliadas se enmascaran temporalmente como ocupantes para evitar que una tropa cree huecos artificiales entre las áreas de movimiento. Los enemigos ocultos también se enmascaran para no revelar sus posiciones por este procedimiento; los enemigos visibles siguen siendo obstáculos.
- Se obtiene además la envolvente convexa conjunta de los extremos de las áreas de movimiento aliadas y se explora su interior para que no queden bolsas de niebla entre ellas al inicio. Esta pasada solo ocurre al construir la visión inicial, no cada frame. Los enemigos dentro de la zona siguen sin hacerse visibles.
- Todas las ocupaciones temporales se restauran antes de terminar el cálculo. El pathfinder real que valida los movimientos no cambia y sigue respetando la ocupación.

## Verificación
- `sh tests/run_tests.sh`: pasan las 23 comprobaciones.
- `sh tests/run_tests_sanitized.sh`: pasan las 23 comprobaciones con ASan/UBSan.
- Las pruebas de niebla comprueban que el hueco central queda explorado pero no visible, y que una zona inicialmente alcanzable tras un aliado no sigue apareciendo como no descubierta.
- En este entorno no se encontró una instalación de SGDK ni `m68k-elf-gcc`, así que no se generó una ROM ni se verificó visualmente con BlastEm. El contador y la posición se han integrado con la API SGDK y los tests de sintaxis del proyecto pasan, pero la comprobación final en emulador queda pendiente.
