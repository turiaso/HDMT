#!/usr/bin/env sh
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# Cada ejecución usa sus propios binarios temporales: permite ejecutar suites
# en paralelo y evita colisiones con archivos antiguos de /tmp.
TEST_TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hdmnext-tests.XXXXXX")
trap 'rm -rf "$TEST_TMP_DIR"' EXIT

# Suite host deliberadamente limitada a GCC; no recurre a Clang.
CC=gcc
CFLAGS="-std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0"

printf '%s\n' '[1/23] Eventos: prueba funcional'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c tests/cell_events_test.c -o "$TEST_TMP_DIR"/cell_events_test
"$TEST_TMP_DIR"/cell_events_test

printf '%s\n' '[2/23] Cámara y cursor'
$CC $CFLAGS -Itests/stubs -Isrc src/camera_cursor.c tests/camera_cursor_test.c -o "$TEST_TMP_DIR"/camera_cursor_test
"$TEST_TMP_DIR"/camera_cursor_test

printf '%s\n' '[3/23] Datos lógicos del mapa'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c tests/map_data_test.c -o "$TEST_TMP_DIR"/map_data_test
"$TEST_TMP_DIR"/map_data_test

printf '%s\n' '[4/23] Pathfinding y costes'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/pathfinder.c src/unit_types.c tests/pathfinder_test.c -o "$TEST_TMP_DIR"/pathfinder_test
"$TEST_TMP_DIR"/pathfinder_test

printf '%s\n' '[5/23] Unidades: ruta, movimiento y eventos'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c tests/unit_controller_test.c -o "$TEST_TMP_DIR"/unit_controller_test
"$TEST_TMP_DIR"/unit_controller_test

printf '%s\n' '[6/23] Vista de unidad: sin marcadores de ruta'
$CC $CFLAGS -Itests/stubs -Isrc src/camera_cursor.c src/unit_view.c src/team.c src/map_data.c tests/unit_view_test.c -o "$TEST_TMP_DIR"/unit_view_test
"$TEST_TMP_DIR"/unit_view_test

printf '%s\n' '[7/23] Reserva de VRAM para la fuente SGDK'
$CC $CFLAGS -Itests/stubs -Isrc tests/hud_vram_layout_test.c -o "$TEST_TMP_DIR"/hud_vram_layout_test
"$TEST_TMP_DIR"/hud_vram_layout_test
if grep -Eq 'VDP_loadTileSet\(&terrain_tiles, TILE_USER_INDEX|TILE_ATTR_FULL\([^;]*TILE_USER_INDEX' src/main.c; then
    printf '%s\n' 'FAIL: el terreno sigue usando TILE_USER_INDEX, que puede solaparse con la fuente'
    exit 1
fi
if grep -Eq 'VDP_loadDefaultFont|VDP_setPlaneSize' src/main.c; then
    printf '%s\n' 'FAIL: main.c no debe cargar manualmente la fuente ni alterar el tamano de los planos'
    exit 1
fi

printf '%s\n' '[8/23] Contenido y actualizacion del HUD'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/cell_hud.c src/team.c tests/cell_hud_test.c -o "$TEST_TMP_DIR"/cell_hud_test
"$TEST_TMP_DIR"/cell_hud_test
if ! sed -n '/static void updateGameFrame(/,/^}/p' src/main.c | grep -q 'CellHud_tick()'; then
    printf '%s\n' 'FAIL: el feedback del HUD no avanza en cada fotograma'
    exit 1
fi
if sed -n '/static void updateRoutePreview(/,/^}/p' src/main.c | grep -q 'CellHud_setUnitStatus('; then
    printf '%s\n' 'FAIL: la previsualizacion puede cancelar feedback prioritario'
    exit 1
fi
printf '%s\n' 'PASS: feedback persistente, expiracion y cambio de casilla comprobados.'

printf '%s\n' '[9/23] Orden de includes de SGDK en todos los módulos'
# El string.h de SGDK declara strlen/strcmp con u16/s8 y requiere que genesis.h
# ya haya definido los tipos. Compilar todos los módulos con el shim en cabeza
# evita que un módulo nuevo vuelva a incluir <string.h> antes de sus tipos.
$CC $CFLAGS -Wformat=2 -Dmain=sgdk_main -fsyntax-only \
    -Itests/sgdk_include_order -Itests/stubs -Isrc \
    src/main.c src/map_data.c src/cell_events.c src/cell_events_selftest.c \
    src/camera_cursor.c src/cell_hud.c src/pathfinder.c src/unit_types.c \
    src/unit_controller.c src/unit_view.c src/team.c src/game_turn.c \
    src/movement_glow.c src/combat.c src/fog_of_war.c src/scenario.c \
    src/enemy_ai.c src/performance.c src/mission_result.c src/scenario_menu.c src/game_screens.c

