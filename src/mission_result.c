#include "mission_result.h"

MissionResultAction MissionResult_resolveInput(bool replayPressed,
                                              bool menuPressed)
{
    if (menuPressed) return MISSION_RESULT_RETURN_TO_MENU;
    if (replayPressed) return MISSION_RESULT_REPLAY;
    return MISSION_RESULT_WAIT;
}
