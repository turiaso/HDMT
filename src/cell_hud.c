/*
 * HUD contextual y feedback temporal. La presentación consulta el estado lógico, pero
 * nunca debe alterar turnos, ocupación ni resultados de combate.
 */
#include <genesis.h>
#include <string.h>
#include "cell_events.h"
#include "cell_hud.h"
#include "game_config.h"

/*
 * Presentación del HUD. Mantiene cachés de texto para escribir solo las líneas que
 * cambian; las reglas de selección, visión y combate se consultan a otros módulos.
 */

/* PAL3 belongs to the red team; its index 1 stays white for HUD glyphs. */

typedef struct
{
    char *text;
    u16 length;
    bool truncated;
} HudLine;

static bool hoveredValid;
static MapData *hoveredMap;
static u16 hoveredX;
static u16 hoveredY;
static char unitStatus[HUD_TEXT_WIDTH + 1];
static char turnStatus[HUD_TEXT_WIDTH + 1];
static char missionStatus[HUD_TEXT_WIDTH + 1];
static char cellInfoCache[HUD_CELL_INFO_LINES][HUD_TEXT_WIDTH + 1];
static bool cellInfoCacheValid[HUD_CELL_INFO_LINES];
static TeamId fogViewerTeam = TEAM_BLUE;
static bool fogEnabled;
static UnitType movementTerrainType = UNIT_TYPE_INFANTRY;
static bool movementTerrainProfileActive;
static bool fogSharedVision;
static const UnitController *hudUnitController;
/* El feedback de una accion se conserva brevemente para que pueda leerse. */
static u8 actionFeedbackFrames;

static void drawHudLine(u16 row, const char *text)
{
    VDP_drawTextBGFill(BG_B, text, HUD_TEXT_X, row, HUD_TEXT_WIDTH);
}

/* Solo escribe filas de informacion de celda cuyo contenido haya cambiado. */
static void drawCellInfoLine(u16 row, const char *text)
{
    u16 length = 0;

    if (row >= HUD_CELL_INFO_LINES) return;
    if (text == NULL) text = "";
    if (cellInfoCacheValid[row] && strcmp(cellInfoCache[row], text) == 0)
        return;

    while (text[length] && (length < HUD_TEXT_WIDTH))
    {
        cellInfoCache[row][length] = text[length];
        length++;
    }
    cellInfoCache[row][length] = '\0';
    drawHudLine(HUD_TEXT_Y + row, cellInfoCache[row]);
    cellInfoCacheValid[row] = TRUE;
}

static void beginHudLine(HudLine *line, char *buffer)
{
    line->text = buffer;
    line->length = 0;
    line->truncated = FALSE;
    buffer[0] = '\0';
}

static void appendHudText(HudLine *line, const char *text)
{
    while (*text && (line->length < HUD_TEXT_WIDTH))
        line->text[line->length++] = *text++;

    if (*text) line->truncated = TRUE;
    line->text[line->length] = '\0';
}

/* Las líneas largas terminan en puntos suspensivos, nunca a mitad de palabra. */
static void finishHudLine(HudLine *line)
{
    u16 cut;

    if ((line == NULL) || !line->truncated || (HUD_TEXT_WIDTH < 4)) return;

    cut = HUD_TEXT_WIDTH - 3;
    while ((cut > 0) && (line->text[cut] != ' ')) cut--;
    if (cut == 0) cut = HUD_TEXT_WIDTH - 3;
    while ((cut > 0) && (line->text[cut - 1] == ' ')) cut--;

    line->text[cut++] = '.';
    line->text[cut++] = '.';
    line->text[cut++] = '.';
    line->text[cut] = '\0';
    line->length = cut;
    line->truncated = FALSE;
}

static void appendHudUnsigned(HudLine *line, u32 value, u16 minDigits)
{
    char number[11];
    uintToStr(value, number, minDigits);
    appendHudText(line, number);
}

static void appendHudSigned(HudLine *line, s32 value)
{
    char number[12];
    intToStr(value, number, 1);
    appendHudText(line, number);
}

