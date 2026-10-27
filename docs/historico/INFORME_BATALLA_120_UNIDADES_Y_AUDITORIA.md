# Entrega: combate integrado, IA y auditoría a 120 unidades

## Alcance

Esta iteración completa la validación host que faltaba en el turno masivo: la prueba anterior de 120 unidades recorría el despliegue del escenario y acababa sin atacar, por lo que no ejercitaba un enfrentamiento. Se añade `tests/ai_combat_stress_test.c`, un escenario de estrés determinista en mapa transitable de 40x40, con 60 unidades por equipo y contacto inicial entre frentes.

## Qué verifica el nuevo escenario

- La IA roja procesa un turno completo y no queda atrapada en un bucle de decisiones.
- Hay movimientos, ataques y reparaciones reales en el escenario integrado.
- La niebla está habilitada en el controlador; la visibilidad roja se fija deliberadamente en toda la cuadrícula para aislar las decisiones de combate de la variabilidad del cálculo de visión. La niebla incremental y la ocultación se cubren por separado en `fog_of_war_test.c` y `enemy_ai_test.c`.
- Después del turno, cada unidad viva ocupa una casilla cuyo `occupantId` coincide con su ID, y ninguna pareja de unidades vivas comparte coordenadas.
- El caso se incorpora tanto a la suite normal como a la suite ASan/UBSan.

## Resultado medido en host

Con GCC y los flags de pruebas del proyecto:

- Decisiones de IA: 73
- Movimientos iniciados: 12
- Ataques resueltos: 18
- Reparaciones resueltas: 2
- Iteraciones del bucle de estrés: 390

Estos números son una línea base determinista del escenario sintético, no una medición de FPS de Mega Drive ni un benchmark de la ROM. No se debe comparar el contador `frames` de esta prueba con fotogramas de VDP: en el harness host significa iteraciones del bucle.

## Los cuatro frentes

1. **Combate coordinado:** se ejercita la puntuación de objetivos, los ataques, el foco de fuego y las decisiones de clase en una batalla real de prueba. El código mantiene la regla de no premiar doblemente el foco cuando el golpe ya elimina al objetivo y descarta intercambios en los que la represalia letal mata al atacante si el defensor sobrevive.
2. **Posicionamiento por rol/terreno:** se conservan las pruebas específicas existentes de cobertura, bosque, camino, apoyo con línea de tiro, escolta y flanqueo. El estrés integrado verifica que el movimiento real no deja solapes.
3. **Información visible:** se conservan las pruebas de niebla persistente y de rechazo de objetivos ocultos, incluyendo la invalidación de un foco heredado. El escenario de estrés fuerza visión para centrarse en el combate, por diseño.
4. **Rendimiento e integridad:** el proyecto mantiene contadores de perfilado por fase para scroll, SAT, niebla, rutas, nodos, alcance, candidatos de IA, movimiento y ataques. Las pruebas host protegen las actualizaciones localizadas y el hover sin pathfinding. Falta medir con el SGDK real/emulador para obtener tiempos y consumo de hardware representativos.

## Validación

Ejecutar desde la raíz del proyecto:

```sh
tests/run_tests.sh
tests/run_tests_sanitized.sh
```

También se debe ejecutar el build real con SGDK y una sesión en BlastEm o hardware antes de declarar cerrada la validación de ROM. Este entorno no aporta esa comprobación.
