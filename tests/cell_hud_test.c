#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cell_hud.h"
#include "cell_events.h"
#include "game_config.h"

#define MAX_WRITES 64

typedef struct
{
    u16 plane;
    u16 x;
    u16 y;
    u16 width;
    char text[HUD_TEXT_WIDTH + 1];
} TextWrite;

static TextWrite writes[MAX_WRITES];
static u16 writeCount;
static u16 stayEventCount;
static Unit hudTestUnits[1];
static UnitController hudTestController;

static void onStay(const CellEventContext *context)
{
    assert(context != NULL);
    assert(context->type == CELL_EVENT_STAY);
    stayEventCount++;
}

static const CellEventSet hudEventSets[] =
{
    { { 0 } },
    { { [CELL_EVENT_STAY] = onStay } }
};

/* Captura las escrituras del HUD sin simular el VDP. */
void VDP_drawTextBGFill(u16 plane, const char *text, u16 x, u16 y, u16 width)
{
    TextWrite *write;

    assert(writeCount < MAX_WRITES);
    assert(width <= HUD_TEXT_WIDTH);
    write = &writes[writeCount++];
    write->plane = plane;
    write->x = x;
    write->y = y;
    write->width = width;
    strncpy(write->text, text, width);
    write->text[width] = '\0';
}

