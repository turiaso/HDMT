#ifndef DEBUG_H
#define DEBUG_H
/**
 * UTILS
 */

#define HALT(fmt, ...)                                                                                                                               \
    do {                                                                                                                                             \
        /*TEXT_DRAW_F(fmt, 0, 0, TEXT_ALIGN_L, __VA_ARGS__);*/                                                                                       \
        LOG_INFO(fmt, __VA_ARGS__);                                                                                                                  \
        while (1)                                                                                                                                    \
            SYS_doVBlankProcess();                                                                                                                   \
    } while (0)

#define HALT_IF(condition, fmt, ...)                                                                                                                 \
    if (condition) {                                                                                                                                 \
        HALT(fmt, __VA_ARGS__);                                                                                                                      \
    }

#define BREAKPOINT(fmt, ...)                                                                                                                         \
    do {                                                                                                                                             \
        /*TEXT_DRAW_F(fmt, 0, 0, TEXT_ALIGN_L, __VA_ARGS__);*/                                                                                       \
        LOG_INFO(fmt, __VA_ARGS__);                                                                                                                  \
        while (!(JOY_readJoypad(JOY_1) & BUTTON_START))                                                                                              \
            SYS_doVBlankProcess();                                                                                                                   \
        while (JOY_readJoypad(JOY_1) & BUTTON_START)                                                                                                 \
            SYS_doVBlankProcess();                                                                                                                   \
    } while (0)

#define BREAKPOINT_IF(condition, fmt, ...)                                                                                                           \
    if (condition) {                                                                                                                                 \
        BREAKPOINT(fmt, __VA_ARGS__);                                                                                                                \
    }

#endif /* DEBUG_H */