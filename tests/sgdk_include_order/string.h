#ifndef TEST_SGDK_STRING_H
#define TEST_SGDK_STRING_H

/* Emula la dependencia de SGDK string.h en tipos definidos por genesis.h. */
#include_next <string.h>

u16 sgdk_string_requires_types(u16 value);

#endif
