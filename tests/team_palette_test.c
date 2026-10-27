#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "team.h"

static u16 paletteWrites;
static u16 recordedPalettes[4][16];

void PAL_setPalette(u16 palette, const u16 *data, u16 transfer)
{
    assert(palette < 4);
    assert(data != NULL);
    assert(transfer == DMA_QUEUE_COPY);
    memcpy(recordedPalettes[palette], data, sizeof(recordedPalettes[palette]));
    paletteWrites++;
}

int main(void)
{
    const TeamInfo *blue = Team_getInfo(TEAM_BLUE);
    const TeamInfo *red = Team_getInfo(TEAM_RED);

    assert(blue != NULL && strcmp(blue->name, "AZUL") == 0);
    assert(red != NULL && strcmp(red->name, "ROJO") == 0);
    assert(blue->paletteLine == PAL2);
    assert(red->paletteLine == PAL3);
    assert(Team_getInfo((TeamId)99) == NULL);

    Team_initPalettes();
    assert(paletteWrites == 2);
    assert(recordedPalettes[PAL2][3] != recordedPalettes[PAL2][4]);
    assert(recordedPalettes[PAL3][3] != recordedPalettes[PAL3][4]);
    assert(recordedPalettes[PAL2][3] != recordedPalettes[PAL3][3]);
    assert(recordedPalettes[PAL2][4] != recordedPalettes[PAL3][4]);
    /* PAL3 index 1 remains white for the HUD font. */
    assert(recordedPalettes[PAL3][1] == RGB24_TO_VDPCOLOR(0xFFFFFF));

    puts("PASS: equipos, paletas azul/rojo y color de fuente del HUD");
    return 0;
}
