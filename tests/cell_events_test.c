#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cell_events.h"

#define LOG_CAPACITY 64

typedef struct
{
    CellEventType type;
    u16 x;
    u16 y;
    u16 otherX;
    u16 otherY;
    MapData *map;
    CellData *cell;
    CellEventSource source;
} EventRecord;

static EventRecord eventLog[LOG_CAPACITY];
static u16 eventCount;

static void recordEvent(const CellEventContext *context)
{
    assert(context != NULL);
    assert(context->map != NULL);
    assert(context->cell != NULL);
    assert(eventCount < LOG_CAPACITY);

    eventLog[eventCount].type = context->type;
    eventLog[eventCount].x = context->x;
    eventLog[eventCount].y = context->y;
    eventLog[eventCount].otherX = context->otherX;
    eventLog[eventCount].otherY = context->otherY;
    eventLog[eventCount].map = context->map;
    eventLog[eventCount].cell = context->cell;
    eventLog[eventCount].source = context->source;
    eventCount++;
}

static const CellEventSet eventSets[] =
{
    { { 0 } }, /* 0 = sin callbacks */
    { { [CELL_EVENT_BEFORE_LEAVE] = recordEvent,
        [CELL_EVENT_LEAVE] = recordEvent,
        [CELL_EVENT_BEFORE_ENTER] = recordEvent,
        [CELL_EVENT_ENTER] = recordEvent,
        [CELL_EVENT_STAY] = recordEvent,
        [CELL_EVENT_CLICK] = recordEvent,
        [CELL_EVENT_INTERACT] = recordEvent,
        [CELL_EVENT_UNIT_ENTER] = recordEvent,
        [CELL_EVENT_UNIT_LEAVE] = recordEvent,
        [CELL_EVENT_TURN_START] = recordEvent,
        [CELL_EVENT_TURN_END] = recordEvent } },
    { { [CELL_EVENT_CLICK] = recordEvent } } /* Solo click */
};

static void clearLog(void)
{
    eventCount = 0;
    memset(eventLog, 0, sizeof(eventLog));
}

static void expectEvent(u16 index, CellEventType type, MapData *map,
                        u16 x, u16 y, u16 otherX, u16 otherY)
{
    assert(index < eventCount);
    assert(eventLog[index].type == type);
    assert(eventLog[index].map == map);
    assert(eventLog[index].x == x);
    assert(eventLog[index].y == y);
    if ((eventLog[index].otherX != otherX) || (eventLog[index].otherY != otherY))
        fprintf(stderr, "event[%u] type=%d at (%u,%u): other=(%u,%u), expected=(%u,%u)\n",
                index, (int)eventLog[index].type, eventLog[index].x,
                eventLog[index].y, eventLog[index].otherX, eventLog[index].otherY,
                otherX, otherY);
    assert(eventLog[index].otherX == otherX);
    assert(eventLog[index].otherY == otherY);
    assert(eventLog[index].cell == MapData_getCell(map, x, y));
}

static void testCursorLifecycle(void)
{
    CellData cells[3] =
    {
        { TERRAIN_PATH, CELL_NO_OCCUPANT, 1, 0 },
        { TERRAIN_FOREST, CELL_NO_OCCUPANT, 1, 0 },
        { TERRAIN_ROCK, CELL_NO_OCCUPANT, 1, 0 }
    };
    MapData map = { .width = 3, .height = 1, .cells = cells, .cellCapacity = 3 };
    const u16 noCoord = CELL_EVENT_NO_COORDINATE;

    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    clearLog();

    /* Primera celda: beforeEnter -> enter; no hay celda anterior válida. */
    CellEvents_updateHover(&map, 0, 0, FALSE);
    assert(eventCount == 2);
    expectEvent(0, CELL_EVENT_BEFORE_ENTER, &map, 0, 0, noCoord, noCoord);
    expectEvent(1, CELL_EVENT_ENTER, &map, 0, 0, noCoord, noCoord);
    assert(eventLog[0].source == CELL_EVENT_SOURCE_CURSOR);
    assert(eventLog[1].source == CELL_EVENT_SOURCE_CURSOR);

    /* Permanencia: stay una vez por actualización/fotograma. */
    CellEvents_updateHover(&map, 0, 0, FALSE);
    assert(eventCount == 3);
    expectEvent(2, CELL_EVENT_STAY, &map, 0, 0, 0, 0);

    /* Click en la misma celda: stay y luego click. */
    CellEvents_updateHover(&map, 0, 0, TRUE);
    assert(eventCount == 5);
    expectEvent(3, CELL_EVENT_STAY, &map, 0, 0, 0, 0);
    expectEvent(4, CELL_EVENT_CLICK, &map, 0, 0, 0, 0);
    assert(eventLog[2].source == CELL_EVENT_SOURCE_CURSOR);
    assert(eventLog[3].source == CELL_EVENT_SOURCE_CURSOR);
    assert(eventLog[4].source == CELL_EVENT_SOURCE_CURSOR);

    /* Cambio: beforeLeave -> leave -> beforeEnter -> enter. */
    CellEvents_updateHover(&map, 1, 0, TRUE);
    assert(eventCount == 10);
    expectEvent(5, CELL_EVENT_BEFORE_LEAVE, &map, 0, 0, 1, 0);
    expectEvent(6, CELL_EVENT_LEAVE, &map, 0, 0, 1, 0);
    expectEvent(7, CELL_EVENT_BEFORE_ENTER, &map, 1, 0, 0, 0);
    expectEvent(8, CELL_EVENT_ENTER, &map, 1, 0, 0, 0);
    expectEvent(9, CELL_EVENT_CLICK, &map, 1, 0, 1, 0);
    for (u16 i = 5; i < 10; i++)
        assert(eventLog[i].source == CELL_EVENT_SOURCE_CURSOR);

    /* Eventos directos no ligados al cursor (unidad, interacción, turno). */
    clearLog();
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_INTERACT, 2, 0);
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_UNIT_ENTER, 0, 0);
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_UNIT_LEAVE, 2, 0);
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_TURN_START, 0, 0);
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_TURN_END, 0, 0);
    assert(eventCount == 5);
    expectEvent(0, CELL_EVENT_INTERACT, &map, 1, 0, 2, 0);
    expectEvent(1, CELL_EVENT_UNIT_ENTER, &map, 1, 0, 0, 0);
    expectEvent(2, CELL_EVENT_UNIT_LEAVE, &map, 1, 0, 2, 0);
    expectEvent(3, CELL_EVENT_TURN_START, &map, 1, 0, 0, 0);
    expectEvent(4, CELL_EVENT_TURN_END, &map, 1, 0, 0, 0);
    for (u16 i = 0; i < eventCount; i++)
        assert(eventLog[i].source == CELL_EVENT_SOURCE_SYSTEM);

    puts("  PASS: entrada, permanencia, salida y orden exacto");
    puts("  PASS: contexto de celda y coordenadas anterior/opuesta");
    puts("  PASS: interact, eventos de unidad y eventos de turno");
}

