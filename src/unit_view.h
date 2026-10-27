#ifndef UNIT_VIEW_H
#define UNIT_VIEW_H

#include <genesis.h>
#include <sprite_eng.h>
#include "camera_cursor.h"
#include "unit_controller.h"
#include "team.h"

#define UNIT_VIEW_MAX_UNITS GAME_MAX_UNITS
#define UNIT_VIEW_POSITION_INVALID ((s16)-32768)
#define UNIT_VIEW_POSITION_HIDDEN  ((s16)-32767)
#define UNIT_VIEW_MAX_MAP_SIDE     GAMEPLAY_MAP_MAX_SIDE
#define UNIT_VIEW_NO_INDEX         0xFFu
#define UNIT_VIEW_PENDING_WORDS     ((UNIT_VIEW_MAX_UNITS + 15u) / 16u)
#define UNIT_VIEW_PRIORITY_LEVELS   5u

typedef struct
{
    Sprite **sprites;
    u8 capacity; /* Capacidad lógica, no número de sprites hardware. */
    u8 activeSpriteCount;
    /* Índices compactos: el scroll estable no necesita escanear 120 unidades. */
    u8 activeSpriteIndices[UNIT_VIEW_MAX_UNITS];
    u8 activeSpriteIndexCount;

    /* Índice espacial por fila/columna: el scroll consulta solo las celdas
       que entran por el borde, no inspecciona todo el ejército cada frame. */
    bool spatialIndexValid;
    u16 spatialMapWidth;
    u16 spatialMapHeight;
    u8 unitColumnHead[UNIT_VIEW_MAX_MAP_SIDE];
    u8 unitRowHead[UNIT_VIEW_MAX_MAP_SIDE];
    u8 unitNextColumn[UNIT_VIEW_MAX_UNITS];
    u8 unitPrevColumn[UNIT_VIEW_MAX_UNITS];
    u8 unitNextRow[UNIT_VIEW_MAX_UNITS];
    u8 unitPrevRow[UNIT_VIEW_MAX_UNITS];

    /* Candidatos pendientes indexados por prioridad: elegir/retirar un candidato
       no requiere barrer ni desplazar la cola completa en cada frame de scroll. */
    u16 pendingSpritePriorityBits[UNIT_VIEW_PRIORITY_LEVELS]
        [UNIT_VIEW_PENDING_WORDS];
    u8 pendingSpriteCandidatePriority[UNIT_VIEW_MAX_UNITS];
    bool pendingSpriteCandidateQueued[UNIT_VIEW_MAX_UNITS];
    u8 pendingSpriteCandidateCount;

    bool spriteAllocationFailed;
    bool spriteBudgetRefreshPending;
    const MapData *visibilityMap;
    TeamId viewerTeam;
    bool fogEnabled;
    bool sharedFogVision;

    /* Una unidad fuera de pantalla no conserva una instancia en SPRITE ENG. */
    s16 lastScreenX[UNIT_VIEW_MAX_UNITS];
    s16 lastScreenY[UNIT_VIEW_MAX_UNITS];

    /* Estado lógico cacheado: evita recalcular visibilidad y posición sin cambios. */
    bool stateCacheValid[UNIT_VIEW_MAX_UNITS];
    bool lastUnitAlive[UNIT_VIEW_MAX_UNITS];
    u16 lastUnitX[UNIT_VIEW_MAX_UNITS];
    u16 lastUnitY[UNIT_VIEW_MAX_UNITS];
    u8 lastUnitTeam[UNIT_VIEW_MAX_UNITS];
    bool lastFogVisible[UNIT_VIEW_MAX_UNITS];

    s16 lastCameraX;
    s16 lastCameraY;
    bool cameraCacheValid;
    /* Detecta el final de una animación para forzar el snap a la casilla lógica. */
    bool lastControllerMoving;
    s16 lastMovingIndex;
    bool forceRefresh;
    bool fogRefreshPending;
    /* Índices de celdas cuya visión cambió en la última actualización de niebla. */
    const u16 *fogDirtyCellIndices;
    u16 fogDirtyCellCount;
    u16 fogDirtyMapWidth;
    bool fogDirtyCellsPending;
} UnitView;

bool UnitView_init(UnitView *view, const UnitController *controller,
                   Sprite **spriteStorage, u8 spriteCapacity);
/* Libera las referencias creadas; puede llamarse más de una vez. */
void UnitView_release(UnitView *view);
/* Devuelve TRUE solo si hay cambios de sprites que exijan SPR_update(). */
bool UnitView_update(UnitView *view, const UnitController *controller,
                     const CameraCursorState *camera);
/* Fuerza una pasada completa tras un cambio de estado visual (p. ej. combate). */
void UnitView_invalidate(UnitView *view);
void UnitView_setFogContext(UnitView *view, const MapData *map,
                            TeamId viewerTeam, bool enabled,
                            bool sharedVision);
/* Programa un refresco selectivo de unidades sobre las celdas cambiadas. */
void UnitView_setFogDirtyCells(UnitView *view, const u16 *cellIndices,
                               u16 cellCount, u16 mapWidth);

#endif
