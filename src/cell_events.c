#include "cell_events.h"

/*
 * Despacho de eventos compartidos por perfil de celda. Los datos de CellData contienen
 * índices compactos, nunca punteros a callbacks; este módulo posee la tabla de perfiles.
 */

static const CellEventSet *eventSets;
static u8 eventSetCount;
static bool hoveredValid;
static u16 hoveredX;
static u16 hoveredY;
static MapData *hoveredMap;

void CellEvents_init(const CellEventSet *sets, u8 setCount)
{
    eventSets = sets;
    eventSetCount = setCount;
    CellEvents_reset();
}

void CellEvents_reset(void)
{
    hoveredValid = FALSE;
    hoveredX = hoveredY = 0;
    hoveredMap = NULL;
}

void CellEvents_dispatchAtForTeam(MapData *map, u16 x, u16 y,
                                  CellEventType type, u16 otherX, u16 otherY,
                                  CellEventSource source, u8 teamId,
                                  u16 turnNumber)
{
    CellData *cell;
    const CellEventSet *set;
    CellEventHandler handler;
    CellEventContext context;
    u8 setId;

    if ((map == NULL) || ((s32)type < 0) || (type >= CELL_EVENT_COUNT) ||
        ((s32)source < 0) || (source >= CELL_EVENT_SOURCE_COUNT))
        return;
    cell = MapData_getCell(map, x, y);
    if (cell == NULL) return;

    setId = cell->eventSetId;
    if ((setId == CELL_EVENT_SET_NONE) || (setId >= eventSetCount) ||
        (eventSets == NULL)) return;

    set = &eventSets[setId];
    handler = set->handlers[type];
    if (handler == NULL) return;

    context.map = map;
    context.cell = cell;
    context.source = source;
    context.x = x;
    context.y = y;
    context.otherX = otherX;
    context.otherY = otherY;
    context.type = type;
    context.teamId = teamId;
    context.turnNumber = turnNumber;
    handler(&context);
}

void CellEvents_dispatchAtFrom(MapData *map, u16 x, u16 y,
                               CellEventType type, u16 otherX, u16 otherY,
                               CellEventSource source)
{
    CellEvents_dispatchAtForTeam(map, x, y, type, otherX, otherY, source,
                                 CELL_EVENT_TEAM_NONE, 0);
}

void CellEvents_dispatchAt(MapData *map, u16 x, u16 y, CellEventType type,
                           u16 otherX, u16 otherY)
{
    CellEvents_dispatchAtFrom(map, x, y, type, otherX, otherY,
                              CELL_EVENT_SOURCE_SYSTEM);
}

void CellEvents_dispatchTurn(MapData *map, CellEventType type, u8 teamId,
                             u16 turnNumber)
{
    u16 x;
    u16 y;

    if ((map == NULL) ||
        ((type != CELL_EVENT_TURN_START) && (type != CELL_EVENT_TURN_END)))
        return;

    for (y = 0; y < map->height; y++)
        for (x = 0; x < map->width; x++)
            CellEvents_dispatchAtForTeam(map, x, y, type,
                                         CELL_EVENT_NO_COORDINATE,
                                         CELL_EVENT_NO_COORDINATE,
                                         CELL_EVENT_SOURCE_SYSTEM, teamId,
                                         turnNumber);
}

static void dispatchCursorAt(MapData *map, u16 x, u16 y,
                             CellEventType type, u16 otherX, u16 otherY)
{
    CellEvents_dispatchAtFrom(map, x, y, type, otherX, otherY,
                              CELL_EVENT_SOURCE_CURSOR);
}

static void dispatchLeave(MapData *map, u16 x, u16 y, u16 otherX, u16 otherY)
{
    dispatchCursorAt(map, x, y, CELL_EVENT_BEFORE_LEAVE, otherX, otherY);
    dispatchCursorAt(map, x, y, CELL_EVENT_LEAVE, otherX, otherY);
}

void CellEvents_updateHover(MapData *map, u16 x, u16 y, bool clicked)
{
    u16 previousX;
    u16 previousY;
    CellData *cell = (map != NULL) ? MapData_getCell(map, x, y) : NULL;

    /* Abandonar el mapa o una coordenada inválida cierra la celda activa. */
    if (cell == NULL)
    {
        if (hoveredValid)
        {
            dispatchLeave(hoveredMap, hoveredX, hoveredY,
                          CELL_EVENT_NO_COORDINATE, CELL_EVENT_NO_COORDINATE);
        }
        CellEvents_reset();
        return;
    }

    if (!hoveredValid || (hoveredMap != map) ||
        (x != hoveredX) || (y != hoveredY))
    {
        previousX = CELL_EVENT_NO_COORDINATE;
        previousY = CELL_EVENT_NO_COORDINATE;

        if (hoveredValid)
        {
            if (hoveredMap == map)
            {
                previousX = hoveredX;
                previousY = hoveredY;
                dispatchLeave(map, hoveredX, hoveredY, x, y);
            }
            else
            {
                dispatchLeave(hoveredMap, hoveredX, hoveredY,
                              CELL_EVENT_NO_COORDINATE, CELL_EVENT_NO_COORDINATE);
            }
        }

        dispatchCursorAt(map, x, y, CELL_EVENT_BEFORE_ENTER,
                         previousX, previousY);
        dispatchCursorAt(map, x, y, CELL_EVENT_ENTER,
                         previousX, previousY);
        hoveredMap = map;
        hoveredX = x;
        hoveredY = y;
        hoveredValid = TRUE;
    }
    else
    {
        /*
         * STAY puede ejecutarse cada frame, pero las celdas sin perfil o sin
         * callback STAY no necesitan entrar de nuevo en la ruta de despacho.
         * La celda ya se obtuvo arriba para validar coordenadas.
         */
        const u8 setId = cell->eventSetId;
        if ((eventSets != NULL) && (setId != CELL_EVENT_SET_NONE) &&
            (setId < eventSetCount) &&
            (eventSets[setId].handlers[CELL_EVENT_STAY] != NULL))
            dispatchCursorAt(map, x, y, CELL_EVENT_STAY, x, y);
    }

    if (clicked)
        dispatchCursorAt(map, x, y, CELL_EVENT_CLICK, x, y);
}
