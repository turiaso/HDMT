# Corrección: precarga de sprites durante el desplazamiento de cámara

## Síntoma

Al desplazar la cámara, algunas unidades aparecían cuando su celda ya era visible. La vista solo admitía nuevos sprites al cruzar el umbral de entrada del viewport, y la admisión de una unidad por fotograma podía retrasar su aparición cuando entraban varias unidades simultáneamente.

## Cambio aplicado

- `src/unit_view.c` utiliza un área de precarga ampliada en una celda lógica (24 px) alrededor del viewport.
- Los sprites se crean y se conservan dentro de ese margen, de forma que SGDK ya tiene preparada la instancia cuando el mapa entra en pantalla. El VDP recorta naturalmente la parte situada fuera del área visible.
- La búsqueda de candidatos usa el mismo margen de precarga.
- Al cruzar el borde de entrada, el índice espacial añade también la celda adyacente por delante del movimiento de cámara, anticipando el trabajo de creación.
- Se mantiene la admisión acotada de un candidato por fotograma para evitar picos al llenar la SAT, junto con la prioridad de sprites y el límite de hardware.
- Se amplían las pruebas de regresión para comprobar que una unidad se registra antes de entrar en el viewport y se mantiene hasta salir del margen de precarga.

## Validación

- `sh tests/run_tests.sh`: suite completa superada (23 grupos y pruebas adicionales).
- `sh tests/run_tests_sanitized.sh`: suite completa superada con ASan/UBSan donde están disponibles.
- Se mantiene el límite de 77 sprites de unidades más los sprites fijos del juego.

La validación se ha realizado en las pruebas host del proyecto. No se ha compilado una ROM con SGDK real ni ejecutado en BlastEm/hardware en este entorno.
