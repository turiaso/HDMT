#ifndef TEST_SPRITE_ENG_H
#define TEST_SPRITE_ENG_H
#include <genesis.h>
typedef enum
{
    VISIBLE,
    HIDDEN,
    AUTO_FAST,
    AUTO_SLOW
} SpriteVisibility;
void SPR_init(void);
void SPR_end(void);
Sprite *SPR_addSprite(const SpriteDefinition *def, s16 x, s16 y, u16 attr);
Sprite *SPR_addSpriteEx(const SpriteDefinition *def, s16 x, s16 y, u16 attr, u16 flags);
void SPR_update(void);
void SPR_setPosition(Sprite *sprite, s16 x, s16 y);
void SPR_setPriority(Sprite *sprite, bool priority);
void SPR_setDepth(Sprite *sprite, s16 depth);
void SPR_setVisibility(Sprite *sprite, SpriteVisibility visibility);
SpriteVisibility SPR_getVisibility(Sprite *sprite);
void SPR_releaseSprite(Sprite *sprite);
u16 SPR_getFreeVRAM(void);
u16 SPR_getLargestFreeVRAMBlock(void);
u16 SPR_getNumActiveSprite(void);
u16 SPR_getUsedVDPSprite(void);
#endif
