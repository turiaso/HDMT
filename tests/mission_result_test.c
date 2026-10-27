#include <assert.h>
#include <stdio.h>
#include "mission_result.h"

int main(void)
{
    assert(MissionResult_resolveInput(FALSE, FALSE) == MISSION_RESULT_WAIT);
    assert(MissionResult_resolveInput(TRUE, FALSE) == MISSION_RESULT_REPLAY);
    assert(MissionResult_resolveInput(FALSE, TRUE) == MISSION_RESULT_RETURN_TO_MENU);
    /* Si se pulsan A y B/START a la vez, volver al menú es la opción más segura. */
    assert(MissionResult_resolveInput(TRUE, TRUE) == MISSION_RESULT_RETURN_TO_MENU);
    puts("PASS: acciones de la pantalla final: esperar, repetir o volver al selector");
    return 0;
}
