# Pruebas de regresión en BlastEm

Estas pruebas son manuales y solo se pueden marcar como superadas después de compilar una ROM con SGDK y ejecutarla en BlastEm. Los tests host no sustituyen esta lista.

## Preparación

1. Ejecutar `tools/build_sgdk.sh` y confirmar que el script informa de una ROM no vacía.
2. Abrir la ROM generada con `blastem out/rom.bin` (o el nombre de ROM reportado por SGDK).
3. Activar el contador de FPS disponible en el juego y anotar la versión/hash de la ROM probada.

## Selector y escenarios

- [ ] El selector presenta las cuatro misiones sin texto superpuesto o cortado.
- [ ] «Punto de control» carga el mapa 40×40 y despliega 60 unidades por equipo.
- [ ] «Defensa del puente» carga el mapa 40×40 y despliega 12 unidades por equipo.
- [ ] «Puente del desfiladero» carga el mapa 20×20 y despliega 6 unidades por equipo.
- [ ] «Cruce de las ruinas» carga el mapa 20×20 girado, con 6 unidades por equipo y el objetivo central.
- [ ] Las cuatro opciones y sus descripciones quedan separadas de las instrucciones inferiores.
- [ ] Las unidades aparecen en las celdas de despliegue y no se solapan.

## Cámara, terreno, niebla y sprites

- [ ] Desplazar la cámara hacia los cuatro bordes y las cuatro esquinas del mapa.
- [ ] No aparecen franjas verdes artificiales a la derecha ni en la parte inferior.
- [ ] Las casillas exploradas conservan el estado descubierto y la niebla sigue el equipo activo.
- [ ] Los enemigos ocultos no se ven ni pueden seleccionarse/atacarse.
- [ ] Los sprites entran por los bordes sin aparecer tarde, parpadear ni quedar tapados por la máscara.
- [ ] Tras varios paneos rápidos, todos los sprites se mantienen centrados y alineados con su celda.

## Movimiento, turnos e IA

- [ ] Una unidad realiza como máximo una orden de movimiento por turno.
- [ ] Una ruta bloqueada antes de abandonar el origen puede replantearse; una ruta interrumpida tras avanzar consume la acción.
- [ ] Los puntos se restauran únicamente al comenzar el siguiente turno propio.
- [ ] Ninguna unidad atraviesa agua infranqueable, roca bloqueante ni otra unidad ocupante.
- [ ] La IA termina su turno, respeta obstáculos y niebla, y no repite acciones de una unidad ya gastada.
- [ ] La reparación del ingeniero y los ataques respetan el presupuesto de acción y los objetivos válidos.

## Objetivos y pantalla final

- [ ] «Punto de control» termina al capturar el punto conforme a su regla de misión.
- [ ] Cada escenario de puente exige tres cierres de turno del equipo que mantiene el objetivo; los turnos rivales no cuentan.
- [ ] Una baja libera la celda; bajas simultáneas de ambos equipos producen empate.
- [ ] No se producen cierres de turno duplicados ni avances dobles del objetivo.
- [ ] La pantalla final muestra el resultado correcto y permite repetir la misión o volver al selector.

## Rendimiento y estabilidad

- [ ] El contador de FPS tiene el prefijo y todos sus dígitos visibles, sin solaparse con el HUD.
- [ ] El valor no se congela ni cae a lecturas evidentemente absurdas durante movimiento o IA.
- [ ] Hacer una prueba larga con 120 unidades y varias decenas de turnos sin corrupción gráfica ni bloqueo.
- [ ] Volver al selector y repetir cada escenario sin reiniciar el emulador.

## Registro de ejecución

Anotar la fecha, versión/hash de ROM, versión de BlastEm, escenario, paso que falla y una captura del primer fallo. No marcar la integración como validada mientras haya casillas pendientes o un fallo reproducible.
