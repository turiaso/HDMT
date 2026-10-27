# Línea de visión de combate · auditoría y maniobra de apertura de tiro

## Punto de partida

La versión base ya contiene una función compartida `MapData_hasLineOfSight()` usada por niebla, combate, predicción de daño/represalia, HUD y evaluación táctica de la IA. Esta entrega no duplica el algoritmo ni cambia los costes, alcances o fórmulas de daño. Completa el comportamiento de la IA cuando un objetivo visible está dentro del alcance geométrico pero el tiro queda tapado.

## Cambio funcional

En `src/enemy_ai.c`, durante la selección de destino (no durante la animación ni en el bucle por fotograma), se detecta el caso de una unidad que tiene un blanco visible dentro de su alcance de ataque pero carece de línea despejada. Las casillas alcanzables que mantienen al blanco dentro de alcance y sí despejan la línea de fuego reciben una bonificación táctica. La evaluación de riesgo y cobertura del destino sigue aplicándose después, por lo que la IA no deja de considerar amenazas para conseguir un tiro.

En su siguiente decisión, tras completar el desplazamiento, el sistema habitual `Combat_predictUnits()` vuelve a validar alcance, visibilidad y línea de visión antes de ejecutar el ataque. La bonificación no autoriza disparos por sí misma.

## Regla de esquinas unificada

La documentación anterior decía que las dos celdas laterales tocadas por un rayo diagonal exacto bloqueaban la visión, pero el código las trataba como celdas rozadas por el borde, que no bloquean. Se conserva la implementación ya usada por la niebla para evitar introducir una modificación táctica/fog sin necesidad, y se corrige la documentación para describir la regla real:

- Bosque y piedras bloquean cuando son celdas intermedias atravesadas por el recorrido.
- La propia casilla objetivo puede ser bosque o piedras; el terreno del objetivo aporta cobertura defensiva, pero no bloquea el tiro hacia esa casilla.
- Al pasar exactamente por una esquina, el recorrido avanza en diagonal y las dos celdas laterales que solo toca por el borde no bloquean. La celda diagonal intermedia sí se comprueba.
- La regla es simétrica y se comparte entre visión táctica y disparos.

## Pruebas añadidas o ampliadas

- `tests/map_data_test.c`: comprueba de forma explícita el paso diagonal cuando el rayo solo roza dos rocas laterales, y el bloqueo cuando la celda diagonal intermedia es opaca. Las comprobaciones inversas aseguran simetría.
- `tests/combat_test.c`: cubre el bloqueo por roca y bosque, y confirma que un objetivo situado en bosque sigue siendo atacable si la línea está despejada y recibe la cobertura correspondiente.
- `tests/enemy_ai_test.c`: coloca un blanco visible dentro del alcance detrás de una roca; la IA no intenta disparar a través de ella, se mueve a una celda alcanzable con línea despejada y ataca en su siguiente decisión.

## Validación

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer` compila los módulos C sin diagnósticos usando los stubs SGDK del proyecto.

Estas pruebas se ejecutan en host y no sustituyen la compilación cruzada real. No se ha generado una nueva ROM ni comprobado visualmente en BlastEm en este entorno. La regla de combate no añade trabajo al bucle de animación; el cálculo táctico adicional solo se realiza al decidir el destino de una unidad de IA.