static void drawUnitStatus(void)
{
    drawHudLine(HUD_TEXT_Y + HUD_CELL_INFO_LINES, unitStatus);
}

static void drawTurnStatus(void)
{
    drawHudLine(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 1, turnStatus);
}

static void drawMissionStatus(void)
{
    drawHudLine(HUD_TEXT_Y + HUD_CELL_INFO_LINES + 2, missionStatus);
}

static void setUnitStatusText(const char *status)
{
    u16 length = 0;
    char nextStatus[HUD_TEXT_WIDTH + 1];

    if (status == NULL) status = "";
    while (status[length] && (length < HUD_TEXT_WIDTH))
    {
        nextStatus[length] = status[length];
        length++;
    }
    nextStatus[length] = '\0';

    /* No cortes una palabra a mitad: reserva tres caracteres para indicar
       truncamiento y retrocede hasta el último espacio disponible. */
    if (status[length] != '\0' && HUD_TEXT_WIDTH >= 4)
    {
        u16 cut = HUD_TEXT_WIDTH - 3;
        while ((cut > 0) && (nextStatus[cut] != ' ')) cut--;
        if (cut == 0) cut = HUD_TEXT_WIDTH - 3;
        while ((cut > 0) && (nextStatus[cut - 1] == ' ')) cut--;
        nextStatus[cut++] = '.';
        nextStatus[cut++] = '.';
        nextStatus[cut++] = '.';
        nextStatus[cut] = '\0';
        length = cut;
    }

    if (strcmp(unitStatus, nextStatus) == 0) return;

    memcpy(unitStatus, nextStatus, length + 1);
    drawUnitStatus();
}

/* Mensaje prioritario: reemplaza cualquier feedback anterior inmediatamente. */
void CellHud_setUnitStatus(const char *status)
{
    actionFeedbackFrames = 0;
    setUnitStatusText(status);
}

/* Estado derivado del cursor: no pisa el resultado reciente de una accion. */
void CellHud_setContextStatus(const char *status)
{
    if (actionFeedbackFrames != 0) return;
    setUnitStatusText(status);
}

/* Mantiene resultados de acciones visibles durante unos frames o hasta mover el cursor. */
void CellHud_setActionFeedback(const char *status)
{
    setUnitStatusText(status);
    actionFeedbackFrames = HUD_ACTION_FEEDBACK_FRAMES;
}

bool CellHud_tick(void)
{
    if (actionFeedbackFrames == 0) return FALSE;
    actionFeedbackFrames--;
    return actionFeedbackFrames == 0;
}

void CellHud_setUnitCombatInfo(const char *shortName, u8 health,
                               u8 maxHealth, u8 attack, u8 defense,
                               u8 attackRange, u8 visionRange,
                               u8 repairRange, u8 repairAmount)
{
    char buffer[HUD_TEXT_WIDTH + 1];
    HudLine line;

    beginHudLine(&line, buffer);
    appendHudText(&line, "SEL ");
    appendHudText(&line, (shortName != NULL) ? shortName : "UNIDAD");
    appendHudText(&line, " HP");
    appendHudUnsigned(&line, health, 1);
    appendHudText(&line, "/");
    appendHudUnsigned(&line, maxHealth, 1);
    appendHudText(&line, " AT");
    appendHudUnsigned(&line, attack, 1);
    appendHudText(&line, " DF");
    appendHudUnsigned(&line, defense, 1);
    appendHudText(&line, " AR");
    appendHudUnsigned(&line, attackRange, 1);
    appendHudText(&line, " V");
    appendHudUnsigned(&line, visionRange, 1);
    if ((repairRange > 0) && (repairAmount > 0))
    {
        /* Formato compacto: rango y PV restaurados por acción (p. ej. REP1/3). */
        appendHudText(&line, " REP");
        appendHudUnsigned(&line, repairRange, 1);
        appendHudText(&line, "/");
        appendHudUnsigned(&line, repairAmount, 1);
    }
    finishHudLine(&line);
    CellHud_setContextStatus(line.text);
}

