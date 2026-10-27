# Corrección de niebla inicial: contorno de tropas y área de movimiento

## Cambio

Durante la inicialización de la niebla, cada unidad viva del equipo activo aporta dos áreas a la visión inicial:

1. Su radio de visión, respetando la línea de visión y los obstáculos.
2. Las casillas que realmente puede alcanzar con sus puntos de movimiento actuales, calculadas mediante el pathfinder ponderado del juego y también filtradas por línea de visión.

La unión de ambas áreas elimina los parches de niebla que quedaban dentro de la zona inicial accesible al grupo, sin convertir el mapa completo en visible ni revelar casillas detrás de obstáculos.

Se mantiene la visión independiente por equipo y el estado de terreno explorado. La modificación se limita a la construcción de la visión inicial; no cambia la lógica de movimiento ni la niebla persistente.

## Archivos modificados

- `src/fog_of_war.c`: incorpora el despeje inicial del área de movimiento alcanzable.
- `src/fog_of_war.h`: amplía el snapshot para registrar identidad y puntos de movimiento de cada unidad.

## Validación

- `sh tests/run_tests.sh`: las 23 comprobaciones disponibles pasan.
- Incluye pruebas de niebla por equipo, oclusión, exploración persistente, movimiento, pathfinding y simulación de turno de IA.
- La compilación SGDK real y la comprobación visual en BlastEm siguen pendientes.
