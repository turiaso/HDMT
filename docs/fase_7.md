# Edición completa · motor y juego contra IA

Este documento registra la edición que integró el pathfinder optimizado y el rival controlado por la máquina. Para editar la versión actual, empieza por [`guia_desarrollo.md`](guia_desarrollo.md) y [`arquitectura.md`](arquitectura.md). El informe de aquella edición está en [`historico/INFORME_EDICION_COMPLETA.md`](historico/INFORME_EDICION_COMPLETA.md).

## Cómo elegir una misión

La misión se define dentro de cada `ScenarioDefinition` en `src/scenario.c`. Al arrancar, el selector muestra las misiones registradas: usa arriba/abajo para elegir y pulsa A o START para comenzar. `ACTIVE_MAP_INDEX` en `src/game_config.h` determina únicamente la selección resaltada por defecto. El motor admite:

- `MISSION_CAPTURE_POINT`: captura inmediata.
- `MISSION_HOLD_POINT`: mantener punto durante `requiredTurns` finales de turno.
- `MISSION_ELIMINATE_ALL`: destruir todas las unidades enemigas.
- `MISSION_SURVIVE_TURNS`: sobrevivir `requiredTurns` turnos propios del `targetTeam`.

Para una misión temporal, define `requiredTurns` en un valor mayor que cero y selecciona el `targetTeam` adecuado para `MISSION_SURVIVE_TURNS`. Los escenarios 0, 1 y 2 tienen reglas explícitas: el primero captura el centro de forma inmediata; los dos escenarios de puente requieren mantener su objetivo durante tres finales de turno propios.

## Diagnosticar rendimiento

Los contadores `PERF PHASE ...` son opcionales y `PERF_DIAGNOSTICS` está desactivado por defecto en `src/game_config.h`. Actívalos temporalmente con `-DPERF_DIAGNOSTICS=1` en una build de diagnóstico. Tras acumular 120 fotogramas de una actividad, los bloques registran scroll, actualizaciones SAT, celdas de niebla, transferencias gráficas, rutas/nodos y trabajo de IA. El overlay `FPS: NN` muestra el FPS estimado por SGDK; los bloques `PERF` comparan cantidades de trabajo, no ciclos del 68000. El procedimiento de diagnóstico original está en [`historico/INFORME_DIAGNOSTICO_RENDIMIENTO_120_UNIDADES.md`](historico/INFORME_DIAGNOSTICO_RENDIMIENTO_120_UNIDADES.md).

## IA y limitaciones

El turno Rojo se ejecuta automáticamente. La IA recibe el `MissionObjective` completo y asigna roles compactos a sus unidades: corredor y escolta hacia el objetivo, hasta dos guardias cuando controla un punto de mantener, pantalla en el perímetro, exploradores flanqueadores y apoyo de fuego. En eliminación prioriza enemigos visibles y ataques valiosos; en supervivencia defiende o ataca según qué equipo deba sobrevivir. Las unidades con vida crítica pueden buscar cobertura y retirarse de una amenaza letal.

Antes de atacar, predice el daño, una posible baja inmediata y la represalia. Suele priorizar bajas seguras y evita intercambios que eliminarían al atacante sin compensación. Para moverse combina progreso estratégico, amenaza de enemigos actualmente visibles y cobertura de terreno; favorece las piedras y el bosque si ayudan a reducir el daño. Nunca consulta las posiciones de unidades ocultas para elegir blancos o evaluar amenazas.

La coordinación ahora puede conservar un foco de fuego compartido para que varias unidades ataquen a un rival prioritario que aún sobrevive, pero esa bonificación no supera una represalia letal. El blanco se descarta cuando muere o deja de ser visible. Si cambia la propiedad del objetivo durante el turno, la IA recalcula los roles en la siguiente decisión; por ejemplo, un corredor de captura se convierte en guardia de un punto de mantener al ocuparlo. Los casos nuevos están en `tests/enemy_ai_test.c`.

Es una coordinación táctica ligera, no una búsqueda táctica profunda de varios turnos: no simula todas las posibles respuestas del jugador. La visión y el combate sí respetan la línea de visión actual; la IA no simula rayos de fuego hipotéticos sobre todos los futuros estados. Las unidades comparten roles por turno y coordinan sus prioridades sin reservar un mapa de influencia adicional. La lógica de juego y movimiento continúa compartida con el controlador humano. El informe histórico de esa heurística está en [`historico/INFORME_IA_TACTICA.md`](historico/INFORME_IA_TACTICA.md).

## Escenarios configurables

Consulta [`escenarios.md`](escenarios.md) para el selector, los mapas, despliegues, objetivos y el procedimiento para añadir otra misión. Los tres descriptores de `MapData` comparten los buffers de RAM y se inicializa solo el seleccionado.

## Ciclo completo de partida

La partida ahora cierra con una pantalla de resultado que muestra el equipo vencedor, el objetivo y el turno final. **A** repite la misión seleccionada; **B** o **START** regresa al selector de escenarios. En cada transición se libera el `Map` creado por SGDK (`MAP_release`) y se termina el motor de sprites (`SPR_end`); la siguiente partida reinicializa el estado lógico, la niebla, el HUD, el controlador de unidades y las cachés de presentación. El helper puro `mission_result.c` resuelve la selección y dispone de pruebas propias.
