#ifndef CELL_EVENTS_H
#define CELL_EVENTS_H

#include <genesis.h>
#include "map_data.h"

/* ID 0 reserva una celda sin callbacks; el perfil predeterminado es el 1. */
#define CELL_EVENT_SET_NONE      0
#define CELL_EVENT_SET_DEFAULT   1
#define CELL_EVENT_NO_COORDINATE 0xFFFFu /* No existe una celda anterior/opuesta. */
#define CELL_EVENT_TEAM_NONE      0xFFu

typedef enum
{
    CELL_EVENT_BEFORE_LEAVE = 0,
    CELL_EVENT_LEAVE,
    CELL_EVENT_BEFORE_ENTER,
    CELL_EVENT_ENTER,
    CELL_EVENT_STAY,
    CELL_EVENT_CLICK,
    CELL_EVENT_INTERACT,
    CELL_EVENT_UNIT_ENTER,
    CELL_EVENT_UNIT_LEAVE,
    CELL_EVENT_TURN_START,
    CELL_EVENT_TURN_END,
    CELL_EVENT_COUNT
} CellEventType;

typedef enum
{
    CELL_EVENT_SOURCE_SYSTEM = 0,
    CELL_EVENT_SOURCE_CURSOR,
    CELL_EVENT_SOURCE_UNIT,
    CELL_EVENT_SOURCE_COUNT
} CellEventSource;

typedef struct
{
    MapData *map;
    CellData *cell;
    CellEventSource source;
    u16 x;
    u16 y;
    u16 otherX; /* Coordenadas de la celda opuesta, cuando proceda. */
    u16 otherY;
    CellEventType type;
    u8 teamId;       /* TEAM_NONE para eventos no asociados a un equipo. */
    u16 turnNumber;  /* 0 para eventos que no pertenecen a un turno concreto. */
} CellEventContext;

typedef void (*CellEventHandler)(const CellEventContext *context);

typedef struct
{
    CellEventHandler handlers[CELL_EVENT_COUNT];
} CellEventSet;

/*
 * La tabla es constante y compartida; no se copia a RAM.
 * El ciclo de entrada/salida conserva el orden:
 * beforeLeave -> leave -> beforeEnter -> enter.
 * CellEventContext.source distingue CURSOR y UNIT. Para una unidad, estos
 * cuatro eventos solo se emiten tras validar el paso; UNIT_LEAVE/UNIT_ENTER
 * son notificaciones adicionales posteriores a la actualización lógica.
 * Los callbacks de transición no vetan el movimiento ni deben cambiar el
 * terreno o la ocupación durante ese mismo tránsito.
 */
void CellEvents_init(const CellEventSet *sets, u8 setCount);
void CellEvents_reset(void);
void CellEvents_updateHover(MapData *map, u16 x, u16 y, bool clicked);
/* Despacho directo para interacciones/sistema, o con origen explícito. */
void CellEvents_dispatchAt(MapData *map, u16 x, u16 y, CellEventType type,
                           u16 otherX, u16 otherY);
void CellEvents_dispatchAtFrom(MapData *map, u16 x, u16 y,
                               CellEventType type, u16 otherX, u16 otherY,
                               CellEventSource source);
void CellEvents_dispatchAtForTeam(MapData *map, u16 x, u16 y,
                                  CellEventType type, u16 otherX, u16 otherY,
                                  CellEventSource source, u8 teamId,
                                  u16 turnNumber);
/* Emite un evento de turno sobre las celdas que tengan un callback registrado. */
void CellEvents_dispatchTurn(MapData *map, CellEventType type, u8 teamId,
                             u16 turnNumber);

#endif
