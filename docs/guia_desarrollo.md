# Guía práctica para editar el proyecto

Esta es la guía de trabajo para realizar cambios sin desordenar el motor ni introducir regresiones. Las cabeceras de cada módulo (`src/*.h`) definen su API; `docs/arquitectura.md` describe quién posee cada estado y qué contratos comparten los subsistemas.

## 1. Entorno de trabajo recomendado

1. Conserva una copia intacta del ZIP que compila o se ejecuta correctamente.
2. Haz un cambio pequeño, centrado en un módulo y una responsabilidad.
3. Ejecuta `sh tests/run_tests.sh` y después `sh tests/run_tests_sanitized.sh`.
4. Ejecuta `sh tools/check_source_quality.sh` para comprobar estilo, compilación estricta y análisis estático; revisa también `git diff --check`.
5. Compila una ROM con SGDK y prueba el comportamiento en BlastEm cuando el cambio toque VDP, recursos, sprites, cámara, HUD o rendimiento.
6. Anota el comportamiento visible esperado y los límites que no se deben modificar.

Si un test tarda más o falla, no declares la suite aprobada por los primeros grupos: comprueba el código de salida final. Los tests host usan stubs y no sustituyen la ROM.

## 2. Mapa rápido: dónde editar

| Quiero cambiar… | Archivo principal |
|---|---|
| Velocidad de cámara, repetición del cursor, geometría o presupuestos VDP | `src/game_config.h` |
| Despliegue inicial, nombre de escenario, misión o mapa asociado | `src/scenario.c` y `src/scenario.h` |
| Geometría lógica, terreno, coste, defensa o bloqueo de visión | `src/map_data.c` / `src/map_data.h` |
| Vida/ataque/defensa/alcance/visión/movimiento y coste por terreno de cada clase | `src/unit_types.c` / `src/unit_types.h` |
| Selección, orden, coste de ruta, consumo de puntos o colisión | `src/unit_controller.c` / `.h` |
| Algoritmo de ruta o máscara de alcanzabilidad | `src/pathfinder.c` / `.h` (`Pathfinder_*ForType` recibe la clase del actor); reglas y tabla en [`movimiento_por_clase_y_terreno.md`](movimiento_por_clase_y_terreno.md) |
| Despacho de eventos compartidos por celda/unidad/turno | `src/cell_events.c` / `.h` |
| Visión, exploración persistente y cálculo de celdas sucias | `src/fog_of_war.c` / `.h` |
| Asignación de sprites, prioridades, cámara y unidades fuera del viewport | `src/unit_view.c` / `.h` |
| Apariencia de las celdas de alcance, blancos de ataque y niebla | `src/movement_glow.c` / `.h` |
| Validación/daño/represalia/alcance de ataque y reparación de ingenieros | `src/combat.c` / `.h` |
| Heurística, roles, terreno y decisiones del equipo rojo | `src/enemy_ai.c` / `.h`; reglas actuales en [`ia_tactica_terreno.md`](ia_tactica_terreno.md) |
| Textos y estado mostrado en pantalla | `src/cell_hud.c` / `.h` |
| Selector y pantallas de menú/resultado | `src/scenario_menu.c`, `src/game_screens.c`, `src/mission_result.c` |
| Contadores de trabajo por fase | `src/performance.c` / `.h`, activados desde `src/game_config.h` |
| Flujo principal e integración entre sistemas | `src/main.c` |

## 3. Reglas de estilo C

- Usa C compatible con la toolchain de SGDK y con el estándar C99 empleado por las pruebas host. Evita dependencias de heap, excepciones de C++ o bibliotecas de escritorio.
- Mantén cuatro espacios, llaves K&R y líneas de hasta 100 columnas. `.clang-format` documenta el estilo esperado; respeta los órdenes de inclusión deliberados de SGDK (`genesis.h` debe preceder a `string.h`).
- Una función debe tener una responsabilidad clara. Si mezcla validación, cálculo, cambios de estado y dibujo, extrae helpers con nombres que describan la fase.
- Nombra funciones públicas como `Modulo_verboNombre`; helpers privados `static` con verbo y propósito claros. Devuelve errores explícitos en APIs que puedan fallar; no ocultes un `NULL`, capacidad insuficiente o ID inválido.
- Comenta invariantes, propiedad de los datos, costes o porqués no obvios; no comentes línea por línea lo que ya dice el código.
- Mantén constantes editables en `game_config.h` o en la tabla de datos correspondiente. Evita replicar números de reglas en `main.c`, la IA y el HUD. Los límites compartidos del mapa de partida se definen en `map_data.h`; `MapData` como contenedor genérico no debe imponer el límite de las vistas de juego.
- No ordenes automáticamente los `#include` sin revisar la regla de tipos de SGDK. La prueba de cabeceras del test detecta el caso conocido de `string.h` antes de los tipos base.

