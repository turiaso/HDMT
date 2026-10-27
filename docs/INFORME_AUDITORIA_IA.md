# Informe de auditoría y refactorización de la IA

## Hallazgos y correcciones

1. **Contexto de turno incoherente.** `EnemyAI_update()` antes podía recibir un `activeTeam` distinto al equipo activo de `UnitController`, inicializar el plan y avanzar el índice de actor, aunque el controlador luego rechazase la orden. Ahora valida el contexto antes de modificar el estado. También exige que el mapa pasado sea el mismo que usa el controlador.
2. **Equipo implícito en estado global.** Se eliminó `currentAITeam`. Los cálculos de rol, objetivos, visibilidad, combate, reparación y puntuación reciben el `TeamId` explícitamente. `EnemyAI_getNextActorIndex()` ya no modifica contexto global al consultar el próximo actor.
3. **Contratos documentados.** Se documentó cuándo debe devolver `ENEMY_AI_IDLE`, la independencia del contexto del equipo y la separación entre la decisión táctica y las reglas autoritativas de movimiento/combate.
4. **Regresión añadida.** El test nuevo fuerza una llamada durante el turno de Azul dirigida a Rojo y otra con un objeto de mapa distinto. Ambas deben devolver `ENEMY_AI_IDLE` sin inicializar la IA, cambiar posiciones ni alterar la salud.

## Comportamiento que se conserva

- Selección de enemigos vivos visibles y descarte de blancos ocultos.
- Roles de corredor, escolta, guardia, pantalla, flanqueo y apoyo.
- Coordinación de fuego, selección de intercambios de combate, retirada táctica y reparación del ingeniero.
- Costes de terreno, pathfinding, gasto de acciones y animación gestionados por los subsistemas autoritativos.
- Cámara, niebla de guerra, sprites y precarga sin cambios intencionados.

## Verificación ejecutada

- `sh tests/run_tests.sh`: suite completa (23 grupos) y prueba de estrés de combate superadas.
- `sh tests/run_tests_sanitized.sh`: suite completa con AddressSanitizer y UndefinedBehaviorSanitizer superada.
- GCC `-Wall -Wextra -Werror -Wformat=2 -Wno-main -fanalyzer -fsyntax-only` sobre todos los `src/*.c`: sin diagnósticos.
- `tools/check_source_quality.sh`: verificación de calidad de fuente ejecutada y resultado registrado en la entrega.
- Integridad del ZIP: verificada.

## Límite

Las pruebas host no sustituyen la compilación real con SGDK ni la ejecución visual en BlastEm. No se afirma haber validado una ROM en esta iteración.
