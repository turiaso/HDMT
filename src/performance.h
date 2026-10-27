#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <genesis.h>
#include "game_config.h"

typedef enum
{
    PERF_EVENT_SCROLL = 0,
    PERF_EVENT_SCROLL_PIXELS,
    PERF_EVENT_SAT_UPDATE,
    PERF_EVENT_FOG_REFRESH,
    PERF_EVENT_FOG_CELLS,
    PERF_EVENT_FOG_ROW_RUNS,
    PERF_EVENT_ROUTE_SEARCH,
    PERF_EVENT_PATH_NODES,
    PERF_EVENT_REACHABILITY_BUILD,
    PERF_EVENT_UNIT_RECORDS,
    PERF_EVENT_AI_CANDIDATES,
    PERF_EVENT_AI_MOVE,
    PERF_EVENT_AI_ATTACK,
    PERF_EVENT_COUNT
} PerformanceEvent;

typedef enum
{
    PERF_PHASE_IDLE = 0,
    PERF_PHASE_PLAYER_SCROLL,
    PERF_PHASE_PLAYER_CURSOR,
    PERF_PHASE_PLAYER_MOVE,
    PERF_PHASE_AI_DECISION,
    PERF_PHASE_AI_CAMERA,
    PERF_PHASE_AI_MOVE,
    PERF_PHASE_AI_MOVE_CAMERA,
    PERF_PHASE_CAMERA_RETURN,
    PERF_PHASE_COUNT
} PerformancePhase;

#if PERF_DIAGNOSTICS
void Performance_setPhase(PerformancePhase phase);
void Performance_count(PerformanceEvent event, u32 amount);
void Performance_tick(void);
#else
#define Performance_setPhase(phase) ((void)0)
#define Performance_count(event, amount) ((void)0)
#define Performance_tick() ((void)0)
#endif

#endif
