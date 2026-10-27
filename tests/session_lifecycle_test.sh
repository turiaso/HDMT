#!/usr/bin/env sh
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# Confirma que las referencias prestadas se invalidan antes de liberar el MAP
# y que el selector reaparece usando la última selección, no un estado de juego.
python3 - <<'PY'
from pathlib import Path
main = Path("src/main.c").read_text(encoding="utf-8")
start = main.index("static void releaseCurrentGame(void)")
end = main.index("int main(", start)
cleanup = main[start:end]
assert cleanup.index("MovementGlow_release();") < cleanup.index("MAP_release(worldMap);")
assert cleanup.index("UnitView_release(&unitView);") < cleanup.index("SPR_end();")
assert "memset(actionTargetMask, 0, sizeof(actionTargetMask));" in cleanup
init = main.index("static bool initGameData(void)")
init_end = main.index("static void initCellCursor", init)
init_block = main[init:init_end]
assert "memset(actionTargetMask, 0, sizeof(actionTargetMask));" in init_block
assert "movementRangeCache.initialized = FALSE;" in cleanup
assert "turnHudCache.initialized = FALSE;" in cleanup
assert cleanup.index("UnitView_release(&unitView);") < cleanup.index("SPR_end();")
assert "cellCursor = NULL;" in cleanup
assert "objectiveMarker = NULL;" in cleanup
assert "fogTileReservation = NULL;" in cleanup
assert "mapScrollInitialized = FALSE;" in cleanup
assert "rememberedScenarioIndex = ScenarioMenu_showFrom(rememberedScenarioIndex);" in main
assert "activeScenario = Scenario_get(rememberedScenarioIndex);" in main
video_init = main[main.index("static void initWorldMap(void)"):main.index("static void initVideo(void)")]
assert "fogRenderedTeam = GameTurn_getActiveTeam(&gameTurn);" in video_init
assert "fogRenderedSpectatorMode = FALSE;" in video_init
assert "MovementGlow_setFogContext" in video_init
print("PASS: limpieza invalida cachés antes de liberar recursos y el selector conserva solo el índice de escenario")
PY