## 4. Contratos clave antes de cambiar código

- **Posición:** solo `UnitController` modifica la posición lógica y la ocupación; `UnitView` refleja la posición al sprite y hace el ajuste visual final.
- **Rutas:** la vista previa no debe disparar `Pathfinder_findRoute()` ni reconstruir el alcance. Las llamadas de juego deben usar `Pathfinder_findRouteForType()` / `Pathfinder_buildReachabilityForType()` para aplicar los costes de la clase; las funciones sin sufijo conservan el perfil de infantería para herramientas y pruebas genéricas. No compartir el workspace de búsqueda mientras una ruta confirmada se está consumiendo.
- **Niebla:** explorado y visible son estados distintos. No revelar ocupantes enemigos ocultos; conservar la lista de celdas sucias y el parcheo gráfico localizado.
- **Combate y reparación:** las comprobaciones de objetivo, alcance, línea de visión, daño y reparación deben compartir el código de `combat.c`/`map_data.c` tanto en el jugador como en la IA. Atacar y reparar gastan la misma acción de la unidad en el turno. El ingeniero solo repara aliados adyacentes heridos, hasta 3 PV y sin superar su vida máxima.
- **IA:** una llamada resuelve una acción, no un turno entero; la animación se desarrolla fuera del cálculo táctico. Los destinos deben ser alcanzables según el perfil terreno/clase; la utilidad de cobertura, línea de fuego y apoyo se puntúa al decidir, nunca durante la animación. Consulta [`ia_tactica_terreno.md`](ia_tactica_terreno.md).
- **Renderizado:** no recorrer 120 unidades ni reconstruir la SAT si solo hay que actualizar los sprites activos. Admite que una unidad lógica puede no tener sprite asignado por los límites del hardware.
- **Escenarios:** sus buffers grandes son compartidos. No inicializar simultáneamente dos definiciones con el mismo almacenamiento.
- **Eventos:** el ID de perfil guardado en cada celda apunta a callbacks compartidos. No guardar punteros por celda ni cambiar el sistema sin revisar el diseño de RAM.

## 5. Comandos de comprobación

Prueba normal:

```sh
sh tests/run_tests.sh
```

Prueba con AddressSanitizer y UndefinedBehaviorSanitizer:

```sh
sh tests/run_tests_sanitized.sh
```

Comprobación de estilo y análisis estricto (incluye `-fanalyzer`):

```sh
sh tools/check_source_quality.sh
```

Compilación estricta de módulos (ajusta las rutas de stubs si cambian):

```sh
for f in src/*.c; do
  gcc -Isrc -Itests/stubs -Dmain=sgdk_main \
      -Wall -Wextra -Werror -Wformat=2 -fanalyzer -fsyntax-only "$f" || exit 1
done
```

Compilación real Windows/SGDK desde la raíz:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" clean
```

Salida prevista: `out\rom.bin`. Luego verifica en BlastEm el flujo de partida, bordes de scroll, límite de sprites por línea, selección, llegada a celda, niebla, ataques, turno rojo y pantalla de resultado.

## 6. Qué no hacer sin plan explícito

- No aumentar `GAME_MAX_UNITS`, `FOG_MAX_MAP_CELLS`, el tamaño del mapa o los búferes estáticos de forma aislada.
- No introducir cálculos pesados dentro del bucle por fotograma cuando puedan hacerse al cambiar el estado.
- No usar un redraw total como solución por defecto para un cambio local.
- No cambiar el orden lógico `beforeLeave -> leave -> beforeEnter -> enter` ni la sincronización de ocupación sin actualizar los tests de eventos/movimiento.
- No implementar las propuestas de optimización de RAM A/B/C ni rediseñar el sistema de eventos compartidos por casilla sin acordarlo explícitamente.
