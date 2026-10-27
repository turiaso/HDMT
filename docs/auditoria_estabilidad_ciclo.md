# Auditoría de estabilidad del ciclo de partida

Fecha: 2026-10-27

## Alcance

Se revisó el flujo de repetir una misión, terminarla y volver al selector. La revisión se centra en el estado transitorio, la liberación de recursos gráficos y la posibilidad de reutilizar los búferes lógicos al iniciar otro escenario. No se modificó la cámara, la niebla, la precarga de sprites ni el sistema compartido de eventos.

## Resultado

- `tests/run_tests.sh`: superado, incluidos los 23 grupos principales y las pruebas adicionales.
- `tests/run_tests_sanitized.sh`: superado con ASan/UBSan disponibles en el entorno.
- `tools/audit_host.sh`: superado; incluye recursos PNG, suites host, sanitizadores, estilo y GCC `-fanalyzer`.
- `tools/check_source_quality.sh`: superado; 42 archivos C/H y análisis estático sin diagnósticos.
- `tests/session_lifecycle_test.sh`: reforzado para comprobar la reinicialización de banderas transitorias, el contexto de niebla, los punteros gráficos y el orden de liberación.

## Invariantes revisados

1. Al repetir, el escenario activo vuelve a pasar por `initGameData()` e `initVideo()`; no se conserva el estado táctico de la partida anterior.
2. Al volver al selector, solo se conserva el índice del escenario elegido.
3. Las referencias de `MovementGlow` y `UnitView` se invalidan antes de liberar el mapa y terminar el motor de sprites.
4. Se anulan los punteros de sprites/mapa y se invalidan las cachés de HUD, movimiento y scroll antes de la siguiente sesión.
5. Se reinician las banderas de movimiento, IA espectadora, retorno de cámara, refresco de niebla, feedback y contador FPS al inicializar los datos de la partida.
6. El contexto de equipo visible y modo espectador se reconstruye al crear el mapa gráfico.
7. Las órdenes y la actualización de la partida quedan bloqueadas después de registrar un resultado terminal.
8. Se cubren escenarios configurables, resultados de victoria/derrota/empate, 120 unidades y el ciclo del selector en pruebas host.

## Limitación

No se ha generado una ROM real ni ejecutado BlastEm: `tools/build_sgdk.sh` requiere configurar `GDK` con la instalación local de SGDK. Las pruebas host y los sanitizadores aumentan la confianza en la lógica C, pero no sustituyen la validación de hardware/emulador para vídeo, VRAM y temporización.
