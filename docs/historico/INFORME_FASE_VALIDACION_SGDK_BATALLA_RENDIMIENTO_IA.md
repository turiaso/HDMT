# Fase de integración: SGDK, batalla, rendimiento e IA

## Alcance de esta entrega

Esta fase cubre los cuatro frentes solicitados, distinguiendo con precisión lo validado en host de lo que exige SGDK y el emulador reales.

### 1. Build real de SGDK

- Se añadió `tools/build_and_launch_sgdk.bat` para comprobar `GDK`, `makefile.gen`, `make`, ejecutar el build, comprobar que existe una ROM no vacía y lanzar BlastEm si está en `PATH`.
- En el entorno de trabajo actual `GDK` no está definido y no se encuentran `m68k-elf-gcc` ni `blastem`. Por tanto, aquí no se pudo compilar ni arrancar una ROM real. No se marca este punto como superado.

### 2. Batalla con 120 unidades

Se reforzó `tests/ai_combat_stress_test.c`. El escenario determinista de 40x40 contiene 60 unidades por equipo y fuerza contacto entre frentes. Durante el turno de la IA roja se comprueba la integridad espacial tras cada movimiento completado y ataque, y al terminar:

- Cada unidad viva coincide con el `occupantId` de su celda.
- No hay dos unidades vivas en la misma coordenada.
- Cada celda ocupada referencia una unidad viva en la coordenada correcta.
- La IA termina dentro del límite de iteraciones.

Resultado host reproducible: 73 decisiones, 12 movimientos, 18 ataques, 2 reparaciones, 1 baja, 31 comprobaciones de integridad y 390 iteraciones.

**Límite de la prueba:** solo ejecuta el turno de la IA roja; las unidades azules no tienen una IA adversaria en esta prueba. La visión roja se fuerza para aislar combate/decisiones. No es todavía una simulación autónoma de ambos ejércitos ni sustituye una partida en emulador.

### 3. Rendimiento

- Se añadió `tools/audit_host.sh` para ejecutar suite normal, suite ASan/UBSan y GCC `-fanalyzer` sobre todos los módulos `src/*.c`.
- Se documentó cómo recopilar los contadores `PERF_DIAGNOSTICS` por fase en `docs/validacion_sgdk_y_batalla.md`.
- Los contadores existentes miden operaciones, no ciclos de CPU ni FPS. Sin ROM real no se reportan tiempos de VDP/CPU ni rendimiento en Mega Drive.

### 4. IA y equilibrio

Se preservaron las reglas tácticas existentes (foco de fuego, eliminación segura, retirada, reparación, roles, terreno y visibilidad) y se añadieron más puntos de comprobación a la simulación de combate para detectar corrupción de ocupación después de ataques y movimientos. No se cambiaron pesos de combate basándose en la prueba host: una única ronda asimétrica no ofrece evidencia suficiente para ajustar el equilibrio sin riesgo de empeorar otras situaciones. La siguiente calibración debe usar partidas reproducibles con turnos de ambos equipos y resultados observables en emulador.

## Validación ejecutada

- `sh tests/run_tests.sh`: 23 grupos, prueba de perfilado y estrés de combate superados.
- `sh tests/run_tests_sanitized.sh`: los mismos grupos, 20.000 mapas aleatorios y estrés con ASan/UBSan superados.
- GCC `-fanalyzer` con `-Wall -Wextra -Werror -Wformat=2`: todos los módulos de `src/*.c` superados.
- ZIP comprobado con `unzip -t`.

## Próximo paso obligatorio

En Windows, desde la raíz del proyecto, configurar `GDK` a la raíz de SGDK y ejecutar `tools\build_and_launch_sgdk.bat`. Solo después de compilar y arrancar la ROM se deben registrar los resultados de la lista de comprobación de `docs/validacion_sgdk_y_batalla.md` y ajustar el equilibrio de IA a partir de evidencia real.
