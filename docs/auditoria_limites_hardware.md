# Auditoría estática de límites de hardware

## Resultado

La comprobación `tools/audit_hardware_budget.py` valida los presupuestos que se pueden demostrar a partir de la configuración y los recursos del proyecto:

- **Atlas de terreno:** 96 × 24 píxeles, equivalente a 36 patrones de 8 × 8.
- **Gráficos de unidades:** 16 patrones reservados, en los índices `[464, 480)`, compartidos por cuatro tipos de unidad.
- **Cursor:** 9 patrones en `[500, 509)`, dentro de los 512 patrones de VRAM.
- **Sprites SAT:** 80 entradas de hardware; el juego reserva 3 para cursor, marcador y niebla, y limita las unidades a 77 sprites activos.
- **Capacidad lógica:** 128 unidades; el mayor escenario despliega 120.

Los rangos anteriores son deliberadamente independientes de la posición del atlas de terreno: SGDK determina su comienzo mediante `TILE_SYSTEM_INDEX`, `TILE_SYSTEM_LENGTH` y `FONT_LENGTH`. `initWorldMap()` comprueba en ejecución que el atlas no invade la zona de unidades y que queda un patrón para la niebla. El código también valida que los patrones del cursor no superen el final de VRAM.

## Límites que la auditoría estática no puede certificar

- El VDP de Mega Drive admite como máximo **20 sprites por línea de exploración**. Tener como máximo 80 entradas en la SAT no garantiza que una escena con unidades alineadas verticalmente se dibuje completa; el límite por línea depende de la colocación real y debe verificarse en emulador/hardware.
- El presupuesto de sprites limita las unidades visibles simultáneas a 77. El escenario puede tener 120 unidades lógicas; la vista administra las instancias visibles sin reservar un sprite hardware permanente por unidad.
- Sin `GDK` no se puede confirmar el mapa de VRAM final generado por SGDK ni compilar la ROM. Tampoco se puede sustituir una prueba de rendimiento en BlastEm o consola real.
- Las métricas de `performance.c` cuentan operaciones por fase; no son una medición de ciclos exactos de CPU. Deben activarse solo para diagnóstico, ya que los logs alteran el rendimiento observado.

## Decisión

No se modificaron la cámara, la niebla, la precarga de gráficos ni el sistema de eventos. La revisión no encontró una prueba estática suficiente para justificar cambios en esos subsistemas. La nueva comprobación se incorpora a `tools/audit_host.sh` para detectar regresiones en los presupuestos explícitos de VRAM y sprites.

## Densidad de sprites por línea

`tools/audit_sprite_scanlines.py` calcula la ocupación vertical de los despliegues iniciales usando la altura real del sprite y el espaciado de celdas. Añade conservadoramente las tres entradas fijas al pico de unidades. Los despliegues iniciales pasan con margen respecto al límite de 20 sprites por línea.

Esta comprobación no predice todos los estados posibles de la partida: el movimiento puede agrupar unidades en la misma franja horizontal y el cursor/objetivo cambian de posición. La certificación final requiere probar la ROM con unidades concentradas y moviéndose en BlastEm o en hardware.


## Cota geométrica conservadora de sprites por línea

La auditoría también calcula una cota horizontal basada en el ancho de pantalla,
el ancho del sprite y el paso entre celdas. Cuenta sprites que toquen parcialmente
los bordes y añade las tres entradas fijas. Con la configuración actual, la cota
es de 14 unidades + 3 sprites fijos = 17 de los 20 permitidos por línea. Los
despliegues iniciales quedan por debajo de esa cota. Esto depende de que las
unidades mantengan sus posiciones alineadas a la cuadrícula de 24 px; no sustituye
la validación visual de una ROM real ni cubre sprites adicionales no contabilizados
por `FIXED_GAME_SPRITE_COUNT`.
