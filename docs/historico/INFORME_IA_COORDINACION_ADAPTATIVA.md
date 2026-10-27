# IA roja: concentración de fuego y adaptación al objetivo

## Cambios

### 1. Concentración de fuego

`EnemyAI` mantiene `focusTargetIndex`, un índice de unidad (no un puntero persistente) que marca el enemigo prioritario visible del equipo Rojo.

- Cuando una unidad elige un blanco visible para aproximarse y no hay un foco válido, lo registra para que otras unidades también puedan aproximarse.
- Al ejecutar un ataque, si el defensor sobrevive, pasa a ser el foco coordinado; si queda eliminado, se libera el foco si correspondía a esa unidad.
- Los siguientes atacantes añaden una bonificación de puntuación al blanco enfocado, para completar el daño iniciado por otra unidad.
- Una represalia prevista que eliminaría al atacante mantiene su penalización; el foco no fuerza un ataque suicida.
- Los corredores mantienen la prioridad del objetivo de misión. Los guardias solo se apartan de la misión si el blanco está cerca de la zona defensiva.
- Si el blanco muere, deja de pertenecer al equipo rival o ya no es visible para Rojo, el foco se invalida.

El foco se mantiene por índices de unidades existentes; no reserva una tabla de objetivos por casilla ni un mapa de influencia.

### 2. Adaptación cuando cambia el dueño del objetivo

`EnemyAI.lastObjectiveOwner` guarda el propietario observado en la última decisión. Después de la animación de movimiento, la siguiente llamada a `EnemyAI_update()` vuelve a consultar la ocupación de la casilla. Si el dueño cambió, recalcula los roles tácticos de inmediato.

Esto permite que un corredor/escolta que acaba de tomar un punto de `MISSION_HOLD_POINT` pase a formar parte de la defensa y del perímetro durante el mismo turno. La reasignación no se ejecuta dentro de un fotograma de movimiento y no modifica los estados de visión ni consulta posiciones enemigas ocultas.

### 3. Memoria y coste

El estado de coordinación mantiene dos bytes para el índice de foco, un byte para el propietario registrado y ahora un byte para el plan global (además del relleno/alineación que aplique el compilador). La tabla de roles sigue ocupando 60 bytes. El barrido de enemigos visibles continúa realizándose una vez por decisión de movimiento, no por fotograma, y el área de destinos sigue limitada al presupuesto de movimiento.

## Pruebas añadidas

En `tests/enemy_ai_test.c`:

1. **Foco compartido:** el tanque rojo golpea un tanque azul que sobrevive; la infantería roja, que preferiría atacar a un ingeniero cercano, concentra su siguiente ataque en el tanque ya dañado.
2. **Cambio de propietario:** el explorador rojo comienza como corredor de un punto de mantener, ocupa la casilla y se reasignan los roles para que pase a guardia durante el mismo turno.

## Validación ejecutada

- `sh tests/run_tests.sh`: 23/23 grupos superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos superados con ASan/UBSan.
- GCC `-fanalyzer -Wall -Wextra -Werror -Wformat=2`: los 19 módulos C analizados sin diagnósticos.
- Se conservan las pruebas de 20.000 mapas aleatorios y la simulación completa del turno de IA con 120 unidades lógicas (60 por equipo).

Las pruebas host usan stubs de SGDK. No equivalen a una compilación real de ROM ni a una medición de FPS en BlastEm; la integración en tu instalación de SGDK sigue pendiente de validación externa.


La capa de maniobras de grupo está descrita en `INFORME_MANIOBRAS_GRUPO.md`: los planes distinguen asalto/defensa/eliminación/supervivencia, guardias y pantallas ocupan sectores diferenciados y las unidades de apoyo eligen una posición lateral de tiro sin recorrer todos los aliados por cada casilla candidata.
