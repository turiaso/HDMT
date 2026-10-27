#ifndef TEST_GENESIS_H
#define TEST_GENESIS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
#define TRUE 1
#define FALSE 0
#define CPU 0
#define DMA 1
#define DMA_QUEUE 2
#define DMA_QUEUE_COPY 3
#define BG_A 0
#define BG_B 1
#define PAL0 0
#define PAL1 1
#define PAL2 2
#define PAL3 3
#define BUTTON_LEFT 0x0004
#define BUTTON_RIGHT 0x0008
#define BUTTON_UP 0x0001
#define BUTTON_DOWN 0x0002
#define BUTTON_C 0x0020
#define BUTTON_A 0x0040
#define BUTTON_B 0x0010
#define BUTTON_START 0x0080
#define JOY_1 0
#define HSCROLL_PLANE 0
#define VSCROLL_PLANE 0
#define TILE_SYSTEM_INDEX 0
#define TILE_SYSTEM_LENGTH 16
#define FONT_LENGTH 96
#define TILE_USER_INDEX 16
#define SPR_MIN_DEPTH 0
#define SPR_FLAG_AUTO_TILE_UPLOAD 1
#define SPR_FLAG_INSERT_HEAD 2
#define TILE_ATTR_PALETTE_SFT 13
#define TILE_ATTR_PALETTE_MASK (3u << TILE_ATTR_PALETTE_SFT)
#define RGB24_TO_VDPCOLOR(v) ((u16)(v))
#define TILE_ATTR(pal,pri,v,h) ((u16)(((pal) << 13) | ((pri) << 15) | ((v) << 12) | ((h) << 11)))
#define TILE_ATTR_FULL(pal, pri, v, h, tile) \
    ((u16)(((pal) << 13) | ((pri) << 15) | ((v) << 12) | ((h) << 11) | (tile)))
struct Palette { const u16 *data; };
typedef struct Palette Palette;
typedef struct { const u16 *data; u16 numTile; } TileSet;
typedef struct AnimationFrame
{
    TileSet *tileset;
} AnimationFrame;
typedef struct Animation
{
    u8 numFrame;
    u8 loop;
    AnimationFrame **frames;
} Animation;
typedef struct
{
    u16 w;
    u16 h;
    Palette *palette;
    u16 numAnimation;
    Animation **animations;
    u16 maxNumTile;
    u16 maxNumSprite;
} SpriteDefinition;
typedef struct { int unused; } MapDefinition;
typedef struct Map
{
    u16 plane;
    u16 baseTile;
    u16 planeWidth;
    u16 planeHeight;
    u32 posX;
    u32 posY;
} Map;
typedef enum { ROW_UPDATE, COLUMN_UPDATE } MapUpdateType;
typedef void MapDataPatchCallback(Map *map, u16 *buffer, u16 x, u16 y,
                                  MapUpdateType updateType, u16 size);
typedef struct Sprite Sprite;
void PAL_setPalette(u16 palette, const u16 *data, u16 transfer);
void VDP_setScreenWidth320(void);
void VDP_setScreenHeight224(void);
void VDP_setScrollingMode(u16 h, u16 v);
void VDP_clearPlane(u16 plane, bool wait);
void VDP_setHorizontalScroll(u16 plane, s16 value);
void VDP_setVerticalScroll(u16 plane, s16 value);
void VDP_setTextPlane(u16 plane);
void VDP_setTextPalette(u16 palette);
void VDP_setTextPriority(u16 priority);
void VDP_drawTextBGFill(u16 plane, const char *text, u16 x, u16 y, u16 len);
void VDP_showFPS(u16 asFloat, u16 x, u16 y);
u32 SYS_getFPS(void);
void VDP_loadTileSet(const TileSet *tiles, u16 index, u16 transfer);
void VDP_fillTileData(u8 value, u16 index, u16 num, bool wait);
void VDP_setTileMapXY(u16 plane, u16 tile, u16 x, u16 y);
Map *MAP_create(const MapDefinition *def, u16 plane, u16 attr);
void MAP_scrollTo(Map *map, u32 x, u32 y);
void MAP_scrollToEx(Map *map, u32 x, u32 y, bool forceRedraw);
u16 MAP_getTile(Map *map, u16 x, u16 y);
void VDP_setTileMapDataRow(u16 plane, const u16 *data, u16 row,
                           u16 x, u16 width, u16 transfer);
void MAP_setDataPatchCallback(Map *map, MapDataPatchCallback *callback);
void MAP_release(Map *map);
void JOY_init(void);
u16 JOY_readJoypad(u16 joy);
void SYS_doVBlankProcess(void);
void KLog(const char *text);
void KLog_U2(char *t1, u32 v1, char *t2, u32 v2);
void kprintf(const char *fmt, ...);
void uintToStr(u32 value, char *dest, u16 minDigits);
void intToStr(s32 value, char *dest, u16 minDigits);
#endif
