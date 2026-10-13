#include <model/game/game.h>
static Game game;

Game *GAME_getInstance() { return &game; }

inline void GAME_init() {
    game.dt = 0;
    game.nextScreen = SCREEN_SERVICE_INITIAL_SCREEN;
    game.tileIndex = TILE_USER_INDEX;
}