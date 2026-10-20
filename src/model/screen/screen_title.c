#define LOG_LEVEL_FILE LOG_LEVEL_TRACE
#include "conf.h"
#include <model/game/game.h>
#include <model/screen/screen_title.h>

void SCREEN_TITLE_init() {
    PAL_setPalette(PAL0, palette_grey, DMA);
    VDP_drawText("Title Screen", 10, 10);
}

void SCREEN_TITLE_exit() {
    VDP_resetScreen();
}

void SCREEN_TITLE_update() {
}

void SCREEN_TITLE_readJoy() {}

void SCREEN_TITLE_onJoyEvent(u16 joy, u16 changed, u16 state) {
    
    if (joy != JOY_1)return;
    if (state) return;
    if(changed & BUTTON_START) {
        GAME_getInstance()->nextScreen = SCREEN_BATTLEMAP;
    }
}

void SCREEN_TITLE_hInt() {}
void SCREEN_TITLE_vInt() {}
