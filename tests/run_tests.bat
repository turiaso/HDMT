@echo off
setlocal
cd /d "%~dp0.."
set "TEST_DIR=%TEMP%\sgdk_strategy_tests"
if not exist "%TEST_DIR%" mkdir "%TEST_DIR%"

rem Comprobaciones estaticas de la reserva de VRAM y la fuente SGDK.
echo [1/23] Reserva de VRAM, fuente SGDK y recursos de escenario
if not exist res\gfx\bridge_map.png goto BAD_SCENARIO_RESOURCE
if not exist res\gfx\bridge_skirmish_map.png goto BAD_SCENARIO_RESOURCE
if not exist res\gfx\ruins_crossing_map.png goto BAD_SCENARIO_RESOURCE
findstr /L /C:"MAP bridge_map" res\resources.res >nul || goto BAD_SCENARIO_RESOURCE
findstr /L /C:"MAP bridge_skirmish_map" res\resources.res >nul || goto BAD_SCENARIO_RESOURCE
findstr /L /C:"MAP ruins_crossing_map" res\resources.res >nul || goto BAD_SCENARIO_RESOURCE
findstr /L /C:"VDP_loadTileSet(&terrain_tiles, TILE_USER_INDEX" src\main.c >nul && goto BAD_TILE_BASE
findstr /L /C:"#define TERRAIN_VRAM_TILE_INDEX (TILE_SYSTEM_INDEX + TILE_SYSTEM_LENGTH + FONT_LENGTH)" src\game_config.h >nul || goto BAD_TILE_BASE
findstr /L /C:"VDP_loadDefaultFont" src\main.c >nul && goto BAD_FONT_INIT
findstr /L /C:"VDP_setPlaneSize" src\main.c >nul && goto BAD_FONT_INIT
findstr /L /C:"PAL_setPalette(PAL1, glowColors, DMA);" src\movement_glow.c >nul && goto BAD_PALETTE_TRANSFER
findstr /L /C:"PAL_setPalette(PAL0, terrain_palette.data, DMA);" src\main.c >nul && goto BAD_PALETTE_TRANSFER
findstr /L /C:"PAL_setPalette(PAL2, cell_cursor.palette->data, DMA);" src\main.c >nul && goto BAD_PALETTE_TRANSFER
findstr /L /C:"PAL_setPalette(PAL2, bluePalette, DMA);" src\team.c >nul && goto BAD_PALETTE_TRANSFER
findstr /L /C:"PAL_setPalette(PAL3, redPalette, DMA);" src\team.c >nul && goto BAD_PALETTE_TRANSFER
echo PASS: reserva de VRAM y transferencias de paleta seguras.

set "CC="
where gcc >nul 2>nul && set "CC=gcc"
rem No se recurre a Clang; las pruebas host usan GCC o se omiten.
if not defined CC goto NO_HOST_COMPILER

echo [2/23] Eventos
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c tests\cell_events_test.c -o "%TEST_DIR%\cell_events_test.exe" || exit /b 1
"%TEST_DIR%\cell_events_test.exe" || exit /b 1

echo [3/23] Camara y cursor
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\camera_cursor.c tests\camera_cursor_test.c -o "%TEST_DIR%\camera_cursor_test.exe" || exit /b 1
"%TEST_DIR%\camera_cursor_test.exe" || exit /b 1

echo [4/23] Datos del mapa
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c tests\map_data_test.c -o "%TEST_DIR%\map_data_test.exe" || exit /b 1
"%TEST_DIR%\map_data_test.exe" || exit /b 1

echo [5/23] Rutas y costes
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\pathfinder.c src\unit_types.c tests\pathfinder_test.c -o "%TEST_DIR%\pathfinder_test.exe" || exit /b 1
"%TEST_DIR%\pathfinder_test.exe" || exit /b 1

echo [6/23] Unidades y puntos de movimiento
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c tests\unit_controller_test.c -o "%TEST_DIR%\unit_controller_test.exe" || exit /b 1
"%TEST_DIR%\unit_controller_test.exe" || exit /b 1

