# Fase 8 — escenarios, combate, IA y resolución de misión

## Estado de la base

La base ya separa el registro de escenarios (`scenario.c`), las reglas de turno
(`game_turn.c`), el combate (`combat.c`) y la planificación de la IA
(`enemy_ai.c`). Los escenarios se describen con recursos de mapa, mapa lógico,
despliegue y objetivo; `main.c` no necesita un bloque de lógica por misión.

## Cambio incorporado: límite opcional de rondas

`MissionObjective.turnLimit` permite limitar una misión sin añadir lógica
específica a cada escenario:

- `0`: sin límite; conserva el comportamiento anterior.
- `N > 0`: al terminar la ronda N (después de que Azul y Rojo hayan actuado),
  se resuelve la partida si ningún objetivo anterior la ha terminado.
- En una misión de mantener punto, el equipo que ocupa el objetivo gana el
  desempate.
- En los demás casos, gana el equipo con más unidades vivas; si empatan, la
  partida termina en empate.
- El resultado se identifica como `LIMITE DE TURNOS` y no se marca como objetivo
  completado. Los efectos `TURN_END` siguen despachándose una sola vez.

Los tres escenarios existentes mantienen `turnLimit = 0`, para no alterar sus
reglas ni la duración de las partidas actuales. Un escenario nuevo puede fijar
el límite desde su registro.

## Sistemas de juego que deben mantenerse como autoridad

- Movimiento y costes: `UnitController` + `Pathfinder` + perfil de terreno/clase.
- Ataque, contraataque, cobertura, línea de visión, reparación y bajas:
  `Combat_predict*()` / `Combat_attack()` / `Combat_repair()`.
- Objetivos y fin de partida: `GameTurn`.
- Roles, selección de objetivos, posicionamiento y decisiones de la IA:
  `EnemyAI`.
- Presentación y pantalla final: HUD y `GameScreens`, sin duplicar reglas.

## Validación

La suite host incorpora una prueba que comprueba que el límite no se evalúa tras
el turno de un solo equipo, que se resuelve al cerrar la ronda completa y que no
se permite cerrar el turno una segunda vez. La auditoría de host sigue sin
sustituir la compilación de ROM con SGDK ni la prueba en BlastEm.
