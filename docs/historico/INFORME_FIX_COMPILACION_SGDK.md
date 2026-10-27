# Corrección de compilación: orden de cabeceras SGDK

## Error observado

Al compilar `src/fog_of_war.c` con SGDK, aparecían errores como `unknown type name 'u16'`, `s8`, `fix16` y otros dentro de `inc/string.h`.

## Causa

El archivo incluía `<string.h>` antes de `fog_of_war.h`, por lo que la cabecera de cadenas de SGDK se procesaba antes de que `genesis.h` declarase sus tipos. La implementación de `string.h` de esta distribución SGDK utiliza esos tipos en sus prototipos.

## Cambio aplicado

Se reordenaron los includes de `src/fog_of_war.c`:

```c
/* La cabecera SGDK string.h usa tipos definidos por genesis.h. */
#include "fog_of_war.h"
#include <string.h>
```

`fog_of_war.h` incluye `genesis.h`, por lo que los tipos SGDK ya están definidos al procesar `string.h`. No se ha cambiado la lógica de niebla ni el comportamiento del juego con esta corrección.

## Verificación

- `CC=gcc sh tests/run_tests.sh`: pasan las 18 secciones.
- Las pruebas incluyen una comprobación específica del orden de inclusión SGDK.
- ZIP reconstruido e íntegro; pruebas repetidas desde una extracción limpia.

La compilación con la instalación SGDK de Windows debe confirmarse ejecutando `make` en el equipo objetivo; este entorno no dispone de esa instalación.