static void setRepairStatus(const char *prefix, u8 healthRestored,
                            u8 resultingHealth, u8 maxHealth, bool isResult)
{
    char buffer[HUD_TEXT_WIDTH + 1];
    HudLine line;

    beginHudLine(&line, buffer);
    appendHudText(&line, prefix);
    appendHudText(&line, " +");
    appendHudUnsigned(&line, healthRestored, 1);
    appendHudText(&line, " HP -> ");
    appendHudUnsigned(&line, resultingHealth, 1);
    appendHudText(&line, "/");
    appendHudUnsigned(&line, maxHealth, 1);
    finishHudLine(&line);
    if (isResult) CellHud_setActionFeedback(line.text);
    else CellHud_setContextStatus(line.text);
}

void CellHud_setRepairPreview(u8 healthRestored, u8 resultingHealth,
                              u8 maxHealth)
{
    setRepairStatus("REPARAR", healthRestored, resultingHealth, maxHealth, FALSE);
}

void CellHud_setRepairResult(u8 healthRestored, u8 resultingHealth,
                             u8 maxHealth)
{
    setRepairStatus("REPARADO", healthRestored, resultingHealth, maxHealth, TRUE);
}

void CellHud_setCombatPreview(u8 damage, u8 counterDamage,
                              u8 targetHealth, u8 targetMaxHealth)
{
    char buffer[HUD_TEXT_WIDTH + 1];
    HudLine line;
    const u8 remainingHealth = (damage >= targetHealth)
        ? 0 : (u8)(targetHealth - damage);

    beginHudLine(&line, buffer);
    appendHudText(&line, "ATQ -");
    appendHudUnsigned(&line, damage, 1);
    appendHudText(&line, " / OBJ HP ");
    appendHudUnsigned(&line, remainingHealth, 1);
    appendHudText(&line, "/");
    appendHudUnsigned(&line, targetMaxHealth, 1);
    if (counterDamage > 0)
    {
        appendHudText(&line, " / REP -");
        appendHudUnsigned(&line, counterDamage, 1);
    }
    else appendHudText(&line, " / SIN REP");
    finishHudLine(&line);
    CellHud_setContextStatus(line.text);
}

void CellHud_setRouteStatus(bool reachable, u16 steps, u8 cost,
                            u8 availablePoints)
{
    char buffer[HUD_TEXT_WIDTH + 1];
    HudLine line;

    beginHudLine(&line, buffer);
    if (!reachable)
        appendHudText(&line, "SIN RUTA VALIDA");
    else
    {
        appendHudText(&line, "RUTA ");
        appendHudUnsigned(&line, steps, 1);
        appendHudText(&line, " PASOS / COSTE ");
        appendHudUnsigned(&line, cost, 1);
        appendHudText(&line, " / MP ");
        appendHudUnsigned(&line, availablePoints, 1);
    }
    finishHudLine(&line);
    CellHud_setContextStatus(line.text);
}


