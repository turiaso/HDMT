# Validación de IA en los tres escenarios

## Objetivo

Ejecutar un turno completo del equipo rojo en cada escenario registrado y comprobar que la simulación termina sin corromper la ocupación del mapa.

## Cobertura

- Punto de control: 40×40, 120 unidades.
- Defensa del puente: 40×40, 24 unidades.
- Puente del desfiladero: 20×20, 12 unidades.
- Inicialización real de mapa, despliegue, controlador de unidades, objetivo y estado de turno.
- Movimiento animado paso a paso a través del controlador común.
- Resolución de decisiones de IA y comprobación de objetivos durante el turno.
- Verificación espacial tras el turno: toda unidad viva coincide con el ocupante de su celda, ninguna comparte posición y cada ocupante del mapa apunta a una unidad viva en esas coordenadas.
- Límite de 50.000 iteraciones para detectar bloqueos.

## Ejecución

La prueba `tests/ai_scenario_smoke_test.c` se ejecuta desde `tests/run_tests.sh` y, por tanto, también queda incluida en la validación host y en la ejecución sanitizada que reutiliza esa suite.

## Resultado observado

Los tres turnos completan y pasan la comprobación espacial. En la disposición inicial de prueba, la IA invierte el turno principalmente en aproximarse; esta prueba no pretende medir la calidad táctica de una batalla completa ni sustituye la validación visual en emulador.

## Límites

- Los recursos SGDK generados se sustituyen por símbolos host: no se compila una ROM.
- El test no valida cámara, renderizado, niebla visible ni límites reales del VDP.
- Los ataques y reparaciones dependen de la situación de cada turno; las pruebas específicas de combate y la simulación de estrés de 120 unidades siguen siendo pruebas separadas.
