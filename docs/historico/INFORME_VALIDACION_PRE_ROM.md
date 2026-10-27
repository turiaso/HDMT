# Informe de validación pre-ROM

**Artefacto revisado:** edición completa del proyecto SGDK, mapa lógico 40×40 y 60 unidades.  
**Resultado:** pruebas host y análisis estático satisfactorios; compilación ROM y prueba en BlastEm pendientes por ausencia del toolchain en este entorno.

## Resumen

| Comprobación | Resultado |
|---|---|
| Suite GCC normal | 23/23 grupos superados, incluida cámara espectadora de IA |
| Suite GCC con AddressSanitizer y UndefinedBehaviorSanitizer | 23/23 grupos superados |
| Análisis estático GCC `-fanalyzer` de los 19 módulos C | Sin diagnósticos |
| Propiedades de pathfinding frente a Dijkstra independiente | 20.000 mapas reproducibles; costes y cadenas de padres coherentes |
| Turno completo de IA con escenario 40×40 y 60 unidades | Termina y devuelve el control a Azul; 727 fotogramas simulados y 30 movimientos en el test host |
| Reglas de línea de visión | Bosque/piedras bloquean visión y disparos; prueba de simetría y actualización incremental superada |
| Compilación real de SGDK | No ejecutable aquí: `GDK` no está definido |
| Emulación BlastEm / FPS reales | No ejecutada: BlastEm no está instalado |

El test de turno completo de IA puede no registrar ataques durante el primer turno porque la formación inicial mantiene a los equipos fuera del alcance de visión; no significa que el ataque no funcione. `enemy_ai_test.c` comprueba además la aproximación, la priorización del objetivo, una eliminación preferente y la defensa con cobertura. Ver `INFORME_IA_TACTICA.md`. Los 715 fotogramas son una medida del bucle simulado por el test host, no un benchmark del 68000.

## Turno de IA visible (cambio posterior a la edición completa)

El turno del equipo Rojo entra ahora en una perspectiva temporal de espectador. La cámara centra y sigue suavemente la posición renderizada de la unidad que está actuando, se oculta el cursor del jugador mientras se observa y el HUD inspecciona la casilla del actor sin disparar eventos STAY/CLICK. La niebla permanece activa y renderiza la unión de las celdas visibles por Azul o Rojo; las celdas que ambos desconocen siguen cubiertas, y los estados lógicos por equipo no se modifican ni se mezclan. Al volver a Azul, se restaura el overlay de niebla de Azul y la cámara vuelve suavemente al encuadre guardado; el mando se bloquea durante ese retorno para no desincronizar cursor y cámara.

Se añadieron comprobaciones a los grupos ya existentes de cámara, HUD, IA y `movement_glow`: cámara de seguimiento y retorno, índice de actor de la IA, HUD de espectador sin eventos y representación de terreno sin overlay manteniendo `MAP_VISION_UNSEEN`. El turno completo automático permanece probado en el host, pero esta modificación visual aún debe validarse en una ROM real/BlastEm.

## Corrección defensiva aplicada

En `src/map_data.c`, la segunda pasada de `MapData_init()` ahora comprueba el resultado de `decodeTerrain()` antes de asignar el terreno. La primera pasada ya validaba los símbolos, pero volver a comprobar la operación evita usar una variable no inicializada si la fuente de datos cambiase entre pasadas o el contrato se debilitase. El analizador de GCC había detectado ese caso potencial.

Se eliminó además una comprobación de capacidad duplicada en `src/unit_view.c::releaseUnitSprite()`; no altera el comportamiento.

## Ejecutar las pruebas host

Desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
sh tests/run_tests_sanitized.sh
```

En Windows:

```bat
tests\run_tests.bat
```

Las pruebas host usan stubs de SGDK. Validan reglas de C y parte de la integración, pero no generan los recursos de SGDK ni simulan la VRAM real.

## Compilar la ROM en Windows con SGDK instalado

La documentación oficial de SGDK describe el uso de `makefile.gen`, el directorio `bin` y la salida `out/rom.bin`: [Setup SGDK](https://github.com/Stephane-D/SGDK/wiki/Setup-SGDK-basic), [SGDK Usage](https://github.com/Stephane-D/SGDK/wiki/SGDK-Usage).

Se ha añadido `tools/build_sgdk.bat` para invocar el compilador de forma reproducible. Desde la raíz del proyecto:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" clean
```

