#define LOG_LEVEL_FILE LOG_LEVEL_TRACE
#include "conf.h"
#include <model/game/game.h>
#include <model/screen/screen_main.h>

void SCREEN_MAIN_init() {
    PAL_setPalette(PAL0, palette_grey, DMA);
    LOG_TRACE("Initializing Screen...");
    BREAKPOINT("%s","Screen initialization complete.");
    LOG_INFO("Initialized Screen.");
    VDP_drawText("Main Screen", 10, 10);
}

void SCREEN_MAIN_exit() {
    VDP_resetScreen();
}

void SCREEN_MAIN_update() {
}

void SCREEN_MAIN_readJoy() {}

void SCREEN_MAIN_onJoyEvent(u16 joy, u16 changed, u16 state) {
    if (joy != JOY_1)return;
    if (state) return;
    if(changed & BUTTON_START) {
        GAME_getInstance()->nextScreen = SCREEN_COMPANY;
    }
}

void SCREEN_MAIN_hInt() {}
void SCREEN_MAIN_vInt() {}
