# Ciclo de partida, reinicio y regreso al selector

## Flujo de navegación

`main()` conserva únicamente `rememberedScenarioIndex`, el índice de la última
misión elegida. Cada vuelta por el selector llama a
`ScenarioMenu_showFrom(rememberedScenarioIndex)`. Si se confirma otra misión, el
nuevo índice se guarda y se obtiene su `ScenarioDefinition` mediante
`Scenario_get()`.

La pantalla final tiene dos salidas:

- **A — repetir:** reinicia directamente el escenario actual mediante una nueva
  ejecución de `initGameData()` e `initVideo()`.
- **B o START — selector:** libera la sesión y abre el selector conservando el
  índice de la última misión. Esto conserva la preferencia de menú, no el estado
  de la partida anterior.

`ScenarioMenu_show()` sigue disponible como entrada compatible que usa
`ACTIVE_MAP_INDEX`; el bucle principal usa la entrada explícita
`ScenarioMenu_showFrom()`.

## Orden de liberación

`releaseCurrentGame()` invalida referencias prestadas antes de liberar recursos:

1. `MovementGlow_release()` limpia la referencia al mapa, máscaras de alcance y
   ataque, contexto de niebla y caché de estilos. El módulo no libera el mapa,
   porque no es su propietario.
2. `UnitView_release()` libera las instancias de sprites de unidades y borra sus
   cachés de posiciones/visibilidad.
3. `MAP_release()` libera el mapa gráfico y se borra `worldMap`.
4. `SPR_end()` termina los sprites restantes (cursor, marcador de objetivo y
   reserva de tile de niebla).
5. Se invalidan punteros y cachés de presentación, se limpia la máscara de
   objetivos y se borran ambos planos antes de regresar al menú.

La siguiente inicialización reconstruye los datos lógicos con `MapData_init()`,
restaura las unidades de despliegue, reinicia cámara/cursor, turnos, IA, niebla,
eventos, `MovementGlow`, vista de unidades y cachés del HUD. Los búferes lógicos
se siguen compartiendo entre escenarios; nunca hay dos partidas activas a la vez.

## Pruebas

- `tests/scenario_menu_test.c` valida que el selector se puede abrir desde el
  último índice elegido, normaliza índices inválidos y mantiene la navegación
  circular.
- `tests/movement_glow_test.c` comprueba que `MovementGlow_release()` invalida
  máscaras y referencias, no anima una caché liberada y permite inicializar una
  sesión nueva.
- `tests/session_lifecycle_test.sh` verifica el orden de limpieza y que el
  bucle principal guarda solo el índice de escenario al volver al selector.

Ejecuta las suites normales y ASan/UBSan tras cambiar este flujo. Estas pruebas
host no sustituyen una compilación SGDK ni la comprobación visual en BlastEm.
