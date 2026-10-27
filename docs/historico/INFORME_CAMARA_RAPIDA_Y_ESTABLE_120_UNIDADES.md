# Ajuste de velocidad y estabilidad de cámara (120 unidades)

## Cambios

- Exploración libre: `CAMERA_STEP` pasa de 2 a 4 px/frame y `CAMERA_MAX_STEP` de 4 a 8. La aceleración se aplica cada 2 frames en lugar de cada 4; al cabo de 10 frames de mantener el paneo, la cámara ha avanzado al menos 55 px, frente a la rampa anterior de 2–4 px/frame.
- Seguimiento automático: velocidad de 10 px/frame (antes 6), suficiente para recuperar margen respecto a una unidad que anima a 4 px/frame. Se conserva por debajo de 16 px/frame para que el motor MAP no tenga que recuperar varias filas/columnas en una llamada continua.
- Modo de selección de destino: el desplazamiento por celdas espera 4 frames por celda en vez de 8, manteniendo el cursor alineado y dentro de la máscara de movimiento.
- Asignación de sprites cuando la SAT está llena: primero se reposicionan y liberan los sprites que ya existen; después se procesan candidatos entrantes con una estructura de bits por prioridad (sin búsquedas ni desplazamientos de una cola lineal). Como máximo se crea/reemplaza una instancia por fotograma. Los candidatos provienen solo de las filas/columnas que entran en el viewport; la lista lógica de 120 unidades no se modifica.
- Se añadieron comprobaciones de compilación para impedir futuras velocidades continuas de 16 px/frame o más sin una revisión deliberada del camino de scroll.

## Validación

- `sh tests/run_tests.sh`: 23/23 grupos pasan.
- `sh tests/run_tests_sanitized.sh`: 23/23 grupos pasan con ASan/UBSan.
- Prueba nueva de paneo manual: tras 10 llamadas con dirección constante, el desplazamiento es >=55 px y el cursor permanece fijo en pantalla.
- Prueba de seguimiento automático: cada actualización inicial avanza exactamente `CAMERA_AUTO_STEP` por eje y la cámara llega al destino con límites válidos.
- Prueba de sprites entrantes: cuando varias unidades atraviesan el mismo borde, el candidato seleccionado se admite primero y las otras entradas quedan representadas en la máscara de bits sin duplicados ni desplazamiento de cola.
- La simulación del turno de IA de 120 unidades sigue terminando y conserva la integridad de movimiento, combate y niebla.

## Lo que no se ha podido medir aquí

Este entorno no tiene `GDK` configurado ni ejecutable `BlastEm`, por lo que no se ha generado ni ejecutado una ROM real. Las pruebas verifican la lógica y el límite de trabajo por actualización, pero no demuestran una tasa de 60 FPS en la Mega Drive/emulador. El objetivo es conservar el refresco del modo de vídeo seleccionado; una garantía de 60 FPS requiere medir el build real en modo NTSC de 60 Hz. En modo PAL, el refresco estándar es de 50 Hz, por lo que no puede haber 60 fotogramas presentados por segundo.

## Archivos modificados

- `src/game_config.h`: paneo manual, desplazamiento en selección, velocidad automática y guardarraíl del límite de 16 px.
- `src/camera_cursor.c`: usa `CAMERA_AUTO_STEP` para ambos ejes.
- `src/unit_view.c`: reduce las sustituciones repetidas de sprites durante el scroll con SAT llena.
- `tests/camera_cursor_test.c`: pruebas de velocidad y autoencuadre ajustadas a las nuevas constantes.