static void testProfilesAndGuards(void)
{
    CellData cells[2] =
    {
        { TERRAIN_PATH, CELL_NO_OCCUPANT, 1, 0 },
        { TERRAIN_ROCK, CELL_NO_OCCUPANT, 1, 0 }
    };
    MapData map = { .width = 2, .height = 1, .cells = cells, .cellCapacity = 2 };
    const u16 noCoord = CELL_EVENT_NO_COORDINATE;

    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    clearLog();

    assert(MapData_setEventSet(&map, 1, 0, 2));
    assert(!MapData_setEventSet(&map, 2, 0, 1));
    CellEvents_updateHover(&map, 1, 0, TRUE);
    assert(eventCount == 1);
    expectEvent(0, CELL_EVENT_CLICK, &map, 1, 0, 1, 0);

    /* Perfil cero desactiva eventos sin romper el seguimiento del cursor. */
    assert(MapData_setEventSet(&map, 1, 0, CELL_EVENT_SET_NONE));
    clearLog();
    CellEvents_updateHover(&map, 1, 0, TRUE);
    assert(eventCount == 0);
    CellEvents_dispatchAt(&map, 1, 0, CELL_EVENT_UNIT_ENTER, 0, 0);
    assert(eventCount == 0);

    /* Coordenadas inválidas cierran la celda actual con coordenadas centinela. */
    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    clearLog();
    CellEvents_updateHover(&map, 0, 0, FALSE);
    clearLog();
    CellEvents_updateHover(&map, 99, 0, FALSE);
    assert(eventCount == 2);
    expectEvent(0, CELL_EVENT_BEFORE_LEAVE, &map, 0, 0, noCoord, noCoord);
    expectEvent(1, CELL_EVENT_LEAVE, &map, 0, 0, noCoord, noCoord);

    /* Null, tipo negativo, tipo fuera del enum y coordenadas inválidas no rompen nada. */
    clearLog();
    CellEvents_dispatchAt(&map, 0, 0, (CellEventType)-1, 0, 0);
    CellEvents_dispatchAt(&map, 0, 0, (CellEventType)CELL_EVENT_COUNT, 0, 0);
    CellEvents_dispatchAt(&map, 99, 0, CELL_EVENT_CLICK, 0, 0);
    CellEvents_dispatchAtFrom(&map, 0, 0, CELL_EVENT_CLICK, 0, 0,
                              (CellEventSource)99);
    CellEvents_dispatchAt(NULL, 0, 0, CELL_EVENT_CLICK, 0, 0);
    CellEvents_updateHover(NULL, 0, 0, TRUE);
    assert(eventCount == 0);

    puts("  PASS: perfiles compartidos y perfil sin eventos");
    puts("  PASS: coordenadas inválidas, mapa nulo y tipos inválidos");
}

static void testMapSwitch(void)
{
    CellData aCells[1] = { { TERRAIN_PATH, CELL_NO_OCCUPANT, 1, 0 } };
    CellData bCells[1] = { { TERRAIN_PATH, CELL_NO_OCCUPANT, 1, 0 } };
    MapData mapA = { .width = 1, .height = 1, .cells = aCells, .cellCapacity = 1 };
    MapData mapB = { .width = 1, .height = 1, .cells = bCells, .cellCapacity = 1 };
    const u16 noCoord = CELL_EVENT_NO_COORDINATE;

    CellEvents_init(eventSets, (u8)(sizeof(eventSets) / sizeof(eventSets[0])));
    clearLog();
    CellEvents_updateHover(&mapA, 0, 0, FALSE);
    clearLog();
    CellEvents_updateHover(&mapB, 0, 0, FALSE);
    assert(eventCount == 4);
    expectEvent(0, CELL_EVENT_BEFORE_LEAVE, &mapA, 0, 0, noCoord, noCoord);
    expectEvent(1, CELL_EVENT_LEAVE, &mapA, 0, 0, noCoord, noCoord);
    expectEvent(2, CELL_EVENT_BEFORE_ENTER, &mapB, 0, 0, noCoord, noCoord);
    expectEvent(3, CELL_EVENT_ENTER, &mapB, 0, 0, noCoord, noCoord);

    puts("  PASS: el cambio de mapa cierra la celda anterior");
}

int main(void)
{
    testCursorLifecycle();
    testProfilesAndGuards();
    testMapSwitch();
    puts("PASS: todas las pruebas del sistema de eventos");
    return 0;
}
