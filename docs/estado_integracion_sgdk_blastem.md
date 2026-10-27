# Estado de integración SGDK y BlastEm

## Resultado actual

**Las pruebas de lógica y la validación estática del proyecto pasan. La ROM real no está validada.** No se debe considerar cerrada la integración hasta generar una ROM con SGDK y arrancarla en BlastEm.

## Comprobaciones ejecutadas en esta revisión

- `sh tests/run_tests.sh`: superado; 23 grupos y pruebas adicionales, incluida la simulación de IA con 120 unidades.
- `sh tests/run_tests_sanitized.sh`: superado; ASan/UBSan, propiedades de pathfinding y estrés de combate con 120 unidades.
- `sh tools/check_source_quality.sh`: superado; 42 archivos C/H y GCC `-fanalyzer` sin diagnósticos.
- `python3 tools/check_resources.py`: superado; 11 declaraciones de recursos SGDK, referencias a tilesets y 12 PNG verificados por estructura/CRC, paleta indexada y dimensiones de los recursos principales.
- `tools/audit_host.sh`: reúne estas comprobaciones en un comando reproducible.
- Se ejecutó desde una copia extraída del ZIP final, no desde un árbol parcial.

El comprobador de recursos es preventivo: no reemplaza `rescomp` de SGDK y no prueba compatibilidad de las imágenes con la versión real de esa herramienta.

## Bloqueo de compilación real

No se pudo ejecutar la compilación de ROM en este entorno porque faltan las herramientas requeridas:

- `GDK` no está configurado y no se ha encontrado `makefile.gen` de SGDK.
- No está disponible `m68k-elf-gcc` (ni un compilador cruzado alternativo para esta diana).
- No está instalado BlastEm.
- Se intentó comprobar el acceso a la fuente oficial, pero la resolución DNS no funcionó; `apt-get update` agotó su límite de ejecución. No pude descargar ni instalar herramientas.

Por tanto, **no se generó una ROM nueva y no están verificados el arranque, el VDP, el aspecto visual ni los FPS dentro de un emulador**. El error de preflight del build es la ausencia de `GDK`, no un error de compilación del código fuente.

## Compilación reproducible cuando SGDK esté disponible

En Linux o macOS, desde la raíz del proyecto:

```sh
export GDK="/ruta/a/SGDK"
export PATH="$GDK/bin:$PATH"
tools/build_sgdk.sh
```

Cuando la compilación indique que la ROM se ha generado, arrancarla con BlastEm (ajustando el nombre si se genera `out/rom.gen`):

```sh
blastem out/rom.bin
```

No dar por superada la integración hasta observar el selector, iniciar los tres escenarios y revisar movimiento, niebla, bordes de cámara, HUD/FPS, turnos, combate, objetivo y pantalla final. El plan de pruebas visuales está en `docs/pruebas_regresion_emulador.md`.
