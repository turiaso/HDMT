# Entrega: estadísticas por clase y reparación del ingeniero

## Objetivo

Dar al ingeniero una función de apoyo real y mantener todas las estadísticas de clase centralizadas, sin introducir búsquedas ni trabajo nuevo por fotograma.

## Estadísticas vigentes

Los perfiles viven en `src/unit_types.c` y se copian al desplegar cada unidad:

| Clase | Mov. | PV | Ataque | Defensa | Alcance de ataque | Visión | Alcance de reparación | Reparación |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Infantería | 6 | 10 | 4 | 2 | 1 | 3 | — | — |
| Explorador | 8 | 7 | 3 | 1 | 1 | 4 | — | — |
| Ingeniero | 5 | 8 | 2 | 2 | 1 | 2 | 1 | 3 PV |
| Blindado | 7 | 14 | 6 | 3 | 2 | 3 | — | — |

## Reglas de reparación

- Solo el ingeniero puede reparar, a distancia Manhattan 1.
- Recupera hasta 3 PV, sin superar `maxHealth`.
- No puede repararse a sí mismo, reparar a enemigos, curar aliados con la vida completa o actuar fuera de su turno.
- Cada unidad tiene una sola acción de combate por turno: atacar o reparar. Ambas acciones son mutuamente excluyentes; el movimiento usa su presupuesto independiente.
- El cambio de turno reinicia las banderas de acción de las unidades del equipo que empieza.
- La validación y la aplicación están en `src/combat.c`, compartidas por jugador e IA.
- Al seleccionar un ingeniero, el HUD muestra `REP1/3`; los aliados reparables tienen un resaltado distinto del rojo utilizado para enemigos atacables.
- La IA intenta atacar si dispone de un ataque válido; en caso contrario, puede reparar un aliado adyacente. Elige primero al aliado con mayor pérdida proporcional de vida y usa la pérdida absoluta como desempate.

## Archivos

- `src/unit_types.c/.h`: valores por clase y capacidades de reparación.
- `src/unit_controller.c/.h`: estado y reinicio de la acción de reparación por turno.
- `src/combat.c/.h`: predicción, validación y aplicación; máscara de blancos de ataque/reparación.
- `src/enemy_ai.c/.h`: selección de aliado y resultado de reparación de IA.
- `src/main.c`: orden del jugador, mensajes y refresco de máscara tras una acción.
- `src/cell_hud.c/.h`: muestra del rango/capacidad (`REP1/3`).
- `src/movement_glow.c/.h`: resaltado de aliados reparables conservando prioridad de la niebla.

No se ha añadido un buffer de mapa nuevo: la máscara de acción existente distingue ataques y reparaciones mediante valores compactos.

## Pruebas ejecutadas

- `sh tests/run_tests.sh`: 23/23 grupos y test adicional de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos y test adicional de perfilado superados con ASan/UBSan.
- GCC 14.2 con `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: 21 módulos C sin diagnósticos.
- C/H: sin espacios finales ni líneas de más de 100 columnas.
- 63 documentos Markdown: enlaces locales válidos.
- `tests/combat_test.c`: capacidad, objetivo del mismo equipo, auto-reparación, límite por acción, curación parcial, límite de PV máximos, fin de turno, objetivo a vida completa y alcance.
- `tests/enemy_ai_test.c`: reparación del aliado adyacente más dañado.
- `tests/movement_glow_test.c`: aliado reparable con paleta de alcance y restauración localizada.

## Límite de validación

Este entorno no dispone del toolchain real de SGDK ni de BlastEm; no se ha generado una ROM ni verificado visualmente la pantalla en emulación. Las pruebas host usan stubs. Compilar con la instalación local y comprobar HUD/paletas/turno de IA siguen siendo la validación final de integración.
