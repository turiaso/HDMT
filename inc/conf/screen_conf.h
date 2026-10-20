#ifndef SCREEN_CONF_H
#define SCREEN_CONF_H

/// Lista de pantallas disponibles en la aplicación
#define SCREEN_MACRO_LIST                                                                                                                              \
    X_MACRO(SCREEN_MAIN)                                                                                                                            \
    X_MACRO(SCREEN_COMPANY)                                                                                                                            \
    X_MACRO(SCREEN_TITLE)                                                                                                                            \
    X_MACRO(SCREEN_BATTLEMAP)        
                                                                                                                        \
 
#include <model/screen/screen.h>

#define SCREEN_SERVICE_INITIAL_SCREEN SCREEN_MAIN

#endif // SCREEN_CONF_H