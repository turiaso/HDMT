# Informe: coordinación táctica, posicionamiento, niebla y estrés de 120 unidades

## Alcance

Continuación de la auditoría de la IA roja sobre el proyecto SGDK de 40x40 y 120 unidades. Se preservan el movimiento animado, el pathfinding autoritativo, los costes de terreno, los eventos compartidos por celda y las decisiones aplazadas de optimización de RAM.

## 1. Concentración de fuego y eliminación eficiente

`findBestAttackTarget()` sigue usando `Combat_predictUnits()` como fuente autoritativa para daño, contraataque y derrota. El daño predicho está limitado a la salud restante del objetivo. La bonificación adicional de foco (+32) ahora solo se aplica si el objetivo sobrevive al ataque previsto; si el ataque ya lo elimina, la bonificación de eliminación (+100) ya expresa esa prioridad y no se duplica el incentivo.

La coordinación mantiene el comportamiento probado de completar eliminaciones en varios ataques y continúa descartando intercambios donde una represalia letal eliminaría al atacante.

## 2. Posicionamiento coordinado por clase y rol

Se conservan y auditan las preferencias existentes: infantería e ingenieros valoran cobertura defensiva, exploradores pueden aprovechar bosque, blindados favorecen caminos y penalizan roca/bosque, los apoyos buscan posiciones de tiro separadas y línea de fuego, los guardias protegen el objetivo y los escoltas mantienen distancia útil del corredor. Estas puntuaciones solo eligen entre destinos alcanzables; no sustituyen al pathfinder ni cambian la legalidad del movimiento.

La suite cubre cobertura de guardia, preferencia de camino del blindado, posiciones laterales de apoyo, apertura de línea de tiro y alcance propio de cada clase.

## 3. Amenazas basadas en información visible

La IA construye una lista de enemigos vivos visibles y la reutiliza para ataques, elección de blanco, riesgo y posicionamiento. El foco heredado se invalida si el objetivo muere o deja de estar visible. La regresión de niebla comprueba que un enemigo oculto no sea seleccionado ni atacado y que la IA no termine ocupando su casilla.

La niebla explorada y la visibilidad de combate siguen siendo conceptos distintos: la memoria del terreno no autoriza atacar a unidades que no están actualmente visibles.

## 4. Estrés e integridad espacial con 120 unidades

La simulación de turno completo mantiene 120 registros lógicos (60 por equipo), espera a que finalicen las animaciones y comprueba que el turno rojo termina. Se añadió una auditoría posterior que verifica para cada unidad viva que la casilla existe, que su ocupante coincide con su ID y que ninguna pareja de unidades vivas comparte coordenadas.

Resultado actual de la simulación: 1492 iteraciones, 57 movimientos y 0 ataques. Por tanto, valida el ciclo de turno y la integridad espacial bajo 120 unidades, pero no se presenta como una prueba de estrés de combate simultáneo; los ataques y el combate están cubiertos por los tests tácticos específicos.

## Validación ejecutada

- `sh tests/run_tests.sh`: 23 grupos y prueba adicional de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23 grupos y prueba adicional superados con ASan/UBSan.
- GCC `-Wall -Wextra -Werror -Wformat=2 -fanalyzer -fsyntax-only` sobre todos los `src/*.c`: superado.
- La prueba de pathfinding continúa cubriendo 20.000 mapas aleatorios.

## Límite de validación

No se compiló una ROM con una instalación real de SGDK ni se ejecutó BlastEm en este entorno. La compilación final y la comprobación visual en emulador quedan pendientes de ejecutarse en el entorno SGDK del proyecto.
