#define LOG_LEVEL_FILE LOG_LEVEL_TRACE
#include "conf.h"
#include <model/game/game.h>
#include <model/screen/screen_battlemap.h>

void SCREEN_BATTLEMAP_init() {
    PAL_setPalette(PAL0, palette_grey, DMA);
    VDP_drawText("Battlemap Screen", 10, 10);
}

void SCREEN_BATTLEMAP_exit() {
    VDP_resetScreen();
}

void SCREEN_BATTLEMAP_update() {
}

void SCREEN_BATTLEMAP_readJoy() {}

void SCREEN_BATTLEMAP_onJoyEvent(u16 joy, u16 changed, u16 state) {}

void SCREEN_BATTLEMAP_hInt() {}
void SCREEN_BATTLEMAP_vInt() {}
