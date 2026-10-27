#include <stdio.h>
#include <genesis.h>
#include "game_config.h"

int main(void)
{
    const u16 firstSafeTile = TILE_SYSTEM_INDEX + TILE_SYSTEM_LENGTH + FONT_LENGTH;

    if (TERRAIN_VRAM_TILE_INDEX != firstSafeTile)
    {
        fputs("FAIL: el terreno no empieza después de la región de fuente reservada\n", stderr);
        return 1;
    }

    if (UNIT_VRAM_TILE_INDEX < TERRAIN_VRAM_TILE_INDEX)
    {
        fputs("FAIL: la unidad se solapa con el atlas de terreno\n", stderr);
        return 1;
    }
    if ((UNIT_VRAM_TILE_INDEX + UNIT_VRAM_TILE_COUNT) > CURSOR_VRAM_TILE_INDEX)
    {
        fputs("FAIL: los cuatro tipos de unidad se solapan con los tiles del cursor\n", stderr);
        return 1;
    }
    if ((CURSOR_VRAM_TILE_INDEX + CURSOR_SPRITE_TILE_COUNT) > VRAM_TILE_COUNT)
    {
        fputs("FAIL: el cursor supera la VRAM\n", stderr);
        return 1;
    }

    puts("PASS: terreno, unidad y cursor tienen rangos de VRAM separados");
    return 0;
}
