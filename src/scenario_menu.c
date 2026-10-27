#include <genesis.h>

#include "scenario_menu.h"
#include "game_config.h"
#include "scenario.h"
#include "team.h"

/* Dibuja solo contenido estático del menú; no crea sprites ni mapas. */
static void drawScenarioMenu(u8 selectedIndex)
{
    u8 i;
    const u8 count = Scenario_getCount();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_drawTextBGFill(BG_B, "ESTRATEGIA MEGA DRIVE", 9, 2, 22);
    VDP_drawTextBGFill(BG_B, "SELECCIONA UNA MISION", 9, 4, 22);

    for (i = 0; i < count; ++i)
    {
        const ScenarioDefinition *scenario = Scenario_get(i);
        /* Cuatro filas por bloque: caben cuatro misiones sin pisar controles.
         * La última descripción y resumen terminan en la fila 19; los controles
         * permanecen abajo, separados de todas las opciones. */
        const u16 row = (u16)(5 + ((u16)i * 4));
        if (scenario == NULL) continue;

        VDP_drawTextBGFill(BG_B, (i == selectedIndex) ? ">" : " ",
                           4, row, 1);
        VDP_drawTextBGFill(BG_B, scenario->name, 6, row, 31);
        VDP_drawTextBGFill(BG_B, scenario->menuDescription,
                           8, (u16)(row + 1), 31);
        VDP_drawTextBGFill(BG_B, scenario->menuSummary,
                           8, (u16)(row + 2), 31);
    }

    VDP_drawTextBGFill(BG_B, "ARRIBA/ABAJO: ELEGIR", 8, 22, 24);
    VDP_drawTextBGFill(BG_B, "A O START: COMENZAR", 8, 24, 24);
}

/* Selector de arranque; no crea sprites ni consume los buffers del mapa. */
u8 ScenarioMenu_showFrom(u8 initialIndex)
{
    u8 selectedIndex = Scenario_cycleIndex(initialIndex, 0);
    u16 previousJoy = 0;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);
    Team_initPalettes();
    VDP_setTextPlane(BG_B);
    VDP_setTextPalette(PAL3);
    VDP_setTextPriority(TRUE);
    JOY_init();

    drawScenarioMenu(selectedIndex);
    SYS_doVBlankProcess();

    while (TRUE)
    {
        const u16 joy = JOY_readJoypad(JOY_1);
        const bool upPressed = ((joy & BUTTON_UP) != 0) &&
                               ((previousJoy & BUTTON_UP) == 0);
        const bool downPressed = ((joy & BUTTON_DOWN) != 0) &&
                                 ((previousJoy & BUTTON_DOWN) == 0);
        const bool confirmPressed =
            (((joy & BUTTON_A) != 0) && ((previousJoy & BUTTON_A) == 0)) ||
            (((joy & BUTTON_START) != 0) &&
             ((previousJoy & BUTTON_START) == 0));

        if (upPressed)
        {
            selectedIndex = Scenario_cycleIndex(selectedIndex, -1);
            drawScenarioMenu(selectedIndex);
        }
        else if (downPressed)
        {
            selectedIndex = Scenario_cycleIndex(selectedIndex, 1);
            drawScenarioMenu(selectedIndex);
        }

        if (confirmPressed)
        {
            /* No dejar A/START pulsado para que no actúe sobre la primera celda. */
            u16 heldButtons;
            do
            {
                SYS_doVBlankProcess();
                heldButtons = JOY_readJoypad(JOY_1);
            } while ((heldButtons & (BUTTON_A | BUTTON_START)) != 0);
            return selectedIndex;
        }

        previousJoy = joy;
        SYS_doVBlankProcess();
    }
}


/* Conserva el comportamiento de arranque para llamadas antiguas. */
u8 ScenarioMenu_show(void)
{
    return ScenarioMenu_showFrom(ACTIVE_MAP_INDEX);
}
