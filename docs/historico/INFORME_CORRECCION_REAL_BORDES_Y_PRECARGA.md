# Corrección del borde visible y la precarga de unidades

## Causa

El mapa lógico usa un área alineada a celdas completas de 312x216 px, pero el VDP presenta 320x224 px. La inicialización dibujaba una máscara fija en el último tile de BG_B, independiente del estado de niebla por casilla. En los píxeles transparentes/solapados de esa franja se filtraba el mapa de BG_A y los sprites quedaban por detrás del plano de máscara. Además, la visibilidad de unidades se evaluaba contra el viewport lógico, no contra el área física visible.

## Cambios

- Se dejó de dibujar la máscara geométrica fija durante la partida. La niebla visible del escenario depende exclusivamente del estado de cada casilla.
- La visibilidad de sprites usa el área física de 320x224 px; las unidades parcialmente dentro del borde se muestran.
- La precarga mantiene un margen de una celda (24 px), ahora medido desde los límites físicos de la pantalla. Los sprites del margen siguen ocultos hasta entrar en pantalla.
- Se amplió la prueba de `unit_view` para probar una unidad visible en el borde físico y otra precargada fuera de pantalla que aparece al desplazar la cámara sin recrear su sprite.
- Los límites lógicos de cámara y cursor permanecen alineados a celdas completas.

## Validación y limitación

La corrección de código se somete a la suite de pruebas del proyecto, incluyendo sanitizadores y análisis estático. La desaparición visual definitiva de las franjas solo puede confirmarse ejecutando la ROM recompilada en BlastEm; no se afirma haber realizado esa comprobación en este entorno.
