# Cierre de integración — pre-ROM

## Estado de esta revisión

Esta revisión parte de `megadrive_strategy_sgdk_40x40_120_unidades_fix_pausa_seleccion.zip`, la última edición de trabajo con 120 unidades, niebla persistente, IA, línea de visión/alcances por clase y refresco localizado al seleccionar.

### Comprobaciones ejecutadas en este entorno

- `sh tests/run_tests.sh`: 23/23 grupos y prueba adicional de perfilado superados.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos y prueba adicional superados con ASan/UBSan.
- GCC 14.2, `-Wall -Wextra -Werror -Wformat=2 -fanalyzer`: todos los módulos de `src/` pasan con stubs SGDK.
- Prueba del orden de inclusiones: todos los módulos se compilan con un shim que reproduce la dependencia de tipos `u16`/`s8` de `string.h` de SGDK.
- Recursos: las declaraciones `MAP`, `TILESET`, `PALETTE` y `SPRITE` apuntan a ficheros existentes; los PNG del directorio `res/gfx` son indexados. Ambos mapas gráficos son de 968×968 px; el atlas de terreno es de 96×24 px; las cuatro unidades son 16×16 px; cursor 24×24 px; reserva de niebla 8×8 px.
- `unzip -t`: integridad del paquete validada tras generarlo.

Estas pruebas son pruebas host/estáticas y **no demuestran** que la ROM compile con el toolchain SGDK real, que las transferencias VDP sean correctas en emulación o que se mantenga un objetivo concreto de FPS.

## Ajuste al lanzador Windows

`tools/build_sgdk.bat` fija ahora `GDK` a la instalación suministrada y antepone su `bin` al `PATH`, en vez de depender de variables de entorno globales configuradas por separado. Ofrece builds normales, de depuración y limpias (`clean`/`debug-clean`) que eliminan `out` para evitar dependencias `.d` obsoletas tras cambiar fuentes o recursos.

## Compilación en la máquina con SGDK

Abrir `cmd.exe` en la raíz del proyecto extraído y ejecutar:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" clean
```

Para depuración, incluyendo símbolos:

```bat
tools\build_sgdk.bat "D:\MDDev\SGDKs\SGDK-master" debug-clean
```

La ruta debe corresponder a la instalación que realmente se usa en el equipo. El resultado esperado es `out\rom.bin`. Si falla, conservar la salida completa y corregir **el primer error de compilación o de `rescomp`**, no los errores en cascada (por ejemplo dependencias `.d` que no llegaron a generarse).

## Validación visual en BlastEm

Usar primero la ROM de diagnóstico y verificar en este orden:

1. Arranque y selector de escenarios; no debe haber tiles corruptos o glifos blancos.
2. Seleccionar y cancelar unidades; el alcance y los blancos de ataque deben aparecer sin congelación visible.
3. Mover unidades en las cuatro direcciones, llegar al destino y comprobar centrado y ocupación.
4. Comprobar que un blindado no dispara a través de bosque/roca; verificar también represalia y eliminación.
5. Verificar ocultación por niebla, exploración persistente y el modo espectador durante el turno rojo.
6. Completar una partida y usar A para repetir; B/START para volver al selector. Alternar escenarios varias veces.
7. Observar el límite de sprites por línea cuando se acumulan muchas unidades. El motor conserva 120 unidades lógicas, pero limita las instancias de unidad a 77, además de tres sprites fijos.
8. Recoger el overlay FPS y los bloques `PERF` durante `IDLE`, `PLAYER_SCROLL`, `PLAYER_MOVE`, `AI_DECISION` y `AI_MOVE_CAMERA`. La instrumentación añade sobrecarga y sus contadores no son ciclos de CPU; sirven para comparar trabajo por actividad.

## Criterio de cierre

La integración solo podrá darse por cerrada tras una compilación SGDK real que produzca `out/rom.bin` y una sesión visual de BlastEm que valide los puntos anteriores. El entorno de esta revisión no dispone de la instalación SGDK/BlastEm del usuario, por lo que dichos pasos no se declaran realizados.
