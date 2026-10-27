# Informe: simulador reproducible de batalla con ambos bandos

## Cambios

- `EnemyAI_update()` ya admite `TEAM_RED` y `TEAM_BLUE`; la misma lógica táctica puede dirigir cualquiera de los bandos.
- Cada instancia `EnemyAI` conserva su equipo y su estado táctico independiente, de modo que los focos, roles y progreso del turno no se comparten entre bandos.
- La ejecución sigue siendo secuencial, como el bucle principal del juego. El equipo activo se establece al entrar en `EnemyAI_update()`.
- `tests/ai_combat_stress_test.c` ahora simula turnos alternos de ambos equipos, con 60 unidades por bando, 12 turnos por equipo y un mapa determinista.
- La prueba comprueba la integridad espacial al finalizar cada movimiento, inmediatamente después de cada ataque y al final: unidad viva en su casilla, ocupación consistente y ninguna superposición.
- Registra decisiones, iteraciones, movimientos, ataques, reparaciones, bajas y turnos terminados por bando.

## Resultado observado

Salida reproducible en el entorno host:

```text
PASS: batalla simetrica 120 unidades; decisiones=1214 iteraciones=7202 movimientos(R/A)=107/205 ataques(R/A)=127/150 reparaciones(R/A)=28/77 bajas(R/A)=60/28 turnos(R/A)=12/12 integridad=590
```

Interpretación: los dos equipos ejecutaron movimientos, ataques y reparaciones; ambos completaron sus doce turnos; no se detectaron inconsistencias espaciales. En este escenario concreto, el equipo rojo perdió sus 60 unidades y el azul perdió 28. Es un resultado de una sola disposición determinista, no una medida estadística de equilibrio ni prueba de que un bando sea siempre superior.

La prueba marca todas las celdas visibles para ambos equipos deliberadamente para aislar decisiones de IA, movimiento y combate. La niebla de guerra real sigue cubierta por las pruebas independientes; esta prueba no mide aún el comportamiento con exploración progresiva ni escenarios variados.

## Validación

- `sh tests/run_tests.sh`: 23 grupos, perfilado y estrés adicional superados.
- `sh tests/run_tests_sanitized.sh`: suite completa y batalla de 120 unidades superadas bajo ASan/UBSan.
- GCC `-Wall -Wextra -Werror -Wformat=2 -Wno-main -fanalyzer -fsyntax-only` sobre todos los `src/*.c`: superado.
- No se compiló una ROM SGDK ni se ejecutó un emulador; este informe solo afirma resultados de las pruebas host.

## Siguientes mejoras sugeridas

1. Ejecutar una batería de escenarios deterministas con distintas disposiciones y terrenos.
2. Añadir estadísticas por clase y distancia, incluyendo unidades que no consiguieron actuar.
3. Probar la niebla real de ambos equipos y comparar resultados sin revelar posiciones ocultas.
4. Medir tiempos de CPU por fase con herramientas host; no inferir FPS de iteraciones de prueba.
