#include "cell_events_selftest.h"

#if CELL_EVENTS_SELF_TEST
#include "map_data.h"
#include "cell_events.h"

#define SELFTEST_EVENT_CAPACITY 16

static u8 eventLog[SELFTEST_EVENT_CAPACITY];
static u8 eventLogCount;
static bool eventContextOK;
static MapData *expectedMap;

static void recordSelfTestEvent(const CellEventContext *context)
{
    if (context == NULL)
    {
        eventContextOK = FALSE;
        return;
    }

    if ((context->map != expectedMap) ||
        (context->cell != MapData_getCell(expectedMap, context->x, context->y)) ||
        (context->source != CELL_EVENT_SOURCE_CURSOR))
        eventContextOK = FALSE;

    if (eventLogCount >= SELFTEST_EVENT_CAPACITY)
    {
        eventContextOK = FALSE;
        return;
    }

    eventLog[eventLogCount++] = (u8)context->type;

    if ((context->type == CELL_EVENT_BEFORE_ENTER) &&
        (context->x == 0) &&
        ((context->otherX != CELL_EVENT_NO_COORDINATE) ||
         (context->otherY != CELL_EVENT_NO_COORDINATE)))
        eventContextOK = FALSE;

    if ((context->type == CELL_EVENT_BEFORE_ENTER) && (context->x == 1) &&
        ((context->otherX != 0) || (context->otherY != 0)))
        eventContextOK = FALSE;
}

static const CellEventSet selfTestEventSets[] =
{
    { { 0 } },
    { {
        [CELL_EVENT_BEFORE_LEAVE] = recordSelfTestEvent,
        [CELL_EVENT_LEAVE] = recordSelfTestEvent,
        [CELL_EVENT_BEFORE_ENTER] = recordSelfTestEvent,
        [CELL_EVENT_ENTER] = recordSelfTestEvent,
        [CELL_EVENT_STAY] = recordSelfTestEvent,
        [CELL_EVENT_CLICK] = recordSelfTestEvent,
        [CELL_EVENT_INTERACT] = recordSelfTestEvent,
        [CELL_EVENT_UNIT_ENTER] = recordSelfTestEvent,
        [CELL_EVENT_UNIT_LEAVE] = recordSelfTestEvent,
        [CELL_EVENT_TURN_START] = recordSelfTestEvent,
        [CELL_EVENT_TURN_END] = recordSelfTestEvent
    } }
};

bool CellEvents_runSelfTest(void)
{
    static const u8 expectedEvents[] =
    {
        CELL_EVENT_BEFORE_ENTER,
        CELL_EVENT_ENTER,
        CELL_EVENT_STAY,
        CELL_EVENT_BEFORE_LEAVE,
        CELL_EVENT_LEAVE,
        CELL_EVENT_BEFORE_ENTER,
        CELL_EVENT_ENTER,
        CELL_EVENT_STAY,
        CELL_EVENT_CLICK,
        CELL_EVENT_BEFORE_LEAVE,
        CELL_EVENT_LEAVE
    };
    CellData cells[3];
    MapData map = { .width = 3, .height = 1, .cells = cells, .cellCapacity = 3 };
    u8 i;

    eventLogCount = 0;
    eventContextOK = TRUE;
    expectedMap = &map;

    for (i = 0; i < 3; i++)
    {
        cells[i].terrain = TERRAIN_PATH;
        cells[i].occupantId = CELL_NO_OCCUPANT;
        cells[i].eventSetId = CELL_EVENT_SET_DEFAULT;
    }

    CellEvents_init(selfTestEventSets,
                    (u8)(sizeof(selfTestEventSets) / sizeof(selfTestEventSets[0])));

    /* Primera entrada -> permanencia -> cambio de celda -> click. */
    CellEvents_updateHover(&map, 0, 0, FALSE);
    CellEvents_updateHover(&map, 0, 0, FALSE);
    CellEvents_updateHover(&map, 1, 0, FALSE);
    CellEvents_updateHover(&map, 1, 0, TRUE);

    /* Una celda sin perfil no llama callbacks, ni siquiera en click. */
    cells[2].eventSetId = CELL_EVENT_SET_NONE;
    CellEvents_updateHover(&map, 2, 0, TRUE);
    CellEvents_updateHover(&map, 3, 0, FALSE); /* fuera de mapa: cierra hover */

    if (!eventContextOK || (eventLogCount != sizeof(expectedEvents)))
        return FALSE;

    for (i = 0; i < eventLogCount; i++)
    {
        if (eventLog[i] != expectedEvents[i])
            return FALSE;
    }

    return TRUE;
}
#endif
