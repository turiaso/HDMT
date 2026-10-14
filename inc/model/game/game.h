#ifndef GAME_H
#define GAME_H

#include "conf.h"
#include "genesis.h"

typedef struct Game {
    u32 dt;
    u16 nextScreen;
    u16 tileIndex;
    void *ctx;
} Game;

Game *GAME_getInstance();
void GAME_init();

#endif // GAME_H