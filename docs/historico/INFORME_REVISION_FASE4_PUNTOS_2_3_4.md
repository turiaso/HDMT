# Revisión de fase 4: costes/rutas, eventos y prueba de ROM

## Resultado

La batería existente pasaba antes de la revisión. Al auditar el contrato de los eventos encontré una discrepancia real entre documentación y comportamiento: `UnitController` vaciaba la casilla origen **antes** de invocar `CELL_EVENT_LEAVE`, aunque la arquitectura documentaba que el cambio de ocupación ocurre entre `leave` y `enter`.

La transición quedó con esta semántica:

1. `beforeLeave`: la celda de origen sigue ocupada; el destino está libre.
2. `leave`: el origen sigue ocupado durante el callback.
3. Se libera el origen.
4. `beforeEnter`: origen y destino están libres.
5. Se ocupa el destino y se actualizan `unit.x`/`unit.y`.
6. `enter`: el destino ya está ocupado; el origen está libre.
7. `unitLeave` y `unitEnter`: notificaciones específicas posteriores a la transición.

Se añadieron aserciones que capturan la ocupación vista por cada callback. Los pasos rechazados continúan sin eventos de transición.

## Costes y límites de ruta revisados

Se añadieron pruebas que comprueban:

- el coste se cobra al entrar en terreno, no por la casilla inicial;
- camino cuesta 1, bosque 2 y piedras 2;
- el presupuesto exacto se acepta, pero quedarse un punto por debajo se rechaza;
- las rutas no se envuelven de una fila a otra en los bordes;
- los fallos dejan `stepCount` y `totalCost` en cero;
- se conservan las pruebas anteriores de agua impasable, ocupación, coordenadas y capacidad de workspace.

El algoritmo actual de Dijkstra y su reconstrucción no necesitaban cambios: las pruebas de regresión pasan con su implementación actual.

## Eventos

Los contextos siguen incluyendo mapa, celda, origen (`SYSTEM`, `CURSOR` o `UNIT`), coordenadas y la coordenada opuesta. Los cuatro eventos genéricos de transición siguen el orden documentado, y `unitLeave`/`unitEnter` llegan después de actualizar el estado lógico.

Los callbacks siguen siendo notificaciones y no pueden vetar una transición ni mutar terreno/ocupación durante el mismo tránsito. Para reglas futuras que puedan bloquear movimiento se necesitaría una fase de validación con resultado explícito, separada de los eventos de notificación.

## Verificación realizada

- `sh tests/run_tests.sh`: todas las 11 secciones pasan con GCC, incluidos ASan/UBSan si está disponible.
- Prueba funcional del sistema de eventos, prueba de movimiento y snapshots de ocupación durante cada callback.
- Costes acumulados de bosque/camino/piedras y límites exactos del presupuesto.
- Las comprobaciones cubren lógica host y sintaxis con stubs; no equivalen a compilar el juego con SGDK.

## Verificación de ROM pendiente

En este entorno no está configurada la variable `GDK` y no se localizó `makefile.gen` de SGDK. Por eso no he podido producir una ROM real ni probarla en BlastEm aquí. La confirmación final exige compilar con la instalación SGDK de Windows del proyecto y ejecutar una ROM, revisando movimientos por varios terrenos, casillas ocupadas, borde del mapa y efectos de eventos. No doy esa prueba por realizada.
