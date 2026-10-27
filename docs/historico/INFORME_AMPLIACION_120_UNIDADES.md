# Ampliación del motor a 120 unidades lógicas

## Resultado

El escenario **Punto de control** despliega ahora 120 unidades lógicas: 60 azules y 60 rojas. El escenario **Defensa del puente** se conserva con 12 unidades por bando. El límite lógico del motor se fija en `GAME_MAX_UNITS = 128`, dejando margen para futuras configuraciones sin exceder el rango válido de IDs.

## Despliegue

- Azul: IDs 0–59 en el cuadrante noroeste.
- Rojo: IDs 60–119 en el cuadrante sureste.
- Cada equipo tiene 16 unidades de infantería, 16 exploradores, 14 ingenieros y 14 blindados.
- Todas las posiciones iniciales son únicas, transitables y distintas de la casilla objetivo.
- Las pruebas validan IDs, equipos, tipos, posiciones, ocupación inicial y capacidad del controlador.

## Límites del hardware y representación visual

El aumento de unidades lógicas no implica que la Mega Drive pueda dibujar 120 sprites de unidad simultáneamente. La SAT admite un máximo de 80 entradas en el modo del proyecto; el motor reserva tres para cursor, marcador de objetivo y reserva del tile de niebla. Por ello, `UnitView` mantiene hasta **77 sprites de unidad activos** y una tabla independiente para las 120 unidades lógicas.

Cuando haya más de 77 unidades visibles dentro del área de juego, las instancias de sprite se asignan por prioridad: unidad en movimiento, unidad seleccionada, unidades del equipo observador/activo y, después, las restantes. Las unidades sin sprite asignado no desaparecen del juego: conservan posición, ocupación, salud, turno, IA y visibilidad lógica, y pueden volver a dibujarse cuando se libera un hueco. El límite independiente de sprites por línea sigue aplicándose y requiere comprobación visual en BlastEm o hardware.

## Sistemas ampliados

- `UnitController`: admite hasta 128 registros y mantiene IDs de unidad compatibles con la ocupación firmada de la celda (0–127, con -1 reservado para casilla vacía).
- `FogOfWar`: amplía las instantáneas, listas por equipo y máscaras de recálculo a `GAME_MAX_UNITS`; el mapa sigue limitado a 1.600 celdas.
- `EnemyAI`: tablas de roles y selección admiten el límite lógico de 128 unidades.
- `UnitView`: cachés por unidad separadas del contador de sprites hardware, con reasignación cuando cambia la visibilidad o se libera una entrada.
- `main.c`: la tabla de referencias de sprite tiene una entrada por unidad lógica y los asserts de compilación separan el máximo lógico del presupuesto SAT.

No se han implementado las optimizaciones de RAM aplazadas A/B/C ni cambiado el sistema compartido de eventos por casilla.

## Pruebas realizadas

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: los módulos C de `src/` compilan con los stubs host sin diagnósticos.
- La prueba de vista crea 120 unidades lógicas con 100 candidatas dentro del viewport y comprueba que el número de sprites activos no supera 77, que el presupuesto total no supera 80 y que la prioridad visual cambia al cambiar el equipo observador.
- `scenario_test.c` valida 60 unidades por bando y la distribución de tipos 16/16/14/14.
- `ai_full_turn_test.c` ejecuta un turno de IA con 120 unidades y niebla activa.
- La suite conserva las pruebas de línea de visión de combate, niebla, movimiento, eventos, misiones y 20.000 mapas aleatorios de pathfinding.

## Pendiente de validar en el toolchain real

No se ha compilado la ROM con SGDK real ni ejecutado BlastEm en este entorno. Antes de dar la ampliación por cerrada en hardware, hay que comprobar la memoria estática final que informa el linker, la SAT y el límite de sprites por línea, así como el FPS durante el turno de Rojo con 60 unidades. Las pruebas host verifican lógica e invariantes, pero no sustituyen esas medidas reales.
