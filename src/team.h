#ifndef TEAM_H
#define TEAM_H

#include <genesis.h>

#define TEAM_NONE 0xFFu

typedef enum
{
    TEAM_BLUE = 0,
    TEAM_RED,
    TEAM_COUNT
} TeamId;

typedef struct
{
    const char *name;
    u16 paletteLine;
} TeamInfo;

const TeamInfo *Team_getInfo(TeamId team);
void Team_initPalettes(void);

#endif