printf '%s\n' '[10/23] Sintaxis e integracion del proyecto'
$CC $CFLAGS -Wformat=2 -Dmain=sgdk_main -fsyntax-only -Itests/stubs -Isrc src/main.c src/map_data.c src/cell_events.c src/cell_events_selftest.c src/camera_cursor.c src/cell_hud.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/unit_view.c src/team.c src/game_turn.c src/movement_glow.c src/combat.c src/fog_of_war.c src/scenario.c src/enemy_ai.c src/performance.c src/mission_result.c src/scenario_menu.c src/game_screens.c
$CC $CFLAGS -Wformat=2 -DACTIVE_MAP_INDEX=1 -Dmain=sgdk_main -fsyntax-only -Itests/stubs -Isrc src/main.c src/map_data.c src/cell_events.c src/cell_events_selftest.c src/camera_cursor.c src/cell_hud.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/unit_view.c src/team.c src/game_turn.c src/movement_glow.c src/combat.c src/fog_of_war.c src/scenario.c src/enemy_ai.c src/performance.c src/mission_result.c src/scenario_menu.c src/game_screens.c

printf '%s\n' '[11/23] Sanitizadores, si el compilador los admite'
if $CC $CFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c tests/unit_controller_test.c -o "$TEST_TMP_DIR"/strategy_route_test_san 2>/dev/null; then
    "$TEST_TMP_DIR"/strategy_route_test_san
else
    printf '%s\n' 'SKIP: este compilador no admite ASan/UBSan'
fi

printf '%s\n' '[12/23] Paletas de equipo'
$CC $CFLAGS -Itests/stubs -Isrc src/team.c tests/team_palette_test.c -o "$TEST_TMP_DIR"/team_palette_test
"$TEST_TMP_DIR"/team_palette_test

printf '%s\n' '[13/23] Turnos, puntos y objetivo de misión'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/game_turn.c src/combat.c tests/game_turn_test.c -o "$TEST_TMP_DIR"/game_turn_test
"$TEST_TMP_DIR"/game_turn_test

printf '%s\n' '[14/23] Alcance ponderado y restriccion del cursor'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/pathfinder.c src/unit_types.c tests/movement_range_test.c -o "$TEST_TMP_DIR"/movement_range_test
"$TEST_TMP_DIR"/movement_range_test

printf '%s\n' '[15/23] Glow: paleta animada y parcheo de tiles'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/movement_glow.c tests/movement_glow_test.c -o "$TEST_TMP_DIR"/movement_glow_test
"$TEST_TMP_DIR"/movement_glow_test

printf '%s\n' '[extra] Paletas siempre transferidas en VBlank'
if grep -REq 'PAL_setPalette\([^;]*,[[:space:]]*DMA\)' src; then
    printf '%s\n' 'FAIL: PAL_setPalette usa DMA inmediato; puede causar CRAM dots'
    exit 1
fi

printf '%s\n' '[extra] Separación estricta entre hover y pathfinding'
if grep -q 'ensureMovementRange' src/unit_controller.c; then
    printf '%s\n' 'FAIL: queda una ruta perezosa para recalcular el alcance'
    exit 1
fi
if sed -n '/bool UnitController_updatePreview(/,/^}/p' src/unit_controller.c | \
    grep -Eq 'Pathfinder_(findRoute|buildReachability)|UnitController_refreshMovementRange'; then
    printf '%s\n' 'FAIL: el hover ejecuta una búsqueda o reconstruye el alcance'
    exit 1
fi
printf '%s\n' 'PASS: el hover solo consulta la caché; la ruta se busca al confirmar A.'

printf '%s\n' '[16/23] Combate y atributos de unidades'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/combat.c tests/combat_test.c -o "$TEST_TMP_DIR"/combat_test
"$TEST_TMP_DIR"/combat_test

printf '%s\n' '[17/23] Niebla de guerra por equipo'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/fog_of_war.c src/combat.c tests/fog_of_war_test.c -o "$TEST_TMP_DIR"/fog_of_war_test
"$TEST_TMP_DIR"/fog_of_war_test

printf '%s\n' '[18/23] Escenarios configurables, despliegues y presupuesto VDP'
if ! grep -q 'MAP bridge_map "gfx/bridge_map.png" terrain_tiles BEST 0' res/resources.res; then
    printf '%s\n' 'FAIL: falta el recurso MAP del escenario Defensa del puente'
    exit 1
fi
if [ ! -f res/gfx/bridge_map.png ]; then
    printf '%s\n' 'FAIL: falta res/gfx/bridge_map.png'
    exit 1
fi
if ! grep -q 'MAP bridge_skirmish_map "gfx/bridge_skirmish_map.png" terrain_tiles BEST 0' res/resources.res; then
    printf '%s\n' 'FAIL: falta el recurso MAP de Puente del desfiladero'
    exit 1
