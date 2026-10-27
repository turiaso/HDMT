# Fase 4 — revisión de eventos y refactorización

## Resultado

Se ha revisado el ciclo de eventos al mover unidades y se ha limpiado el controlador de movimiento. La API de eventos diferencia ahora el origen (`SYSTEM`, `CURSOR`, `UNIT`) sin duplicar las tablas de perfiles compartidos. Los eventos de cursor conservan su comportamiento y los movimientos de unidad notifican de forma explícita su origen.

## Ciclo de una transición de unidad

Cada tramo se valida antes de empezar y se vuelve a validar al alcanzar el centro de la casilla. Para un paso válido, el orden queda fijado así:

1. `beforeLeave` en el origen, con la unidad todavía ocupándolo.
2. Se libera el origen.
3. `leave` en el origen.
4. `beforeEnter` en el destino, aún libre.
5. Se ocupa el destino y se actualizan las coordenadas lógicas de la unidad.
6. `enter` en el destino.
7. `unitLeave` en el origen y `unitEnter` en el destino como notificaciones específicas adicionales.

Todos los eventos de ese ciclo usan `CellEventContext.source = CELL_EVENT_SOURCE_UNIT`. Un tramo rechazado por validación no emite eventos de salida o entrada. Los callbacks son notificaciones síncronas, no una API para vetar el movimiento; no deben modificar el terreno ni la ocupación dentro de la misma transición. Si más adelante se necesitan decisiones que puedan cancelar una orden, habrá que diseñar explícitamente un resultado de validación previo al commit.

## Refactorización aplicada

- Se unificó la resolución del alcance de movimiento por tipo/instancia.
- Se centralizó la limpieza del estado de planificación y de una ruta detenida.
- Se sustituyó el cálculo de casillas vecinas con aritmética sin signo por una función con límites y direcciones verificadas.
- Se separó el refresco pasivo del HUD de la actualización de hover. Durante la animación, refrescar la ocupación del HUD ya no genera eventos `stay` por fotograma.
- Se redujo la duplicación en el controlador y se añadieron comentarios sobre los contratos de los callbacks.
- Se mantiene intacta la inicialización de la fuente SGDK y la asignación de VRAM del HUD.

## Cobertura verificada

Ejecutado con GCC, sin Clang:

- `sh tests/run_tests.sh`: las 11 secciones terminan correctamente.
- Compilación y ejecución del autotest de eventos con `-Wall -Wextra -Werror`.
- Prueba del controlador con `-fanalyzer`: sin diagnósticos y ejecución correcta.
- La traza de unidad comprueba tipo, origen, coordenadas y orden exacto de eventos en cada paso.
- Se comprueba que una casilla bloqueada antes del siguiente tramo detiene el movimiento centrado, conserva la ocupación correcta y no emite eventos para el paso rechazado.
- Se comprueba que el refresco del HUD actualiza la ocupación sin disparar `stay`.

## Límites de estas comprobaciones

Las pruebas host emplean los stubs SGDK del proyecto; no sustituyen una compilación con la instalación real de SGDK ni una inspección de la ROM en BlastEm o en hardware. No se ha ejecutado Clang. Tampoco se ha modificado ni dado por resuelto el problema gráfico anterior de glifos/bloques blancos del HUD.
