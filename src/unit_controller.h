#ifndef UNIT_CONTROLLER_H
#define UNIT_CONTROLLER_H

#include <genesis.h>
#include "map_data.h"
#include "pathfinder.h"
#include "unit_types.h"
#include "team.h"

/* Máximo lógico del motor. Los sprites activos tienen un límite separado por hardware. */
#define GAME_MAX_UNITS 128u

typedef struct
{
    u8 id;
    u16 x;
    u16 y;
    u8 movementPointsPerTurn; /* 0 = presupuesto predeterminado del tipo. */
    UnitType type;
    TeamId team;
} UnitStart;

/*
 * Estado autoritativo de una unidad. x/y son coordenadas de celda, nunca píxeles;
 * la posición visual interpolada pertenece a UnitView y no altera la ocupación.
 */
typedef struct
{
    u8 id;
    u16 x;
    u16 y;
    u8 maxMovementPoints;
    u8 movementPoints;
    UnitType type;
    TeamId team;
    u8 maxHealth;
    u8 health;
    u8 attack;
    u8 defense;
    u8 attackRange;
    u8 visionRange;
    u8 repairRange;
    u8 repairAmount;
    bool hasAttackedThisTurn;
    bool hasRepairedThisTurn;
    bool alive;
} Unit;

/*
 * Controlador sin asignaciones dinámicas: map, pathWorkspace y units son prestados
 * por el escenario. No liberar ni sustituir esos buffers mientras el juego está activo.
 * movementRange* es la caché consultada por el cursor; los campos preview* son lectura
 * de destino y pending* conserva la ruta candidata antes de que el jugador la confirme.
 */
typedef struct
{
    MapData *map;
    PathfinderWorkspace *pathWorkspace;
    Unit *units;
    u8 count;
    s16 selectedIndex;
    s16 movingIndex;
    bool moving;
    u16 pathLength;
    u16 pathStep;
    u8 segmentPixels;
    bool previewChecked;
    bool previewRelevant;
    bool previewReachable;
    u16 previewX;
    u16 previewY;
    u16 previewLength;
    u8 previewCost;
    bool pendingRoute;
    u16 pendingX;
    u16 pendingY;
    TeamId activeTeam;
    bool fogEnabled; /* Evita tratar enemigos no visibles como información conocida. */
    u16 turnNumber;
    u16 lastStartedTurnByTeam[TEAM_COUNT]; /* 0 = el equipo aún no ha iniciado un turno. */
    u16 movementStartX;
    u16 movementStartY;
    bool movementRangeValid;
    s16 movementRangeUnitIndex;
    u16 movementRangeUnitX;
    u16 movementRangeUnitY;
    u8 movementRangePoints;
} UnitController;

typedef enum
{
    UNIT_ACTION_INVALID,
    UNIT_ACTION_SELECTED,
    UNIT_ACTION_DESELECTED,
    UNIT_ACTION_ROUTE_READY,
    UNIT_ACTION_CANCELLED,
    UNIT_ACTION_MOVING,
    UNIT_ACTION_MOVED,
    UNIT_ACTION_NO_UNIT,
    UNIT_ACTION_NO_ROUTE,
    UNIT_ACTION_BLOCKED,
    UNIT_ACTION_ENEMY_UNIT,
    UNIT_ACTION_NO_MOVEMENT_POINTS
} UnitActionResult;

/* Requiere que el workspace incluya la máscara reachable para el glow. */
bool UnitController_init(UnitController *controller, MapData *map,
                         Unit *storage, u8 capacity,
                         const UnitStart *starts, u8 count);
/*
 * A prepara una ruta; A de nuevo en el mismo destino la confirma.
 * Una orden confirmada consume la acción de movimiento del turno. Al completarla,
 * la unidad se deselecciona automáticamente; una ruta bloqueada antes de salir
 * del origen permite replantear el camino y conserva los puntos restantes.
 */
UnitActionResult UnitController_activate(UnitController *controller,
                                         u16 x, u16 y);
/* B cancela la planificación y la selección; no interrumpe un movimiento. */
UnitActionResult UnitController_update(UnitController *controller);
UnitActionResult UnitController_cancelOrder(UnitController *controller);
bool UnitController_hasPendingRoute(const UnitController *controller);
/* Vista previa de solo lectura; nunca calcula una ruta ni reconstruye el alcance. */
bool UnitController_updatePreview(UnitController *controller, u16 x, u16 y);
bool UnitController_getPreviewInfo(const UnitController *controller,
                                   bool *reachable, u16 *stepCount,
                                   u8 *totalCost, u8 *availablePoints);
bool UnitController_getPreviewCell(const UnitController *controller,
                                   u16 stepIndex, u16 *x, u16 *y);
bool UnitController_isMoving(const UnitController *controller);
/*
 * Inicia un turno propio una sola vez. Una repetición del mismo equipo y número
 * de turno se rechaza para impedir que una llamada duplicada reponga acciones.
 */
bool UnitController_beginTeamTurn(UnitController *controller, TeamId team,
                                   u16 turnNumber);
/* Activa el filtrado de ocupación enemiga según la visión del equipo activo. */
void UnitController_setFogEnabled(UnitController *controller, bool enabled);
/* Refresco explícito al cambiar selección/presupuesto; nunca se llama desde hover. */
bool UnitController_refreshMovementRange(UnitController *controller);
const u8 *UnitController_getMovementRange(const UnitController *controller);
/* Detiene el movimiento por fin de misión y borra también la selección. */
void UnitController_haltMovement(UnitController *controller);
void UnitController_getRenderPosition(const UnitController *controller,
                                      u8 unitIndex, s32 *pixelX, s32 *pixelY);
u8 UnitController_getCount(const UnitController *controller);
const Unit *UnitController_getUnit(const UnitController *controller, u8 index);
Unit *UnitController_getMutableUnitById(UnitController *controller, u8 id);
const Unit *UnitController_getUnitAt(const UnitController *controller,
                                     u16 x, u16 y);
bool UnitController_removeUnit(UnitController *controller, u8 id);
void UnitController_invalidateMovementRange(UnitController *controller);
const Unit *UnitController_getSelected(const UnitController *controller);
s16 UnitController_getSelectedIndex(const UnitController *controller);
TeamId UnitController_getActiveTeam(const UnitController *controller);
u16 UnitController_getTurnNumber(const UnitController *controller);
const char *UnitController_getActionText(UnitActionResult result);

#endif
