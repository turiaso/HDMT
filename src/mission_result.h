#ifndef MISSION_RESULT_H
#define MISSION_RESULT_H

#include <genesis.h>

typedef enum
{
    MISSION_RESULT_WAIT = 0,
    MISSION_RESULT_REPLAY,
    MISSION_RESULT_RETURN_TO_MENU
} MissionResultAction;

/* Resuelve la elección del panel final; menú tiene prioridad si hay dos botones. */
MissionResultAction MissionResult_resolveInput(bool replayPressed,
                                              bool menuPressed);

#endif
