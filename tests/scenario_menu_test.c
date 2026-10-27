#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scenario_menu.h"
#include "scenario.h"
#include "team.h"

#define TEST_SCENARIO_COUNT 4u
#define DRAW_RECORD_CAPACITY 96u
#define JOY_CAPACITY 16u

typedef struct
{
    char text[40];
    u16 x;
    u16 y;
} DrawRecord;

static ScenarioDefinition scenarios[TEST_SCENARIO_COUNT] =
{
    { .name = "PUNTO DE CONTROL", .menuDescription = "CAPTURA EL OBJETIVO",
      .menuSummary = "120 UNIDADES" },
    { .name = "DEFENSA DEL PUENTE", .menuDescription = "MANTEN EL PUENTE",
      .menuSummary = "24 UNIDADES" },
    { .name = "PUENTE DEL DESFILADERO", .menuDescription = "MANTEN EL PUENTE",
      .menuSummary = "12 UNIDADES" },
    { .name = "CRUCE DE LAS RUINAS", .menuDescription = "MANTEN EL CRUCE",
      .menuSummary = "12 UNIDADES" }
};
static DrawRecord drawRecords[DRAW_RECORD_CAPACITY];
static u16 drawCount;
static u16 joyValues[JOY_CAPACITY];
static u8 joyLength;
static u8 joyIndex;

const ScenarioDefinition *Scenario_get(u8 index)
{
    return (index < TEST_SCENARIO_COUNT) ? &scenarios[index] : NULL;
}

u8 Scenario_getCount(void)
{
    return TEST_SCENARIO_COUNT;
}

u8 Scenario_cycleIndex(u8 current, s8 direction)
{
    if (current >= TEST_SCENARIO_COUNT) current = 0;
    if (direction < 0)
        return (current == 0) ? (TEST_SCENARIO_COUNT - 1u)
                              : (u8)(current - 1u);
    if (direction > 0)
        return ((u8)(current + 1u) >= TEST_SCENARIO_COUNT)
            ? 0 : (u8)(current + 1u);
    return current;
}

void Team_initPalettes(void) { }
void VDP_setScreenWidth320(void) { }
void VDP_setScreenHeight224(void) { }
void VDP_setScrollingMode(u16 horizontal, u16 vertical)
{ (void)horizontal; (void)vertical; }
void VDP_clearPlane(u16 plane, bool wait) { (void)plane; (void)wait; }
void VDP_setHorizontalScroll(u16 plane, s16 value)
{ (void)plane; (void)value; }
void VDP_setVerticalScroll(u16 plane, s16 value)
{ (void)plane; (void)value; }
void VDP_setTextPlane(u16 plane) { (void)plane; }
void VDP_setTextPalette(u16 palette) { (void)palette; }
void VDP_setTextPriority(u16 priority) { (void)priority; }

void VDP_drawTextBGFill(u16 plane, const char *text, u16 x, u16 y, u16 len)
{
    DrawRecord *record;
    (void)plane;
    (void)len;
    assert(drawCount < DRAW_RECORD_CAPACITY);
    record = &drawRecords[drawCount++];
    (void)snprintf(record->text, sizeof(record->text), "%s", text);
    record->x = x;
    record->y = y;
}

void JOY_init(void) { joyIndex = 0; }
u16 JOY_readJoypad(u16 joy)
{
    (void)joy;
    if (joyIndex >= joyLength) return 0;
    return joyValues[joyIndex++];
}
void SYS_doVBlankProcess(void) { }

static void setJoySequence(const u16 *values, u8 count)
{
    assert(count <= JOY_CAPACITY);
    memset(joyValues, 0, sizeof(joyValues));
    if (count > 0) memcpy(joyValues, values, (size_t)count * sizeof(values[0]));
    joyLength = count;
    joyIndex = 0;
}

static bool drewTextAt(const char *text, u16 x, u16 y)
{
    u16 i;
    for (i = 0; i < drawCount; i++)
        if ((drawRecords[i].x == x) && (drawRecords[i].y == y) &&
            (strcmp(drawRecords[i].text, text) == 0))
            return TRUE;
    return FALSE;
}

static void resetDraws(void)
{
    memset(drawRecords, 0, sizeof(drawRecords));
    drawCount = 0;
}

static void testMenuRestoresLastSelectedScenario(void)
{
    const u16 input[] = { BUTTON_A, 0 };

    resetDraws();
    setJoySequence(input, (u8)(sizeof(input) / sizeof(input[0])));
    assert(ScenarioMenu_showFrom(2) == 2);
    assert(drewTextAt(">", 4, 13));
    assert(drewTextAt("PUENTE DEL DESFILADERO", 6, 13));
}

static void testInvalidRememberedIndexFallsBackToFirstScenario(void)
{
    const u16 input[] = { BUTTON_A, 0 };

    resetDraws();
    setJoySequence(input, (u8)(sizeof(input) / sizeof(input[0])));
    assert(ScenarioMenu_showFrom(99) == 0);
    assert(drewTextAt(">", 4, 5));
    assert(!drewTextAt(">", 4, 13));
}

static void testNavigationWrapsFromRememberedScenario(void)
{
    const u16 input[] = { BUTTON_DOWN, 0, BUTTON_A, 0 };

    resetDraws();
    setJoySequence(input, (u8)(sizeof(input) / sizeof(input[0])));
    assert(ScenarioMenu_showFrom(3) == 0);
    assert(drewTextAt(">", 4, 5));
}

static void testFourthScenarioFitsWithoutOverwritingControls(void)
{
    const u16 input[] = { BUTTON_A, 0 };

    resetDraws();
    setJoySequence(input, (u8)(sizeof(input) / sizeof(input[0])));
    assert(ScenarioMenu_showFrom(3) == 3);
    assert(drewTextAt(">", 4, 17));
    assert(drewTextAt("CRUCE DE LAS RUINAS", 6, 17));
    assert(drewTextAt("MANTEN EL CRUCE", 8, 18));
    assert(drewTextAt("12 UNIDADES", 8, 19));
    assert(drewTextAt("ARRIBA/ABAJO: ELEGIR", 8, 22));
    assert(drewTextAt("A O START: COMENZAR", 8, 24));
}

int main(void)
{
    testMenuRestoresLastSelectedScenario();
    testInvalidRememberedIndexFallsBackToFirstScenario();
    testNavigationWrapsFromRememberedScenario();
    testFourthScenarioFitsWithoutOverwritingControls();
    puts("PASS: selector de cuatro misiones, navegación circular y texto sin solapamientos");
    return 0;
}
