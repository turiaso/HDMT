# Niebla de guerra conservada durante el turno de IA

## Comportamiento implementado

- Al cambiar del equipo Azul a Rojo, guarda la posición actual de la cámara y oculta el cursor de selección.
- La cámara sigue suavemente a la unidad roja que va a actuar. La IA espera a que la cámara centre al siguiente actor, y los ataques permanecen en pantalla durante 30 fotogramas.
- **La niebla no se desactiva durante el turno rojo.** La vista de espectador utiliza la unión de visión de ambos equipos: una casilla queda visible si Azul o Rojo la ve actualmente; queda como terreno explorado si al menos un equipo la conoció previamente; permanece bajo la niebla opaca solo si los dos equipos la desconocen.
- Las unidades se muestran si su casilla está dentro del alcance de visión actual de cualquiera de los dos equipos. Las posiciones enemigas fuera de la visión compartida siguen ocultas. Los datos de visión y exploración de Azul y Rojo permanecen guardados por separado.
- Los cambios de visión causados por el movimiento de la IA actualizan solo las filas de tiles afectadas, manteniendo el refresco incremental.
- El HUD inspecciona la casilla del actor sin disparar eventos de cursor. Si ambos equipos desconocen la casilla, muestra terreno y ocupación desconocidos.
- Al acabar el turno rojo, se restaura automáticamente la perspectiva normal de Azul, el encuadre previo y el cursor después del retorno suave de la cámara.

## Pruebas ejecutadas

- `tests/run_tests.sh`: 23/23 grupos superados.
- Los ejecutables de la suite se compilan con `-fsanitize=address,undefined` donde está disponible el GCC del entorno.
- Prueba de estado combinado: visible si ve cualquier equipo, explorado si cualquier equipo lo recuerda, desconocido únicamente si ambos lo desconocen.
- Pruebas del mapa visual verifican que una celda vista solo por Azul se muestra durante el turno de Rojo, mientras una celda desconocida para ambos sigue usando el tile de niebla.
- Prueba de unidades confirma que la perspectiva compartida muestra unidades visibles por cualquiera de los equipos.
- Prueba del HUD confirma que los datos de terreno se muestran con visión compartida y permanecen ocultos si los dos equipos desconocen la celda.

## Pendiente de comprobación en la ROM

No se ha compilado una ROM desde este entorno porque no está configurado SGDK (`GDK`) ni está instalado BlastEm. En Windows, compila con `tools\build_sgdk.bat "D:\ruta\real\a\SGDK"` y prueba:

1. En el turno Azul se mantiene la visión normal de Azul.
2. Al pasar a Rojo, la cámara sigue a sus unidades y las zonas dentro de la visión actual Azul o Roja quedan visibles.
3. Las zonas que ambos equipos desconocen siguen cubiertas.
4. Las unidades que estén fuera de la visión compartida permanecen ocultas.
5. Al terminar la IA se restaura la niebla normal de Azul y la cámara vuelve al encuadre previo.


Nota de evolución: el comportamiento inicial de espectador que ocultaba toda la niebla ha sido sustituido. La implementación actual mantiene la niebla para las zonas desconocidas por ambos bandos y muestra la unión de su visión actual.