echo [7/23] Vista de unidad y paletas sin marcadores de ruta
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\camera_cursor.c src\unit_view.c src\team.c src\map_data.c tests\unit_view_test.c -o "%TEST_DIR%\unit_view_test.exe" || exit /b 1
"%TEST_DIR%\unit_view_test.exe" || exit /b 1

echo [8/23] HUD y fuente
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\cell_hud.c src\team.c tests\cell_hud_test.c -o "%TEST_DIR%\cell_hud_test.exe" || exit /b 1
"%TEST_DIR%\cell_hud_test.exe" || exit /b 1
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc tests\hud_vram_layout_test.c -o "%TEST_DIR%\hud_vram_layout_test.exe" || exit /b 1
"%TEST_DIR%\hud_vram_layout_test.exe" || exit /b 1

echo [9/23] Orden de includes de SGDK
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -fsyntax-only -Itests\sgdk_include_order -Itests\stubs -Isrc src\pathfinder.c || exit /b 1

echo [10/23] Integracion y sintaxis del proyecto
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Wformat=2 -Dmain=sgdk_main -fsyntax-only -Itests\stubs -Isrc src\main.c src\map_data.c src\cell_events.c src\cell_events_selftest.c src\camera_cursor.c src\cell_hud.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\unit_view.c src\team.c src\game_turn.c src\movement_glow.c src\combat.c src\fog_of_war.c src\scenario.c src\enemy_ai.c src\performance.c src\mission_result.c || exit /b 1
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Wformat=2 -DACTIVE_MAP_INDEX=1 -Dmain=sgdk_main -fsyntax-only -Itests\stubs -Isrc src\main.c src\map_data.c src\cell_events.c src\cell_events_selftest.c src\camera_cursor.c src\cell_hud.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\unit_view.c src\team.c src\game_turn.c src\movement_glow.c src\combat.c src\fog_of_war.c src\scenario.c src\enemy_ai.c src\performance.c src\mission_result.c || exit /b 1

echo [11/23] Paletas azul y rojo
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\team.c tests\team_palette_test.c -o "%TEST_DIR%\team_palette_test.exe" || exit /b 1
"%TEST_DIR%\team_palette_test.exe" || exit /b 1

echo [12/23] Turnos, puntos y objetivo
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\game_turn.c src\combat.c tests\game_turn_test.c -o "%TEST_DIR%\game_turn_test.exe" || exit /b 1
"%TEST_DIR%\game_turn_test.exe" || exit /b 1

echo [13/23] Alcance ponderado
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\pathfinder.c src\unit_types.c tests\movement_range_test.c -o "%TEST_DIR%\movement_range_test.exe" || exit /b 1
"%TEST_DIR%\movement_range_test.exe" || exit /b 1

echo [14/23] Glow: paleta animada y parcheo de tiles
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\movement_glow.c tests\movement_glow_test.c -o "%TEST_DIR%\movement_glow_test.exe" || exit /b 1
"%TEST_DIR%\movement_glow_test.exe" || exit /b 1

echo [15/23] Integracion de alcance, cursor y glow
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Dmain=sgdk_main -fsyntax-only -Itests\stubs -Isrc src\main.c src\movement_glow.c src\camera_cursor.c src\pathfinder.c src\unit_controller.c src\unit_view.c src\combat.c src\fog_of_war.c || exit /b 1

echo [16/23] Combate y atributos
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\combat.c tests\combat_test.c -o "%TEST_DIR%\combat_test.exe" || exit /b 1
"%TEST_DIR%\combat_test.exe" || exit /b 1

echo [17/23] Niebla de guerra
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\fog_of_war.c src\combat.c tests\fog_of_war_test.c -o "%TEST_DIR%\fog_of_war_test.exe" || exit /b 1
"%TEST_DIR%\fog_of_war_test.exe" || exit /b 1