void PAL_setPalette(u16 palette, const u16 *data, u16 transfer)
{ (void)palette; (void)data; (void)transfer; }
void VDP_clearPlane(u16 plane, bool wait) { (void)plane; (void)wait; }
void VDP_setHorizontalScroll(u16 plane, s16 value) { (void)plane; (void)value; }
void VDP_setVerticalScroll(u16 plane, s16 value) { (void)plane; (void)value; }
void VDP_setTextPlane(u16 plane) { (void)plane; }
void VDP_setTextPalette(u16 palette) { (void)palette; }
void VDP_setTextPriority(u16 priority) { (void)priority; }
void uintToStr(u32 value, char *dest, u16 minDigits)
{
    char reversed[10];
    u16 length = 0;
    u16 i;

    do
    {
        reversed[length++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && length < sizeof(reversed));

    while ((length < minDigits) && (length < sizeof(reversed)))
        reversed[length++] = '0';

    for (i = 0; i < length; i++) dest[i] = reversed[length - i - 1];
    dest[length] = '\0';
}

void intToStr(s32 value, char *dest, u16 minDigits)
{
    u32 magnitude;

    if (value < 0)
    {
        *dest++ = '-';
        magnitude = (u32)(-(value + 1)) + 1;
    }
    else magnitude = (u32)value;

    uintToStr(magnitude, dest, minDigits);
}

static void expectWrite(u16 index, u16 row, const char *text)
{
    assert(index < writeCount);
    if ((writes[index].plane != BG_B) || (writes[index].x != HUD_TEXT_X) ||
        (writes[index].y != row) || (writes[index].width != HUD_TEXT_WIDTH))
    {
        fprintf(stderr,
                "HUD write %u position mismatch: expected plane=%u x=%u y=%u width=%u; got plane=%u x=%u y=%u width=%u\n",
                index, BG_B, HUD_TEXT_X, row, HUD_TEXT_WIDTH,
                writes[index].plane, writes[index].x, writes[index].y,
                writes[index].width);
        assert(0);
    }
    if (strcmp(writes[index].text, text) != 0)
    {
        fprintf(stderr, "HUD write %u mismatch: expected [%s], got [%s]\n",
                index, text, writes[index].text);
        assert(strcmp(writes[index].text, text) == 0);
    }
}

static void expectMostRecentWrite(u16 row, const char *text)
{
    u16 index = writeCount;

    while (index > 0)
    {
        index--;
        if (writes[index].y == row)
        {
            expectWrite(index, row, text);
            return;
        }
    }

    fprintf(stderr, "HUD row %u has no captured writes\n", row);
    assert(0);
}

int main(void)
{
    CellHud_init();
    assert(MapData_init(&worldMapData));
    memset(hudTestUnits, 0, sizeof(hudTestUnits));
    memset(&hudTestController, 0, sizeof(hudTestController));
    hudTestUnits[0].id = 5;
    hudTestUnits[0].x = 4;
    hudTestUnits[0].y = 1;
    hudTestUnits[0].type = UNIT_TYPE_TANK;
    hudTestUnits[0].team = TEAM_RED;
    hudTestUnits[0].health = 9;
    hudTestUnits[0].maxHealth = 14;
    hudTestUnits[0].alive = TRUE;
    hudTestController.map = &worldMapData;
    hudTestController.units = hudTestUnits;
    hudTestController.count = 1;
    CellHud_setUnitController(&hudTestController);
    CellEvents_init(hudEventSets,
                    (u8)(sizeof(hudEventSets) / sizeof(hudEventSets[0])));
    stayEventCount = 0;

    writeCount = 0;
    CellHud_update(&worldMapData, 0, 0, 0, 0, FALSE);
    assert(writeCount == HUD_CELL_INFO_LINES);
    expectWrite(0, HUD_TEXT_Y,     "CASILLA: X=01 Y=01");
    expectWrite(1, HUD_TEXT_Y + 1, "TERRENO: Agua");
    expectWrite(2, HUD_TEXT_Y + 2, "COSTE: 0 TRANSITABLE: NO");
    expectWrite(3, HUD_TEXT_Y + 3, "OCUPACION: LIBRE");

    /* La misma celda no repinta el HUD; otra solo escribe filas distintas. */
    CellHud_update(&worldMapData, 0, 0, 0, 0, FALSE);
    assert(writeCount == HUD_CELL_INFO_LINES);
    assert(stayEventCount == 1);
    CellHud_update(&worldMapData, 4 * CELL_PIXELS, CELL_PIXELS, 0, 0, FALSE);
    assert(writeCount == 7); /* Coordenadas, terreno y coste; ocupacion no cambia. */
    expectWrite(4, HUD_TEXT_Y, "CASILLA: X=05 Y=02");
    expectWrite(5, HUD_TEXT_Y + 1, "TERRENO: Bosque");
    expectWrite(6, HUD_TEXT_Y + 2, "COSTE: 2 TRANSITABLE: SI");

    /* Refrescar una ocupacion solo repinta su fila, sin eventos de cursor. */
    assert(MapData_setOccupant(&worldMapData, 4, 1, 5));
    CellHud_refresh();
    assert(writeCount == 8);
    expectWrite(7, HUD_TEXT_Y + 3, "OCUPACION: BLD ROJO HP9/14");
    assert(stayEventCount == 1);

    /* La ficha de ocupación no filtra unidades enemigas fuera de visión. */
    {
        CellData *cell = MapData_getCell(&worldMapData, 4, 1);
        assert(cell != NULL);
        CellHud_setFogContext(TEAM_BLUE, TRUE, FALSE);

        cell->visionState = MAP_VISION_EXPLORED;
        CellHud_refresh();
        expectMostRecentWrite(HUD_TEXT_Y + 3, "OCUPACION: SIN VISION");

        cell->visionState = MAP_VISION_UNSEEN;
        CellHud_refresh();
        expectMostRecentWrite(HUD_TEXT_Y + 3, "OCUPACION: DESCONOCIDA");

        cell->visionState = MAP_VISION_VISIBLE;
        CellHud_refresh();
        expectMostRecentWrite(HUD_TEXT_Y + 3, "OCUPACION: BLD ROJO HP9/14");

        CellHud_setFogContext(TEAM_BLUE, FALSE, FALSE);
    }
    assert(MapData_setOccupant(&worldMapData, 4, 1, CELL_NO_OCCUPANT));
    CellHud_refresh();
    expectMostRecentWrite(HUD_TEXT_Y + 3, "OCUPACION: LIBRE");

    /* A partir de aquí, las pruebas de estado empiezan con un contador limpio. */
    writeCount = 0;

    CellHud_setUnitStatus("UNIDAD SELECCIONADA");
    assert(writeCount == 1);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "UNIDAD SELECCIONADA");
    CellHud_setUnitStatus("UNIDAD SELECCIONADA");
    assert(writeCount == 1); /* Un estado idéntico no escribe de nuevo el HUD. */

    CellHud_setRouteStatus(TRUE, 6, 6, 6);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "RUTA 6 PASOS / COSTE 6 / MP 6");
    CellHud_setRouteStatus(FALSE, 0, 0, 6);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "SIN RUTA VALIDA");

    /* El resultado de una accion no debe desaparecer por la previsualizacion siguiente. */
    {
        u16 i;
        const u16 beforeFeedback = writeCount;
        CellHud_setActionFeedback("ENEMIGO ELIMINADO (-4 HP)");
        assert(writeCount == (u16)(beforeFeedback + 1));
        expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                    "ENEMIGO ELIMINADO (-4 HP)");
        CellHud_setContextStatus("RUTA 2 PASOS / COSTE 2 / MP 4");
        assert(writeCount == (u16)(beforeFeedback + 1));
        for (i = 0; i < HUD_ACTION_FEEDBACK_FRAMES - 1; i++)
            assert(!CellHud_tick());
        CellHud_setContextStatus("RUTA 2 PASOS / COSTE 2 / MP 4");
        assert(writeCount == (u16)(beforeFeedback + 1));
        assert(CellHud_tick());
        assert(!CellHud_tick());
        CellHud_setContextStatus("RUTA 2 PASOS / COSTE 2 / MP 4");
        assert(writeCount == (u16)(beforeFeedback + 2));
        expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                    "RUTA 2 PASOS / COSTE 2 / MP 4");
    }

    /* Mover el cursor de casilla libera el feedback antes de que venza el tiempo. */
    {
        const u16 oldWriteCount = writeCount;
        CellHud_setActionFeedback("ATAQUE BLOQUEADO");
        CellHud_update(&worldMapData, 0, 0,
                       (s16)(6 * CELL_PIXELS), (s16)(3 * CELL_PIXELS), FALSE);
        CellHud_setContextStatus("A: SELECCIONAR UNIDAD");
        assert(writeCount > oldWriteCount);
        expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                              "A: SELECCIONAR UNIDAD");
    }

    CellHud_setTurnInfo("AZUL", 2, TRUE, 4, 6, 10, 10, "", TEAM_NONE,
                        MISSION_CAPTURE_POINT, 0, 1);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T2 AZUL | MP 4/6");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: CAPTURAR X11,Y11");

    CellHud_setTurnInfo("AZUL", 2, TRUE, 4, 6, 10, 10,
                        "VICTORIA AZUL / DERROTA ROJA", TEAM_NONE,
                        MISSION_CAPTURE_POINT, 0, 1);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "VICTORIA AZUL / DERROTA ROJA");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "");

    CellHud_setTurnInfo("AZUL", 3, FALSE, 0, 0, 10, 10, "", TEAM_BLUE,
                        MISSION_HOLD_POINT, 1, 3);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T3 AZUL");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: PUENTE - AZUL 1/3");

    /* El HUD explica explícitamente que el puente aún no tiene ocupante. */
    CellHud_setTurnInfo("AZUL", 3, FALSE, 0, 0, 10, 10, "", TEAM_NONE,
                        MISSION_HOLD_POINT, 0, 3);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2,
                          "OBJ: PUENTE - SIN CONTROL 0/3");

    CellHud_setTurnInfo("ROJO", 4, FALSE, 0, 0, 10, 10, "", TEAM_NONE,
                        MISSION_SURVIVE_TURNS, 2, 4);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T4 ROJO");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: RESISTIR 2/4");

    CellHud_setTurnInfo("ROJO", 4, FALSE, 0, 0, 10, 10, "", TEAM_NONE,
                        MISSION_ELIMINATE_ALL, 0, 1);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: ELIMINAR TODO");

    CellHud_setTurnInfo("AZUL", 3, TRUE, 4, 6, 10, 10, "", TEAM_BLUE,
                        MISSION_HOLD_POINT, 1, 3);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T3 AZUL | MP 4/6");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: PUENTE - AZUL 1/3");

    CellHud_setTurnInfo("ROJO", 4, TRUE, 3, 5, 10, 10, "", TEAM_NONE,
                        MISSION_SURVIVE_TURNS, 2, 4);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T4 ROJO | MP 3/5");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: RESISTIR 2/4");

    CellHud_setTurnInfo("ROJO", 4, TRUE, 3, 5, 10, 10, "", TEAM_NONE,
                        MISSION_ELIMINATE_ALL, 0, 1);
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, "T4 ROJO | MP 3/5");
    expectMostRecentWrite(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, "OBJ: ELIMINAR TODO");
    {
        const u16 oldWriteCount = writeCount;
        CellHud_setTurnInfo("ROJO", 4, TRUE, 3, 5, 10, 10, "", TEAM_NONE,
                            MISSION_ELIMINATE_ALL, 0, 1);
        assert(writeCount == oldWriteCount);
    }

    /* La predicción anticipa los PV que quedarán al objetivo tras el ataque. */
    CellHud_setCombatPreview(3, 2, 6, 8);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "ATQ -3 / OBJ HP 3/8 / REP -2");
    CellHud_setCombatPreview(7, 0, 4, 8);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "ATQ -7 / OBJ HP 0/8 / SIN REP");

    /* Los mensajes largos mantienen palabras completas y señalan el recorte. */
    CellHud_setUnitStatus("ALIADO FUERA DE ALCANCE DE REPARACION");
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "ALIADO FUERA DE ALCANCE DE...");

    CellHud_setUnitCombatInfo("INF", 10, 10, 4, 2, 1, 3, 0, 0);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "SEL INF HP10/10 AT4 DF2 AR1 V3");
    CellHud_setUnitCombatInfo("ING", 8, 8, 2, 2, 1, 2, 1, 3);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "SEL ING HP8/8 AT2 DF2 AR1 V2 REP1/3");

    CellHud_setRepairPreview(3, 8, 10);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "REPARAR +3 HP -> 8/10");
    CellHud_setRepairResult(2, 10, 10);
    expectWrite(writeCount - 1, HUD_TEXT_Y + HUD_CELL_INFO_LINES,
                "REPARADO +2 HP -> 10/10");

    /* La inspección de espectador dibuja una casilla sin despachar STAY. */
    {
        const u16 oldStayCount = stayEventCount;
        const u16 oldWriteCount = writeCount;
        CellHud_inspectCell(&worldMapData, 8, 4);
        assert(writeCount > oldWriteCount);
        assert(stayEventCount == oldStayCount);
        writeCount = oldWriteCount;
        CellHud_inspectCell(&worldMapData, 8, 4);
        assert(writeCount == oldWriteCount); /* Casilla igual: no repinta. */
        assert(stayEventCount == oldStayCount);
    }

    /* La perspectiva de IA conoce lo que ve cualquiera de los equipos, pero
       sigue ocultando una casilla que ambos desconocen. */
    {
        CellData sharedCell = { TERRAIN_PATH, CELL_NO_OCCUPANT, 0,
            (u8)(MAP_VISION_VISIBLE | (MAP_VISION_UNSEEN << 2)) };
        MapData sharedMap = { .width = 1, .height = 1, .cells = &sharedCell,
                              .cellCapacity = 1 };
        u16 baseWriteCount;

        /* Fuerza una caché limpia para poder verificar las cuatro filas escritas. */
        CellHud_init();
        baseWriteCount = writeCount;
        CellHud_setFogContext(TEAM_RED, TRUE, TRUE);
        CellHud_inspectCell(&sharedMap, 0, 0);
        expectWrite(baseWriteCount, HUD_TEXT_Y, "CASILLA: X=01 Y=01");
        expectWrite(baseWriteCount + 1, HUD_TEXT_Y + 1, "TERRENO: Camino");
        expectWrite(baseWriteCount + 3, HUD_TEXT_Y + 3, "OCUPACION: LIBRE");

        sharedCell.visionState =
            (u8)(MAP_VISION_UNSEEN | (MAP_VISION_UNSEEN << 2));
        CellHud_refresh();
        expectWrite(writeCount - HUD_CELL_INFO_LINES, HUD_TEXT_Y,
                    "CASILLA: DESCONOCIDA");
        expectWrite(writeCount - HUD_CELL_INFO_LINES + 1, HUD_TEXT_Y + 1,
                    "TERRENO: DESCONOCIDO");
    }

    /* El coste del terreno cambia con la clase seleccionada y vuelve al coste
       base al quitar la selección; solo repinta la línea cuyo texto varía. */
    CellHud_init();
    CellHud_inspectCell(&worldMapData, 4, 1); /* Bosque. */
    expectWrite(writeCount - HUD_CELL_INFO_LINES + 2, HUD_TEXT_Y + 2,
                "COSTE: 2 TRANSITABLE: SI");
    {
        u16 previousWrites = writeCount;
        CellHud_setMovementTerrainProfile(UNIT_TYPE_SCOUT, TRUE);
        assert(writeCount == previousWrites + 1);
        expectWrite(writeCount - 1, HUD_TEXT_Y + 2,
                    "COSTE: 1 TRANSITABLE: SI");

        previousWrites = writeCount;
        CellHud_setMovementTerrainProfile(UNIT_TYPE_TANK, TRUE);
        assert(writeCount == previousWrites + 1);
        expectWrite(writeCount - 1, HUD_TEXT_Y + 2,
                    "COSTE: 3 TRANSITABLE: SI");

        previousWrites = writeCount;
        CellHud_setMovementTerrainProfile(UNIT_TYPE_INFANTRY, FALSE);
        assert(writeCount == previousWrites + 1);
        expectWrite(writeCount - 1, HUD_TEXT_Y + 2,
                    "COSTE: 2 TRANSITABLE: SI");
    }

    puts("PASS: HUD de celda, turno y objetivo; coste por clase y sin eventos redundantes");
    return 0;
}
