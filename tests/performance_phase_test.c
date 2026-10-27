#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <genesis.h>
#include "performance.h"

static bool sawScrollPhase;
static bool sawAIMoveCameraPhase;
static bool sawScrollCounts;
static bool sawAIMoveRecords;

void KLog(const char *text)
{
    if (strcmp(text, "PERF PHASE PLAYER_SCROLL") == 0)
        sawScrollPhase = TRUE;
    if (strcmp(text, "PERF PHASE AI_MOVE_CAMERA") == 0)
        sawAIMoveCameraPhase = TRUE;
}

void KLog_U2(char *labelA, u32 valueA, char *labelB, u32 valueB)
{
    if (strcmp(labelA, "PERF frames / scroll calls") == 0 &&
        valueA == 120u && strcmp(labelB, "calls") == 0 && valueB == 12u)
        sawScrollCounts = TRUE;
    if (strcmp(labelA, "PERF unit records / AI candidates") == 0 &&
        valueA == 400u && strcmp(labelB, "candidates") == 0)
        sawAIMoveRecords = TRUE;
}

int main(void)
{
    u16 i;

    Performance_setPhase(PERF_PHASE_PLAYER_SCROLL);
    Performance_count(PERF_EVENT_SCROLL, 12u);
    Performance_count(PERF_EVENT_SCROLL_PIXELS, 180u);
    Performance_count(PERF_EVENT_SAT_UPDATE, 90u);
    for (i = 0; i < 120u; i++) Performance_tick();

    assert(sawScrollPhase);
    assert(sawScrollCounts);

    Performance_setPhase(PERF_PHASE_AI_MOVE_CAMERA);
    Performance_count(PERF_EVENT_UNIT_RECORDS, 400u);
    for (i = 0; i < 120u; i++) Performance_tick();

    assert(sawAIMoveCameraPhase);
    assert(sawAIMoveRecords);
    puts("PASS: muestras separadas por fase y contadores por 120 fotogramas");
    return 0;
}