echo [18/23] Escenarios configurables, despliegue y presupuesto VDP
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\game_turn.c src\scenario.c tests\scenario_test.c -o "%TEST_DIR%\scenario_test.exe" || exit /b 1
"%TEST_DIR%\scenario_test.exe" || exit /b 1

echo [19/23] IA roja: movimiento y ataque
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\combat.c src\enemy_ai.c tests\enemy_ai_test.c -o "%TEST_DIR%\enemy_ai_test.exe" || exit /b 1
"%TEST_DIR%\enemy_ai_test.exe" || exit /b 1

echo [20/23] Instrumentacion con diagnosticos activados
"%CC%" -std=c99 -Wall -Wextra -Werror -UPERF_DIAGNOSTICS -DPERF_DIAGNOSTICS=1 -Dmain=sgdk_main -fsyntax-only -Itests\stubs -Isrc src\performance.c src\pathfinder.c src\unit_view.c src\main.c || exit /b 1

echo [21/23] Propiedades del buscador de rutas (20.000 mapas)
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\pathfinder.c src\unit_types.c tests\pathfinder_property_test.c -o "%TEST_DIR%\pathfinder_property_test.exe" || exit /b 1
"%TEST_DIR%\pathfinder_property_test.exe" || exit /b 1

echo [22/23] Turno completo de IA con 120 unidades (60 por equipo)
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\map_data.c src\scenario.c src\cell_events.c src\pathfinder.c src\unit_types.c src\unit_controller.c src\game_turn.c src\combat.c src\fog_of_war.c src\enemy_ai.c tests\ai_full_turn_test.c -o "%TEST_DIR%\ai_full_turn_test.exe" || exit /b 1
"%TEST_DIR%\ai_full_turn_test.exe" || exit /b 1

echo [23/23] Pantalla final: repetir o volver al selector
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\mission_result.c tests\mission_result_test.c -o "%TEST_DIR%\mission_result_test.exe" || exit /b 1
"%TEST_DIR%\mission_result_test.exe" || exit /b 1

echo [extra] Resumen visual: victoria, derrota, empate y controles
"%CC%" -std=c99 -Wall -Wextra -Werror -DPERF_DIAGNOSTICS=0 -Itests\stubs -Isrc src\mission_result.c src\game_screens.c tests\game_screens_test.c -o "%TEST_DIR%\game_screens_test.exe" || exit /b 1
"%TEST_DIR%\game_screens_test.exe" || exit /b 1

echo [extra] Perfilado: muestreo aislado por fase de actividad
"%CC%" -std=c99 -Wall -Wextra -Werror -UPERF_DIAGNOSTICS -DPERF_DIAGNOSTICS=1 -Itests\stubs -Isrc src\performance.c tests\performance_phase_test.c -o "%TEST_DIR%\performance_phase_test.exe" || exit /b 1
"%TEST_DIR%\performance_phase_test.exe" || exit /b 1
echo OK: 23 grupos y prueba adicional de perfilado superados.
del /q "%TEST_DIR%\*.exe" >nul 2>nul
exit /b 0

:BAD_TILE_BASE
echo FAIL: el atlas debe usar TERRAIN_VRAM_TILE_INDEX.
exit /b 1

:BAD_FONT_INIT
echo FAIL: main.c no debe cargar manualmente la fuente ni modificar el tamano de los planos.
exit /b 1

:BAD_PALETTE_TRANSFER
echo FAIL: PAL_setPalette no debe usar DMA inmediato durante el juego; usa DMA_QUEUE_COPY.
exit /b 1

:BAD_SCENARIO_RESOURCE
echo FAIL: falta el recurso gráfico del segundo escenario o su declaración en resources.res.
exit /b 1

:NO_HOST_COMPILER
echo SKIP: no hay GCC en PATH; las pruebas host se omiten.
echo Compila con SGDK y prueba la ROM en BlastEm para validar el hardware real.
exit /b 0