void CellHud_setTurnInfo(const char *teamName, u16 turnNumber,
                         bool hasSelectedUnit, u8 movementPoints,
                         u8 maxMovementPoints, u16 objectiveX,
                         u16 objectiveY, const char *outcomeText,
                         TeamId missionHolder, MissionType missionType,
                         u16 missionProgress,
                         u8 requiredTurns)
{
    char turnBuffer[HUD_TEXT_WIDTH + 1];
    char missionBuffer[HUD_TEXT_WIDTH + 1];
    HudLine turnLine;
    HudLine missionLine;

    beginHudLine(&turnLine, turnBuffer);
    beginHudLine(&missionLine, missionBuffer);

    if ((outcomeText != NULL) && (outcomeText[0] != '\0'))
    {
        /* El resultado sustituye al turno; se limpia el objetivo anterior. */
        appendHudText(&turnLine, outcomeText);
    }
    else
    {
        /* Mantén el turno y los MP separados del progreso de la misión. */
        appendHudText(&turnLine, "T");
        appendHudUnsigned(&turnLine, turnNumber, 1);
        appendHudText(&turnLine, " ");
        appendHudText(&turnLine, (teamName != NULL) ? teamName : "?");
        if (hasSelectedUnit)
        {
            appendHudText(&turnLine, " | MP ");
            appendHudUnsigned(&turnLine, movementPoints, 1);
            appendHudText(&turnLine, "/");
            appendHudUnsigned(&turnLine, maxMovementPoints, 1);
        }

        switch (missionType)
        {
            case MISSION_HOLD_POINT:
            {
                const TeamInfo *holder = Team_getInfo(missionHolder);
                appendHudText(&missionLine, "OBJ: PUENTE - ");
                appendHudText(&missionLine,
                    (holder != NULL) ? holder->name : "SIN CONTROL");
                appendHudText(&missionLine, " ");
                appendHudUnsigned(&missionLine, missionProgress, 1);
                appendHudText(&missionLine, "/");
                appendHudUnsigned(&missionLine,
                    requiredTurns ? requiredTurns : 1u, 1);
                break;
            }
            case MISSION_ELIMINATE_ALL:
                appendHudText(&missionLine, "OBJ: ELIMINAR TODO");
                break;
            case MISSION_SURVIVE_TURNS:
                appendHudText(&missionLine, "OBJ: RESISTIR ");
                appendHudUnsigned(&missionLine, missionProgress, 1);
                appendHudText(&missionLine, "/");
                appendHudUnsigned(&missionLine,
                    requiredTurns ? requiredTurns : 1u, 1);
                break;
            case MISSION_CAPTURE_POINT:
            default:
                appendHudText(&missionLine, "OBJ: CAPTURAR X");
                appendHudUnsigned(&missionLine, (u32)objectiveX + 1, 1);
                appendHudText(&missionLine, ",Y");
                appendHudUnsigned(&missionLine, (u32)objectiveY + 1, 1);
                break;
        }
    }

    finishHudLine(&turnLine);
    finishHudLine(&missionLine);

    if (strcmp(turnStatus, turnLine.text) != 0)
    {
        u16 i;
        for (i = 0; i <= turnLine.length; i++)
            turnStatus[i] = turnLine.text[i];
        drawTurnStatus();
    }

    if (strcmp(missionStatus, missionLine.text) != 0)
    {
        u16 i;
        for (i = 0; i <= missionLine.length; i++)
            missionStatus[i] = missionLine.text[i];
        drawMissionStatus();
    }
}

static void drawUnavailableHud(void)
{
    u16 row;

    drawCellInfoLine(0, "CASILLA: NO DISPONIBLE");
    for (row = 1; row < HUD_CELL_INFO_LINES; row++)
        drawCellInfoLine(row, "");
    drawUnitStatus();
    drawTurnStatus();
    drawMissionStatus();
}

static u8 getHudVisionState(const MapData *map, u16 x, u16 y)
{
    if (fogSharedVision) return MapData_getCombinedVisionState(map, x, y);
    return MapData_getVisionState(map, (u8)fogViewerTeam, x, y);
}

