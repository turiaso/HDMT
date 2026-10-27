# Corrección de fluidez durante el movimiento

## Causa encontrada

Durante el turno de la IA, la cámara se recentraba sobre el centro de la unidad móvil en cada fotograma. Eso hacía que el scroll del mapa y la reposición de los sprites visibles se ejecutaran prácticamente en todos los fotogramas de la ruta. Además, al cruzar cada casilla se repintaba el HUD con la información de esa casilla y se llamaba a `GameTurn_checkObjective`, que puede recorrer unidades aunque el objetivo no esté afectado por ese paso. El trabajo extra aparecía justo en las fronteras de celda y se percibía como pequeños parones.

## Cambios

- La cámara de espectador de la IA ahora tiene una zona muerta: mantiene el encuadre durante la parte central del recorrido y solo empieza a seguir a la unidad cuando se acerca a un borde. Una vez comienza el seguimiento, se mantiene suave; no se bloquea ni se pausa la animación.
- El HUD de celda ya no se repinta por cada casilla atravesada. Se actualiza una sola vez al finalizar la ruta, con la celda del actor que queda enfocado.
- La comprobación del objetivo se limita a entradas/salidas de la casilla objetivo para misiones de captura o mantener posición. Los objetivos de eliminación/supervivencia no se vuelven a contar en cada paso de movimiento, porque un movimiento normal no cambia el número de unidades vivas.
- Se conserva la interpolación subcelda y el ajuste final exacto del sprite. La velocidad por fotograma no se ha alterado.
- Se conserva la actualización diferida de la niebla al final de la ruta y el registro ligero de las casillas que quedan libres.

## Validación

- `tests/run_tests.sh`: pasan las 23 comprobaciones.
- `tests/run_tests_sanitized.sh`: pasan las 23 comprobaciones con ASan/UBSan.
- Las pruebas existentes verifican la continuidad de la posición renderizada, que el sprite termina centrado en la casilla lógica, el turno completo de IA con 60 unidades y 20.000 mapas aleatorios de pathfinding.

No se ha medido el FPS real ni ejecutado una ROM en BlastEm en este entorno; la mejora final de fotogramas debe confirmarse con el contador de FPS integrado en la ROM.