Para una compilación limpia de depuración:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" debug-clean
```

La ruta es un ejemplo y debe apuntar a la instalación real de SGDK. El resultado esperado es `out\rom.bin`. Si falla, usa **el primer error de compilación o del compilador de recursos**, no los mensajes derivados, como punto de partida para depurar.

## Checklist manual en BlastEm

1. La ROM arranca y muestra el mapa sin tiles corruptos ni bloques blancos en el HUD.
2. Las máscaras de los bordes aparecen bien alineadas y no tapan el cursor ni el texto; comprobar orden BG_A/BG_B y fuente.
3. El cursor se mueve en exploración; el scroll suave y los límites de cámara funcionan sin temblores.
4. Seleccionar una unidad Azul, comprobar el glow, navegar por casillas alcanzables, confirmar el movimiento y verificar costes por terreno.
5. Confirmar niebla por equipo: las unidades enemigas no visibles no deben aparecer y el terreno explorado debe permanecer recordado. Probar una línea de visión bloqueada por bosque/piedras y una casilla visible al rodear el obstáculo.
6. Atacar un enemigo visible: comprobar daño, posible represalia, muerte y liberación de sprite/ocupación. Un blindado no debe disparar atravesando una casilla intermedia de bosque o piedras; el HUD debe indicar que el obstáculo bloquea el tiro.
7. Pulsar START y esperar el turno Rojo: la cámara debe seguir a las unidades rojas; se deben ver las celdas que conoce cualquiera de los dos equipos y las desconocidas para ambos deben seguir con niebla. Al terminar, debe restaurarse la visión normal azul y volver la cámara al encuadre anterior sin aceptar entrada durante el retorno.
8. Probar las condiciones de misión cambiando `ScenarioDefinition.objective` en `src/scenario.c`: captura, mantener, eliminación y supervivencia. Elegir escenarios desde el selector de arranque; `ACTIVE_MAP_INDEX` solo determina cuál queda resaltado por defecto.
9. Repetir scroll, movimiento y turno Rojo con muchas unidades dentro de pantalla. Observar sprite parpadeante, desapariciones, tiles fuera de sitio y pausas.
10. En esta edición de perfilado `PERF_DIAGNOSTICS` está activo por defecto. Comparar muestras aisladas de 120 fotogramas de `IDLE`, `PLAYER_SCROLL`, `PLAYER_MOVE`, `AI_DECISION`, `AI_CAMERA` y `AI_MOVE_CAMERA`; el contador visible `FPS: NN` es el indicador de fotogramas, mientras que los bloques `PERF` cuantifican operaciones. Consultar `INFORME_DIAGNOSTICO_RENDIMIENTO_120_UNIDADES.md` y poner `PERF_DIAGNOSTICS` a `0` en la build normal tras recoger la medición.
11. Terminar una misión, comprobar la pantalla de victoria/derrota, pulsar A para repetir y B/START para volver al selector; repetir el ciclo y alternar entre los dos escenarios para detectar referencias o sprites residuales.

## Pendientes que siguen abiertos

- Compilación real de ROM con la instalación del usuario.
- Validación visual de VRAM, paletas, fuente, máscaras y límite de sprites por línea.
- Medición empírica de rendimiento con BlastEm y, posteriormente, hardware real si está disponible.
- Inspección del mapa de memoria/linker real de la ROM: las estimaciones host no sustituyen tamaños de símbolos generados con `m68k-elf-size`.
- Completar una partida manual con cada modo de misión y verificar las transiciones de victoria/derrota.

No se afirma que la ROM haya sido compilada ni que BlastEm haya sido probado en este entorno.


### Seguimiento visible del turno rojo

La edición actual hace que la IA espere a que la cámara centre la próxima unidad roja antes de ejecutar su acción y mantiene el encuadre durante 30 fotogramas tras un ataque. La niebla permanece activa durante el turno rojo y representa la unión de visión actual de ambos equipos; las zonas desconocidas para ambos siguen cubiertas y los datos lógicos continúan separados por equipo. Al terminar vuelve la perspectiva azul y se restaura su niebla. Pendiente comprobarlo visualmente en BlastEm.

## Escenarios configurables (actualización posterior)

El registro contiene Punto de control y Defensa del puente. La segunda misión usa una imagen y terreno lógicos propios, 12 unidades por equipo y una condición de mantener el objetivo durante tres finales de turno. Ahora la elección se hace desde el selector de arranque con la cruceta y A/START; `ACTIVE_MAP_INDEX` solo define qué opción aparece resaltada por defecto.


### Ciclo de partida (actualización)

Se añadió una pantalla final que muestra victoria/derrota, misión y número de turno. **A** reinicia la misión actual; **B/START** regresa al selector. La transición libera el mapa gráfico con `MAP_release`, termina el motor de sprites con `SPR_end` y reinicializa las cachés para impedir que una partida herede datos de otra. La lógica de decisión de la pantalla se prueba en `mission_result_test.c`. Sigue siendo necesaria una compilación con SGDK real y una prueba visual de ambas salidas en BlastEm.


### Actualización de IA: maniobras por grupos

Se añadió comprobación de planes tácticos derivados de la misión y una prueba que verifica que una unidad de apoyo alcanza una posición lateral a distancia de ataque antes de disparar. Resultados: 23/23 grupos en suite normal y con ASan/UBSan; 19 módulos C con `-fanalyzer` sin diagnósticos. Esta validación usa stubs de SGDK; sigue siendo necesaria la compilación real de ROM.

## Regresión posterior: sincronización final de sprites de unidad

La versión actual añade una prueba específica para un defecto de presentación en la optimización de reposo: cuando `UnitController` cambiaba de `moving = TRUE` a `FALSE`, `UnitView_update()` podía salir por la ruta rápida de fotograma quieto y conservar la última coordenada interpolada. Se da especialmente cuando el paso siguiente queda bloqueado justo al completar el tramo; el controlador vuelve a la casilla lógica correcta, pero el sprite podría permanecer desplazado.

La vista ahora recuerda si el controlador estaba moviéndose. La transición `moving -> idle` fuerza una única pasada de sincronización que calcula la visibilidad y la posición desde las coordenadas lógicas vigentes. No se añaden recorridos por todos los sprites durante el movimiento normal. La prueba verifica tanto la llegada válida como la vuelta a la casilla anterior al abortar un tramo.

También se añadieron pruebas al controlador para los movimientos arriba, abajo, izquierda, derecha y una ruta con giros. En cada fotograma comprueban la posición renderizada frente al paso interpolado, que la ocupación lógica contiene exactamente a la unidad y que, al terminar, el controlador limpia `segmentPixels`, ruta, paso e índices activos. Otra prueba introduce un ocupante justo en el último fotograma de un tramo y verifica que la unidad se detiene centrada en su casilla actual.

Validación de esta revisión: `sh tests/run_tests.sh` y `sh tests/run_tests_sanitized.sh` pasan; GCC `-fanalyzer` con `-Wall -Wextra -Werror -Wformat=2` no emite diagnósticos al compilar todos los módulos host. Sigue pendiente la compilación real de SGDK y una comprobación visual en BlastEm.
