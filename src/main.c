#include "conf.h"
#include "genesis.h"
#include <model/game/game.h>
#include <service/screen_service.h>


int main(bool resetType) {
    if (!resetType)
        SYS_hardReset();

    GAME_init();
    SCREEN_SERVICE_init();

    while (1) {
        SCREEN_SERVICE_update();
    }
    return (0);
}