static void drawCellHud(const MapData *map, u16 x, u16 y)
{
    char buffer[HUD_TEXT_WIDTH + 1];
    HudLine line;
    const CellData *cell = MapData_getCellConst(map, x, y);
    const TerrainInfo *terrain = (cell != NULL)
        ? MapData_getTerrainInfo((TerrainType)cell->terrain) : NULL;

    if ((cell == NULL) || (terrain == NULL))
    {
        drawUnavailableHud();
        return;
    }

    if (fogEnabled && (getHudVisionState(map, x, y) == MAP_VISION_UNSEEN))
    {
        drawCellInfoLine(0, "CASILLA: DESCONOCIDA");
        drawCellInfoLine(1, "TERRENO: DESCONOCIDO");
        drawCellInfoLine(2, "COSTE: ?  TRANSITABLE: ?");
        drawCellInfoLine(3, "OCUPACION: DESCONOCIDA");
        return;
    }

    beginHudLine(&line, buffer);
    appendHudText(&line, "CASILLA: X=");
    appendHudUnsigned(&line, (u32)x + 1, 2);
    appendHudText(&line, " Y=");
    appendHudUnsigned(&line, (u32)y + 1, 2);
    finishHudLine(&line);
    drawCellInfoLine(0, line.text);

    beginHudLine(&line, buffer);
    appendHudText(&line, "TERRENO: ");
    appendHudText(&line, terrain->name);
    finishHudLine(&line);
    drawCellInfoLine(1, line.text);

    {
        const u8 movementCost = movementTerrainProfileActive
            ? UnitType_getTerrainMoveCost(movementTerrainType, cell->terrain)
            : terrain->moveCost;
        const bool traversable = terrain->passable && (movementCost != 0);

        beginHudLine(&line, buffer);
        appendHudText(&line, "COSTE: ");
        if (movementTerrainProfileActive && (movementCost == 0))
            appendHudText(&line, "NO");
        else
            appendHudUnsigned(&line, movementCost, 1);
        appendHudText(&line, " TRANSITABLE: ");
        appendHudText(&line, traversable ? "SI" : "NO");
        finishHudLine(&line);
        drawCellInfoLine(2, line.text);
    }

    beginHudLine(&line, buffer);
    if (fogEnabled &&
        (getHudVisionState(map, x, y) != MAP_VISION_VISIBLE))
    {
        /* El terreno se recuerda, pero no se filtra la posición de enemigos. */
        appendHudText(&line, "OCUPACION: SIN VISION");
    }
    else if (cell->occupantId == CELL_NO_OCCUPANT)
        appendHudText(&line, "OCUPACION: LIBRE");
    else
    {
        const Unit *occupant = (hudUnitController != NULL)
            ? UnitController_getUnitAt(hudUnitController, x, y) : NULL;

        if (occupant != NULL)
        {
            const UnitTypeInfo *unitInfo = UnitType_getInfo(occupant->type);
            const TeamInfo *teamInfo = Team_getInfo(occupant->team);

            appendHudText(&line, "OCUPACION: ");
            appendHudText(&line, (unitInfo != NULL)
                ? unitInfo->shortName : "UNIDAD");
            appendHudText(&line, " ");
            appendHudText(&line, (teamInfo != NULL) ? teamInfo->name : "?");
            appendHudText(&line, " HP");
            appendHudUnsigned(&line, occupant->health, 1);
            appendHudText(&line, "/");
            appendHudUnsigned(&line, occupant->maxHealth, 1);
        }
        else
        {
            /* Fallback diagnóstico si mapa y controlador no están sincronizados. */
            appendHudText(&line, "OCUPACION: ID ");
            appendHudSigned(&line, cell->occupantId);
        }
    }
    finishHudLine(&line);
    drawCellInfoLine(3, line.text);
}

void CellHud_setMovementTerrainProfile(UnitType type, bool hasSelectedUnit)
{
    const UnitType nextType = (UnitType_getInfo(type) != NULL)
        ? type : UNIT_TYPE_INFANTRY;

    if ((movementTerrainProfileActive == hasSelectedUnit) &&
        (!hasSelectedUnit || (movementTerrainType == nextType)))
        return;

    movementTerrainProfileActive = hasSelectedUnit;
    movementTerrainType = nextType;
    if (hoveredValid && (hoveredMap != NULL))
        drawCellHud(hoveredMap, hoveredX, hoveredY);
}

static void updateHoveredCell(MapData *map, u16 x, u16 y, bool clicked)
{
    if (!hoveredValid || (map != hoveredMap) || (x != hoveredX) || (y != hoveredY))
    {
        hoveredMap = map;
        hoveredX = x;
        hoveredY = y;
        hoveredValid = TRUE;
        drawCellHud(map, x, y);
    }

    CellEvents_updateHover(map, x, y, clicked);
}

