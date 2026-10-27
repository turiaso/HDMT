# Ampliación del escenario · 40 × 40 y 120 unidades

## Cambios realizados

- El mapa lógico pasa a 40 × 40 celdas (1.600 celdas). El escenario gráfico mide 968 × 968 px: 960 px para las 40 × 40 celdas de 24 px y 8 px de margen técnico para el scroll.
- El terreno se compone de cuatro repeticiones del cuadrante original, conectadas mediante un cruce central de caminos de dos celdas de anchura. El objetivo de captura está en la celda lógica `(20,20)`.
- El despliegue inicial ahora contiene 60 unidades por equipo, 120 en total. Cada bando tiene 16 infanterías, 16 exploradores, 14 ingenieros y 14 blindados. Azul comienza en el cuadrante noroeste y Rojo en el sureste.
- Los cuatro recursos de unidad se reducen a PNG indexados de 16 × 16 px, con índice 0 transparente e índices de color 3 y 4 para conservar las paletas por equipo. En `res/resources.res` se declaran como sprites de 2 × 2 tiles y se centran en la celda lógica de 24 × 24 px.
- Se actualizan las vistas previas del mapa y las unidades, la documentación de arquitectura y el README.
- Se actualizan los lanzadores de pruebas Linux y Windows; ambos incluyen una prueba de escenario que valida dimensiones, despliegue, IDs, posiciones, terreno, mezcla de unidades y presupuesto de sprites.

## Memoria del mapa

`CellData` reserva 4 bytes por celda: 6.400 bytes para 1.600 celdas. Los tres búferes estáticos de pathfinding reservan 4.800 bytes y la cola circular optimizada otros 3.200 bytes. Mapa, workspace y cola suman 14.400 bytes (14,06 KiB), antes de contar las unidades, controladores, pila y resto de la aplicación. No se han aplicado las optimizaciones de RAM A/B/C que se habían aplazado.

## Presupuesto de sprites

En la edición completa las máscaras de los bordes se dibujan con tiles estáticos del plano BG_B y no consumen entradas de sprite hardware. Con la ampliación a 120 unidades lógicas, la vista crea como máximo 77 sprites de unidad simultáneos y reserva tres entradas SAT para cursor, marcador de objetivo y niebla. Si hay más unidades visibles que huecos, la vista prioriza la unidad en movimiento, la seleccionada y las del equipo activo; las restantes se conservan en el estado lógico y pueden volver a dibujarse al liberarse un hueco. El límite de 80 entradas SAT y el límite independiente por línea siguen siendo restricciones de hardware.

## Verificación realizada

- `sh tests/run_tests.sh`: pasan los 23 grupos del lanzador actualizado.
- GCC con `-Wall -Wextra -Werror -fanalyzer`: todos los módulos C de `src/` compilan sin diagnósticos con los stubs de host.
- Validación de recursos: 40 filas lógicas de 40 celdas, mapa gráfico de 968 × 968, cuatro sprites indexados de 16 × 16 y recurso de niebla indexado de 8 × 8; las declaraciones de unidades usan 2 × 2 tiles.
- Enlaces locales de Markdown verificados; no se encontraron espacios finales ni líneas largas en los módulos C/H.
- El lanzador `tests/run_tests.bat` está actualizado para incluir `scenario.c` y `scenario_test.c` y reportar el escenario de 120 unidades.

## Pendiente

No se ha compilado la ROM con la instalación real de SGDK ni se ha probado visualmente en BlastEm desde este entorno. Debe comprobarse el scroll del mapa de 40 × 40, la carga de recursos, las paletas y el límite de sprites por línea en la ROM real. Las pruebas host no sustituyen esa validación.
