# Informe: auditoría de movimiento y turnos

## Cambios

- La ruta completada sigue consumiendo los puntos restantes del turno, aunque el coste real haya sido inferior al presupuesto inicial.
- Si la ruta se interrumpe después de que la unidad haya confirmado una o más celdas nuevas, también se consume la acción de movimiento. Así la unidad no puede iniciar otra ruta en el mismo turno tras quedar bloqueada a mitad del recorrido.
- Si el bloqueo ocurre antes de que la unidad abandone su origen lógico, conserva los puntos, reconstruye el alcance y puede planificar otra ruta sin reseleccionar. La interpolación parcial se devuelve al centro de la última celda confirmada.
- `UnitController_beginTeamTurn()` rechaza volver a inicializar el mismo equipo y el mismo número de turno, evitando recargar puntos y reiniciar acciones de combate/reparación por una llamada duplicada. Los turnos nuevos siguen restaurando el presupuesto del equipo activo y dejan a cero el presupuesto del equipo inactivo.

## Pruebas añadidas

- Bloqueo de una ruta de varios pasos después de confirmar una celda: la unidad se centra correctamente, sus puntos pasan a cero y otra orden es rechazada.
- Bloqueo de la primera celda antes de confirmarla: conserva el presupuesto, centra el sprite en el origen y permite elegir una ruta alternativa.
- Repetición de `UnitController_beginTeamTurn()` para el mismo equipo/turno, incluso después de activar al rival: no restaura el movimiento consumido.
- Cambio al equipo rival y posterior retorno: se restauran los puntos solo al llegar el nuevo turno propio.
- Prueba de integración `GameTurn`: una unidad no encadena una segunda orden, espera a la rotación de turnos y vuelve a poder mover en su siguiente turno.

## Validación

- `sh tests/run_tests.sh`: las 23 suites y pruebas adicionales pasan.
- `sh tests/run_tests_sanitized.sh`: las suites con ASan/UBSan pasan.
- GCC `-fanalyzer`: todos los módulos C pasan.

La validación host confirma las reglas lógicas y la integridad de memoria dentro de estas pruebas. Esta entrega no incluye verificación visual en BlastEm ni compilación con el SGDK real.