void CellHud_setFogContext(TeamId viewerTeam, bool enabled,
                           bool sharedVision)
{
    fogViewerTeam = viewerTeam;
    fogEnabled = enabled;
    fogSharedVision = enabled && sharedVision;
}

void CellHud_setUnitController(const UnitController *controller)
{
    if (hudUnitController == controller) return;

    hudUnitController = controller;
    if (hoveredValid && (hoveredMap != NULL))
        drawCellHud(hoveredMap, hoveredX, hoveredY);
}

void CellHud_init(void)
{
    hoveredValid = FALSE;
    hoveredMap = NULL;
    fogViewerTeam = TEAM_BLUE;
    fogEnabled = FALSE;
    fogSharedVision = FALSE;
    movementTerrainType = UNIT_TYPE_INFANTRY;
    movementTerrainProfileActive = FALSE;
    hudUnitController = NULL;
    hoveredX = hoveredY = 0;
    unitStatus[0] = '\0';
    actionFeedbackFrames = 0;
    turnStatus[0] = '\0';
    missionStatus[0] = '\0';
    memset(cellInfoCache, 0, sizeof(cellInfoCache));
    memset(cellInfoCacheValid, 0, sizeof(cellInfoCacheValid));

    /* Team_initPalettes() sets PAL3 afterwards; its entry 1 is HUD white. */
    VDP_clearPlane(BG_B, TRUE);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_setTextPlane(BG_B);
    VDP_setTextPalette(PAL3);
    VDP_setTextPriority(TRUE);
}

void CellHud_refresh(void)
{
    if (!hoveredValid) return;

    /* El estado del mapa puede cambiar durante una animación de unidad. */
    drawCellHud(hoveredMap, hoveredX, hoveredY);
}


void CellHud_inspectCell(MapData *map, u16 cellX, u16 cellY)
{
    if ((map == NULL) || (cellX >= map->width) || (cellY >= map->height))
        return;
    if (hoveredValid && (map == hoveredMap) &&
        (cellX == hoveredX) && (cellY == hoveredY))
        return;

    hoveredMap = map;
    hoveredX = cellX;
    hoveredY = cellY;
    hoveredValid = TRUE;
    drawCellHud(map, cellX, cellY);
}

void CellHud_update(MapData *map, s16 cameraPixelX, s16 cameraPixelY,
                    s16 cursorPixelX, s16 cursorPixelY, bool clicked)
{
    u16 cellX;
    u16 cellY;
    const u16 worldX = (u16)(cameraPixelX + cursorPixelX);
    const u16 worldY = (u16)(cameraPixelY + cursorPixelY);

    if ((map == NULL) || (map->width == 0) || (map->height == 0))
    {
        drawUnavailableHud();
        hoveredValid = FALSE;
        hoveredMap = NULL;
        CellEvents_updateHover(NULL, 0, 0, FALSE);
        return;
    }

    cellX = worldX / CELL_PIXELS;
    cellY = worldY / CELL_PIXELS;
    if (cellX >= map->width) cellX = map->width - 1;
    if (cellY >= map->height) cellY = map->height - 1;

    /* Al cambiar de casilla, el jugador pide nuevo contexto: libera el mensaje. */
    if (!hoveredValid || (map != hoveredMap) ||
        (cellX != hoveredX) || (cellY != hoveredY))
        actionFeedbackFrames = 0;

    if (fogEnabled && (getHudVisionState(map, cellX, cellY) == MAP_VISION_UNSEEN))
    {
        if (!hoveredValid || (map != hoveredMap) ||
            (cellX != hoveredX) || (cellY != hoveredY))
        {
            hoveredMap = map;
            hoveredX = cellX;
            hoveredY = cellY;
            hoveredValid = TRUE;
            drawCellHud(map, cellX, cellY);
        }
        CellEvents_updateHover(NULL, 0, 0, FALSE);
        return;
    }

    updateHoveredCell(map, cellX, cellY, clicked);
}
