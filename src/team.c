#include "team.h"

/* Identidad y paletas de los equipos. Las reglas lógicas no dependen del color visual. */

/*
 * Palette layout for the Mega Drive's four hardware palette lines:
 * PAL0 = terrain, PAL1 = alcance animado, PAL2 = azul/mascaras, PAL3 = rojo/HUD.
 * Unit artwork uses indices 3 and 4; cursor artwork keeps indices 1 and 2.
 * PAL3 index 1 remains white for the SGDK text font on BG_B.
 */
static const u16 bluePalette[16] =
{
    RGB24_TO_VDPCOLOR(0x000000), RGB24_TO_VDPCOLOR(0x071221),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0x092C76),
    RGB24_TO_VDPCOLOR(0x36A0FF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF)
};

static const u16 redPalette[16] =
{
    RGB24_TO_VDPCOLOR(0x000000), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0x76101B),
    RGB24_TO_VDPCOLOR(0xFF4657), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF),
    RGB24_TO_VDPCOLOR(0xFFFFFF), RGB24_TO_VDPCOLOR(0xFFFFFF)
};

static const TeamInfo teams[TEAM_COUNT] =
{
    { "AZUL", PAL2 },
    { "ROJO", PAL3 }
};

const TeamInfo *Team_getInfo(TeamId team)
{
    if ((u8)team >= TEAM_COUNT) return NULL;
    return &teams[team];
}

void Team_initPalettes(void)
{
    PAL_setPalette(PAL2, bluePalette, DMA_QUEUE_COPY);
    PAL_setPalette(PAL3, redPalette, DMA_QUEUE_COPY);
}
