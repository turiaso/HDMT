# Corrección de compilación SGDK: orden de inclusión de `string.h`

## Causa

`src/combat.c` incluía `<string.h>` antes de `combat.h`. En SGDK, `string.h` declara funciones usando tipos como `u16` y `s8`, que todavía no estaban definidos en ese punto. En el entorno host, la cabecera estándar no reproduce esa dependencia, por eso las pruebas previas no lo detectaban.

## Corrección

- `combat.c` incluye primero `combat.h`, que incluye `genesis.h` y establece los tipos SGDK, y después `<string.h>`.
- La prueba de orden de inclusiones (`[9/23]` en `tests/run_tests.sh`) ahora compila todos los módulos C con un `string.h` de prueba que exige la disponibilidad de `u16`. Así, un error similar en otro módulo falla en la suite antes de crear el ZIP.

## Verificación

Las suites de pruebas normal y ASan/UBSan se ejecutan en host. También se ejecuta el chequeo estricto de includes en todos los módulos, no solo en `pathfinder.c`.

Este arreglo corrige el error de cabecera indicado en el log de SGDK. No sustituye una compilación cruzada completa con la instalación local de SGDK.
