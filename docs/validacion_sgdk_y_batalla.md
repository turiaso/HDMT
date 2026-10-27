# Validación de integración SGDK, batalla y rendimiento

## 1. Qué se valida en host

Ejecutar `tools/audit_host.sh` desde una shell compatible con POSIX. Lanza la suite funcional, la suite ASan/UBSan y GCC `-fanalyzer` para todos los módulos de `src/`.

La prueba `tests/ai_combat_stress_test.c` usa 120 unidades y fuerza contacto entre los frentes. Después de cada movimiento terminado y cada ataque valida:

- Cada unidad viva apunta a una casilla cuyo `occupantId` coincide con su ID.
- No existen dos unidades vivas en la misma coordenada.
- Toda casilla ocupada referencia una unidad viva en esa misma coordenada.
- El turno de la IA termina y se registran movimientos, ataques, reparaciones y bajas.

La visibilidad roja se fuerza a toda la cuadrícula en esta prueba para aislar combate/decisiones. La niebla y la selección de objetivos ocultos se prueban en pruebas específicas separadas; este escenario no sustituye esos casos.

## 2. Build real de SGDK

En Linux o macOS, desde la raíz del proyecto, configura tu instalación real y ejecuta el script de compilación:

```sh
export GDK="/ruta/a/SGDK"
export PATH="$GDK/bin:$PATH"
tools/build_sgdk.sh
```

El script comprueba `makefile.gen`, `make`, Java y `m68k-elf-gcc`, ejecuta el makefile de SGDK y verifica que la ROM generada no esté vacía. Para limpiar la salida antes de compilar:

```sh
tools/build_sgdk.sh clean
tools/build_sgdk.sh
```

Después, si BlastEm está instalado, arranca la ROM generada:

```sh
blastem out/rom.bin
```

Si se genera `out/rom.gen`, usa esa ruta en su lugar. **No marcar el build como superado hasta que la compilación termine correctamente y el emulador arranque la ROM.** Conserva la salida completa si falla y corrige primero el primer error real de C o `rescomp`; los siguientes pueden ser consecuencia del primero.

## 3. Checklist de batalla en BlastEm

1. Arrancar cada escenario desde el selector y repetirlo varias veces.
2. Mover unidades en los cuatro sentidos, con giros, bordes, esquinas y rutas bloqueadas durante el trayecto.
3. Verificar que cada sprite acaba exactamente centrado y que el mapa no permite ocupar dos veces una casilla.
4. Ejecutar turnos rojos completos con enemigos visibles, ocultos y objetivos que mueren durante el turno.
5. Comprobar ataques por alcance/clase, línea de visión, cobertura, reparaciones, retirada y captura/defensa del objetivo.
6. Desplazar la cámara mientras hay unidades entrando/saliendo de pantalla; revisar que no hay sprites fantasma, parpadeos persistentes ni saltos de scroll.
7. Repetir en el escenario con puente/agua y comprobar que la IA no atraviesa terreno prohibido.
8. Jugar varias rondas seguidas y repetir misión/volver al selector para detectar corrupción de estado.

## 4. Rendimiento

`PERF_DIAGNOSTICS=1` instrumenta contadores de operaciones por fase: scroll/SAT, niebla/celdas/filas, búsquedas/nodos, reconstrucciones de alcance, registros de unidades, candidatos de IA, movimientos y ataques. Los contadores no son ciclos exactos de CPU ni FPS.

Hacer una compilación de diagnóstico cambiando temporalmente `PERF_DIAGNOSTICS` en `src/game_config.h` a `1`, ejecutar exactamente 120 fotogramas de una sola actividad y guardar las líneas `PERF PHASE ...`. Medir por separado reposo, scroll, movimiento del jugador, decisión de IA y movimiento de IA; no mezclar fases. Después restaurar el valor normal y recompilar.

Para comparar builds, registrar versión de SGDK, emulador, escenario, número de unidades vivas, fase, contadores y cualquier caída visible de fotogramas. No inferir rendimiento de Mega Drive a partir de los tiempos/iteraciones del harness host.

## 5. Estado honesto de validación

Las pruebas host verifican lógica y consistencia, no el VDP real. La compilación SGDK, el arranque de ROM y la medición de rendimiento en BlastEm deben ejecutarse en el equipo que tenga SGDK y el emulador instalados. No se deben dar por completados esos puntos solo porque pasen las pruebas host.
