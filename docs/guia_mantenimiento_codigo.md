# Guía de mantenimiento del código

Esta guía describe dónde hacer cada cambio para evitar duplicar reglas o introducir
regresiones en cámara, niebla, movimiento o presentación. Está pensada para editar el
proyecto de forma incremental y conservar el presupuesto de RAM de Mega Drive.

## Mapa de responsabilidades

| Módulo | Responsabilidad | No debe asumir |
|---|---|---|
| `main.c` | Bucle principal, coordinación de estados y ciclo de vida de la partida | Reglas de combate, pathfinding o decisiones de IA |
| `scenario.c/.h` | Registro de misiones, recursos, despliegues y objetivos | Estado mutable de una partida concreta |
| `map_data.c/.h` | Dimensiones, terreno, ocupación y visibilidad lógica | Sprites o desplazamiento de cámara |
| `unit_controller.c/.h` | Unidades, órdenes, rutas aplicadas, puntos de movimiento y ocupación | Dibujo de unidades |
| `pathfinder.c/.h` | Costes y búsqueda de rutas/áreas alcanzables | Mover unidades o actualizar sprites |
| `combat.c/.h` | Predicción y aplicación coherente de daño y contraataque | HUD o animación |
| `game_turn.c/.h` | Equipo activo, economía de acciones y resultado de misión | Presentación de mensajes |
| `enemy_ai.c/.h` | Evaluación táctica y selección de una acción | Avanzar animaciones o controlar el bucle principal |
| `cell_events.c/.h` | Despacho de eventos de celda y turno | Duplicar reglas que ya pertenecen a otro sistema |
| `camera_cursor.c/.h` | Conversión entre coordenadas lógicas y pantalla | Cambiar ocupación lógica |
| `unit_view.c/.h` | Ciclo de vida, visibilidad y sincronización de sprites | Ser la fuente de verdad de la posición de una unidad |
| `fog_of_war.c/.h` | Estado y actualización incremental de niebla | Recalcularla innecesariamente en cada fotograma |
| `movement_glow.c/.h` | Resaltado visual del área alcanzable | Recalcular rutas al mover el cursor |
| `cell_hud.c/.h` | Inspección de casilla y feedback temporal | Cambiar el estado del juego |
| `performance.c/.h` | Contadores y diagnóstico de rendimiento | Afectar las reglas del juego |

## Invariantes que deben conservarse

1. **Una sola fuente de verdad:** la posición lógica y `occupantId` se mantienen sincronizados por
   `UnitController`; los sprites reflejan ese estado.
2. **Movimiento separado de presentación:** calcular el área alcanzable no mueve unidades.
   La ruta definitiva se busca al confirmar el destino, no en cada movimiento del cursor.
3. **Combate consistente:** el HUD y la IA deben usar la misma predicción que aplica el combate.
4. **Turnos idempotentes:** un callback de final de turno y el cierre de misión no deben ejecutarse
   dos veces por reintentos de actualización.
5. **Cámara y niebla incrementales:** no volver a convertirlos en actualizaciones completas por
   fotograma sin medición y una prueba que demuestre la necesidad.
6. **RAM limitada:** no añadir copias permanentes de mapas, rutas o unidades por escenario. Reutilizar
   los buffers compartidos cuando solo hay una misión activa.
7. **Orden de liberación:** invalidar referencias de presentación antes de liberar mapa y sprites.
   La prueba `tests/session_lifecycle_test.sh` protege este orden.
8. **Tipos SGDK:** incluir primero la cabecera del módulo que trae los tipos SGDK antes de
   `<string.h>`; la implementación SGDK puede depender de `u16` y `s8`.

## Cómo validar cada cambio

Desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
sh tests/session_lifecycle_test.sh
sh tools/check_source_quality.sh
sh tools/audit_host.sh
```

`tests/run_tests.sh` y `tests/run_tests_sanitized.sh` compilan sus ejecutables en un directorio
temporal privado, que se elimina al terminar. Esto evita colisiones con binarios antiguos y
permite ejecutar varias copias de la suite a la vez.

En entornos restringidos, ASan puede no arrancar si el proceso no puede reservar memoria shadow
o si el entorno inyecta una biblioteca mediante `LD_PRELOAD`. En ese caso, registra el error como
limitación del entorno; no lo presentes como un fallo funcional ni como una prueba superada.
El análisis estático GCC es una comprobación independiente, no sustituye la ejecución de la ROM.

La compilación real requiere configurar `GDK` para la instalación SGDK y probar el resultado en
un emulador compatible. La suite de escritorio no certifica VBlank, VRAM, temporización ni
comportamiento físico del VDP.

## Método recomendado para futuras modificaciones

- Cambiar una responsabilidad por vez y mantener pequeñas las funciones nuevas.
- Nombrar constantes según su propósito, no por el valor que tienen hoy.
- Documentar el motivo de una restricción o el orden de operaciones; no comentar cada línea obvia.
- Añadir una prueba de regresión para cada bug reproducible.
- Ejecutar las pruebas funcionales tras cada bloque; luego sanitizadores y análisis estático.
- No tocar cámara, niebla ni precarga de sprites como efecto colateral de una refactorización.
- No afirmar que la ROM está validada hasta compilarla con SGDK y ejecutarla.
