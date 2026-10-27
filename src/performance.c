#include <genesis.h>
#include "performance.h"

/*
 * Diagnósticos opcionales de carga de trabajo por fase. Miden contadores de operaciones,
 * no ciclos exactos de CPU; mantener PERF_DIAGNOSTICS desactivado en builds normales.
 */

#if PERF_DIAGNOSTICS
/* Cada informe representa 120 fotogramas de UNA actividad, no una mezcla de
 * reposo, scroll y turno de IA. Los contadores se reinician por fase. */
#define PERF_PHASE_SAMPLE_FRAMES 120u

static u32 counters[PERF_PHASE_COUNT][PERF_EVENT_COUNT];
static u16 phaseFrames[PERF_PHASE_COUNT];
static PerformancePhase currentPhase = PERF_PHASE_IDLE;

static const char *phaseNames[PERF_PHASE_COUNT] =
{
    "PERF PHASE IDLE",
    "PERF PHASE PLAYER_SCROLL",
    "PERF PHASE PLAYER_CURSOR",
    "PERF PHASE PLAYER_MOVE",
    "PERF PHASE AI_DECISION",
    "PERF PHASE AI_CAMERA",
    "PERF PHASE AI_MOVE",
    "PERF PHASE AI_MOVE_CAMERA",
    "PERF PHASE CAMERA_RETURN"
};

void Performance_setPhase(PerformancePhase phase)
{
    currentPhase = ((u16)phase < (u16)PERF_PHASE_COUNT)
        ? phase : PERF_PHASE_IDLE;
}

void Performance_count(PerformanceEvent event, u32 amount)
{
    if (((u16)event < (u16)PERF_EVENT_COUNT) &&
        ((u16)currentPhase < (u16)PERF_PHASE_COUNT))
        counters[currentPhase][event] += amount;
}

static void reportCurrentPhase(void)
{
    const u16 phase = (u16)currentPhase;
    u16 i;

    KLog(phaseNames[phase]);
    KLog_U2("PERF frames / scroll calls",
            phaseFrames[phase],
            "calls", counters[phase][PERF_EVENT_SCROLL]);
    KLog_U2("PERF scroll pixels / SAT",
            counters[phase][PERF_EVENT_SCROLL_PIXELS],
            "updates", counters[phase][PERF_EVENT_SAT_UPDATE]);
    KLog_U2("PERF fog calls / cells",
            counters[phase][PERF_EVENT_FOG_REFRESH],
            "processed", counters[phase][PERF_EVENT_FOG_CELLS]);
    KLog_U2("PERF fog row DMA / routes",
            counters[phase][PERF_EVENT_FOG_ROW_RUNS],
            "searches", counters[phase][PERF_EVENT_ROUTE_SEARCH]);
    KLog_U2("PERF path nodes / range builds",
            counters[phase][PERF_EVENT_PATH_NODES],
            "builds", counters[phase][PERF_EVENT_REACHABILITY_BUILD]);
    KLog_U2("PERF unit records / AI candidates",
            counters[phase][PERF_EVENT_UNIT_RECORDS],
            "candidates", counters[phase][PERF_EVENT_AI_CANDIDATES]);
    KLog_U2("PERF AI moves / attacks",
            counters[phase][PERF_EVENT_AI_MOVE],
            "attacks", counters[phase][PERF_EVENT_AI_ATTACK]);

    for (i = 0; i < PERF_EVENT_COUNT; i++) counters[phase][i] = 0;
    phaseFrames[phase] = 0;
}

void Performance_tick(void)
{
    const u16 phase = (u16)currentPhase;

    if (phase >= (u16)PERF_PHASE_COUNT) return;
    if (++phaseFrames[phase] >= PERF_PHASE_SAMPLE_FRAMES)
        reportCurrentPhase();
}
#endif