fi
if [ ! -f res/gfx/bridge_skirmish_map.png ]; then
    printf '%s\n' 'FAIL: falta res/gfx/bridge_skirmish_map.png'
    exit 1
fi
if ! grep -q 'MAP ruins_crossing_map "gfx/ruins_crossing_map.png" terrain_tiles BEST 0' res/resources.res; then
    printf '%s\n' 'FAIL: falta el recurso MAP de Cruce de las ruinas'
    exit 1
fi
if [ ! -f res/gfx/ruins_crossing_map.png ]; then
    printf '%s\n' 'FAIL: falta res/gfx/ruins_crossing_map.png'
    exit 1
fi
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/game_turn.c src/scenario.c tests/scenario_test.c -o "$TEST_TMP_DIR"/scenario_test
"$TEST_TMP_DIR"/scenario_test

printf '%s\n' '[19/23] IA roja: movimiento y ataque'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/combat.c src/enemy_ai.c tests/enemy_ai_test.c -o "$TEST_TMP_DIR"/enemy_ai_test
"$TEST_TMP_DIR"/enemy_ai_test

printf '%s\n' '[extra] Integridad visual y muestreo correcto del contador FPS'
sh tests/fps_overlay_test.sh

printf '%s\n' '[20/23] Instrumentacion con diagnosticos activados'
$CC $CFLAGS -UPERF_DIAGNOSTICS -DPERF_DIAGNOSTICS=1 -Dmain=sgdk_main -fsyntax-only -Itests/stubs -Isrc src/performance.c src/pathfinder.c src/unit_view.c src/main.c

printf '%s\n' '[21/23] Propiedades del buscador de rutas (20.000 mapas)'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/pathfinder.c src/unit_types.c tests/pathfinder_property_test.c -o "$TEST_TMP_DIR"/pathfinder_property_test
"$TEST_TMP_DIR"/pathfinder_property_test

printf '%s\n' '[22/23] Simulacion de turno completo de IA con 120 unidades (60 por equipo)'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/scenario.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/game_turn.c src/combat.c src/fog_of_war.c src/enemy_ai.c tests/ai_full_turn_test.c -o "$TEST_TMP_DIR"/ai_full_turn_test
"$TEST_TMP_DIR"/ai_full_turn_test
printf '%s\n' '[extra] Turno de IA en los cuatro escenarios'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/scenario.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/game_turn.c src/combat.c src/enemy_ai.c tests/ai_scenario_smoke_test.c -o "$TEST_TMP_DIR"/ai_scenario_smoke_test
"$TEST_TMP_DIR"/ai_scenario_smoke_test

printf '%s\n' '[23/23] Pantalla final: repetir misión o volver al selector'
$CC $CFLAGS -Itests/stubs -Isrc src/mission_result.c tests/mission_result_test.c -o "$TEST_TMP_DIR"/mission_result_test
"$TEST_TMP_DIR"/mission_result_test
printf '%s\n' '[extra] Resumen visual de misión, supervivientes y botones'
$CC $CFLAGS -Itests/stubs -Isrc src/mission_result.c src/game_screens.c tests/game_screens_test.c -o "$TEST_TMP_DIR"/game_screens_test
"$TEST_TMP_DIR"/game_screens_test
printf '%s\n' '[extra] Bloqueo de órdenes tras fin de partida'
sh tests/mission_end_guard_test.sh
printf '%s\n' '[extra] Perfilado: muestreo aislado por fase de actividad'
$CC $CFLAGS -UPERF_DIAGNOSTICS -DPERF_DIAGNOSTICS=1 -Itests/stubs -Isrc src/performance.c tests/performance_phase_test.c -o "$TEST_TMP_DIR"/performance_phase_test
"$TEST_TMP_DIR"/performance_phase_test
printf '%s\n' 'OK: 23 grupos y prueba adicional de perfilado superados.'
printf '%s\n' '[extra] Estrés de combate real con 120 unidades y verificación espacial'
$CC $CFLAGS -Itests/stubs -Isrc src/map_data.c src/cell_events.c src/pathfinder.c src/unit_types.c src/unit_controller.c src/combat.c src/enemy_ai.c tests/ai_combat_stress_test.c -o "$TEST_TMP_DIR"/ai_combat_stress_test
"$TEST_TMP_DIR"/ai_combat_stress_test
printf '%s\n' '[extra] Ciclo del selector y persistencia de escenario'
$CC $CFLAGS -Itests/stubs -Isrc src/scenario_menu.c tests/scenario_menu_test.c -o "$TEST_TMP_DIR"/scenario_menu_test
"$TEST_TMP_DIR"/scenario_menu_test
printf '%s\n' '[extra] Orden de liberación y persistencia de selección'
sh tests/session_lifecycle_test.sh
