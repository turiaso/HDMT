# Alcance de ataque por clase y previsualización de blancos

## Alcances vigentes

Los rangos siguen centralizados en `src/unit_types.c`:

- Infantería: 1 celda Manhattan.
- Explorador: 1 celda Manhattan.
- Ingeniero: 1 celda Manhattan.
- Blindado: 2 celdas Manhattan.

No se alteraron daño, defensa, puntos de movimiento, visión, reglas de represalia ni decisiones de misión.

## Resaltado de objetivos atacables

`Combat_buildAttackableMask()` construye una máscara de las celdas ocupadas por enemigos que la unidad seleccionada puede atacar en ese momento. Para cada candidato llama a `Combat_predictUnits()`, de modo que resaltado, vista previa del HUD y ataque real comparten la validación de turno, equipo, acción consumida, vida, visibilidad, distancia Manhattan y línea de visión.

`MovementGlow` aplica el estilo de objetivo en PAL3 sobre la celda del enemigo. La niebla desconocida tiene precedencia sobre el resaltado; las unidades ocultas nunca se marcan. Si el objetivo coincide con una casilla alcanzable por movimiento, al retirar el resaltado recupera su estilo de movimiento subyacente.

El cálculo se realiza solo cuando cambia la selección/estado de acción. Durante scroll y animación no se vuelve a evaluar el conjunto de 120 unidades. Al deseleccionar se repintan solo las filas visuales correspondientes a los blancos marcados.

## Feedback del cursor

Al apuntar a un enemigo visible, el HUD muestra la previsión de daño y represalia si el ataque es legal; informa de forma explícita si el objetivo está fuera de alcance, la línea está bloqueada o la unidad ya ha gastado su ataque.

## Pruebas añadidas

- Se verifica la tabla de rangos de las cuatro clases.
- A distancia 2 solo el blindado obtiene un ataque legal; todas las clases atacan a distancia 1.
- La máscara de objetivos solo marca la unidad realmente atacable, no marca enemigos fuera de alcance, ocultos o detrás de roca.
- Un buffer de máscara de tamaño insuficiente falla de forma segura.
- El estilo de objetivo usa PAL3, se restaura de forma localizada al quitarlo, y no reemplaza el glow de movimiento subyacente.
- La niebla desconocida conserva precedencia sobre la máscara de objetivos.

## Verificación

- `sh tests/run_tests.sh`: 23 grupos y pruebas adicionales superadas.
- `sh tests/run_tests_sanitized.sh`: 23 grupos y pruebas adicionales superadas con ASan/UBSan disponibles.
- GCC `-fanalyzer` y advertencias estrictas: sin diagnósticos en los módulos C.

No se ha compilado una ROM SGDK ni se ha verificado visualmente en BlastEm en este entorno. La comprobación visual pendiente debe confirmar que el tinte rojo bajo los sprites comunica bien el estado de objetivo atacable y que el contador de FPS no empeora al seleccionar/deseleccionar unidades.

## Corrección adicional de compilación SGDK

En la revisión posterior se corrigió el orden de inclusión de `src/combat.c`: `combat.h` debe preceder a `<string.h>` para que los tipos SGDK (`u16`, `s8`) estén definidos antes de leer la cabecera SGDK. La prueba de orden de inclusiones se amplió a todos los módulos tanto en `tests/run_tests.sh` como en `tests/run_tests_sanitized.sh`, con una cabecera de prueba que reproduce esa dependencia. Ver `INFORME_FIX_COMPILACION_SGDK_STRING_INCLUDE.md`.
