#include <model/game/game.h>
#include <service/screen_service.h>

static ScreenType selectedScreen;
static u32 lastTick = 0;
static u16 accumulatedDelta = 0;

ScreenInfo const Screens[SCREEN_MACRO_TOTAL] = {
#define X_MACRO(name) SCREEN_MACRO_INFO_ENTRY(name),
    SCREEN_MACRO_LIST
#undef X_MACRO
};

void _SCREEN_SERVICE_startScreen(ScreenType screen);
void _SCREEN_SERVICE_closeScreen();

void _SCREEN_SERVICE_joyEventHandler(u16 joy, u16 changed, u16 state) { Screens[selectedScreen].joyEvent(joy, changed, state); }
void _SCREEN_SERVICE_vInt() {
    // TEXT_DRAW_F("V FreeM: %5lu CPU: %3u", 40, 26, TEXT_ALIGN_R, MEM_getFree(), SYS_getCPULoad());
    Screens[selectedScreen].vInt();
}

HINTERRUPT_CALLBACK _SCREEN_SERVICE_hInt() {
    // TEXT_DRAW_F("H FreeM: %5lu CPU: %3u", 40, 27, TEXT_ALIGN_R, MEM_getFree(), SYS_getCPULoad());
    Screens[selectedScreen].hInt();
}

void SCREEN_SERVICE_init() {
    selectedScreen = SCREEN_SERVICE_INITIAL_SCREEN;
    JOY_setEventHandler(NULL);
    SYS_setHIntCallback(NULL);
    SYS_setVIntCallback(NULL);
    _SCREEN_SERVICE_startScreen(selectedScreen);
    lastTick = 0;
}

void SCREEN_SERVICE_update() {
    u32 now = getTick();
    accumulatedDelta += now - lastTick;
    lastTick = now;
    if (selectedScreen != GAME_getInstance()->nextScreen) {
        _SCREEN_SERVICE_closeScreen();
        _SCREEN_SERVICE_startScreen(GAME_getInstance()->nextScreen);
    } else {
        Screens[selectedScreen].readJoy();
    }
    if (selectedScreen == GAME_getInstance()->nextScreen && (accumulatedDelta > SCREEN_SERVICE_UPDATE_TICK_INTERVAL)) {
        GAME_getInstance()->dt = accumulatedDelta;
        Screens[selectedScreen].update();
        accumulatedDelta = 0;
    }
    SPR_update();
#if DEBUG
    LOG_DEBUG("FreeM: %5lu CPU: %3u", MEM_getFree(), SYS_getCPULoad());
#endif
    SYS_doVBlankProcess();
}

void _SCREEN_SERVICE_startScreen(ScreenType screen) {
    SYS_disableInts();
    lastTick = 0;
    accumulatedDelta = 0;
    selectedScreen = screen;
    SPR_init();
    PAL_setColors(0, palette_black, 64, DMA);
    Screens[selectedScreen].init();
    JOY_setEventHandler(_SCREEN_SERVICE_joyEventHandler);
    SYS_setHIntCallback(_SCREEN_SERVICE_hInt);
    SYS_setVIntCallback(&_SCREEN_SERVICE_vInt);
    SYS_enableInts();
}

void _SCREEN_SERVICE_closeScreen() {
    JOY_setEventHandler(NULL);
    SYS_setHIntCallback(NULL);
    SYS_setVIntCallback(NULL);
    SYS_disableInts();
    Screens[selectedScreen].exit();
    SPR_clear();
    SYS_doVBlankProcess();
    waitTick(100);
    SYS_enableInts();
}