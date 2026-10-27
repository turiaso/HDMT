# Documentación del proyecto

Empieza por los documentos que describen el estado actual:

1. **[Guía de desarrollo](guia_desarrollo.md):** dónde cambiar cada comportamiento, contratos entre módulos, estilo C y comandos de comprobación.
2. **[Arquitectura](arquitectura.md):** flujo de una orden, propiedad de los datos, responsabilidades por módulo, límites de hardware y reglas que no deben romperse.
3. **[Escenarios](escenarios.md):** mapas, despliegues, objetivos y cómo registrar una misión nueva.
4. **[Unidades](unidades.md):** perfiles de clase, despliegue, controles, movimiento, combate, reparación de ingenieros y niebla.
5. **[Movimiento por clase y terreno](movimiento_por_clase_y_terreno.md):** tabla de costes, contratos entre pathfinding, movimiento, niebla y HUD, y pruebas de regresión.
6. **[IA táctica por terreno y clase](ia_tactica_terreno.md):** preferencias posicionales, alcance propio, línea de tiro, cobertura, apoyo del ingeniero y selección de corredor según movilidad.
7. **[Posicionamiento y coordinación de la IA](posicionamiento_ia_y_congestion.md):** separación de posiciones de tiro, bloqueo de corredores, cobertura y tráfico en puentes estrechos.
8. **[Refactorización y auditoría](refactorizacion_y_auditoria.md):** cambios estructurales de esta edición y resultados de validación.
9. **[Informe de refactorización de mantenibilidad](INFORME_REFACTORIZACION_MANTENIBILIDAD.md):** cambios recientes, optimizaciones conservadoras y pruebas.

Los documentos `fase_3.md` a `fase_7.md` son registros del desarrollo por etapas. Para el estado vigente, prevalecen la guía, la arquitectura, los documentos de escenarios/unidades y el propio código de `src/`.

La carpeta [`historico/`](historico/README.md) conserva informes de entregas previas. Son bitácoras, no especificaciones actuales; algunas describen decisiones o límites que posteriormente cambiaron.

- [HUD y feedback de acciones](hud_jugabilidad.md): ficha de unidad seleccionada, mensajes de ruta y puntos de movimiento, objetivo/progreso en una fila independiente, predicción de combate y reparación, y feedback de resultados.
- [Separación de objetivo y turno en el HUD](hud_objetivo_y_turno_separados.md): una fila para el turno/MP y otra para la misión, con progreso explícito y limpieza del estado final.
- [Mejoras de selección, movimiento y reparación](mejoras_feedback_seleccion_movimiento_reparacion.md): cambios concretos en la ficha `SEL`, mensajes de rutas y estimación/confirmación de PV reparados.
- [Inspección de ocupación en el HUD](hud_inspeccion_ocupacion.md): clase, equipo y PV de unidades visibles, con pruebas para evitar filtrar información a través de la niebla.
- [Estado de integración SGDK/BlastEm](estado_integracion_sgdk_blastem.md): resultado comprobado de las suites host y bloqueo explícito de la validación real de ROM en este entorno.

## Auditorías recientes

- [Auditoría y refactorización de la IA](INFORME_AUDITORIA_IA.md): contexto de turnos, eliminación del equipo global implícito y pruebas de regresión.
- [Ajuste de selección del corredor de IA](ajuste_corredor_ia.md): priorización por movilidad efectiva y salud, con regresión dedicada.
- [Refuerzo del posicionamiento de la IA](refuerzo_posicionamiento_ia.md): coordinación del fuego, separación de unidades, corredores y colas en puentes, con estado de validación SGDK/BlastEm.
- [Auditoría de configuraciones de misión](auditoria_escenarios.md): conectividad de despliegues, coherencia de mapas/objetivos y resultados de las tres misiones para ambos equipos.
- [Corrección de objetivos de misión](correccion_objetivos_mision.md): empate y equipos vacíos, orden de `TURN_END`, progreso de puntos de control y pruebas de regresión.
- [Validación del progreso de objetivos por turnos](validacion_turnos_objetivos.md): llamadas repetidas a `GameTurn_checkObjective()`, progreso por turno propio y cierre sin duplicados.
- [Casos límite de resultados de misión](casos_limite_resultados_mision.md): bajas simultáneas, objetivos disputados, equipos sin unidades y evaluación inicial sin cierre prematuro.

- [Pruebas de regresión en BlastEm](pruebas_regresion_emulador.md): lista manual obligatoria para validar cámara, niebla, turnos, IA, objetivos y FPS con una ROM real.
- [Feedback temporal de acciones del HUD](hud_feedback_temporal.md): diferencia resultados persistentes de las previsualizaciones y libera el mensaje al cambiar de casilla.
- [Pantalla de resultado y flujo de partida](pantalla_resultado_y_flujo_partida.md): motivo del resultado, turnos transcurridos, supervivientes, progreso, controles de repetir/menú y bloqueo de órdenes tras finalizar.
- [Ciclo de partida y reinicio](ciclo_partida_y_reinicio.md): persistencia de selección del escenario, orden de liberación de mapa/sprites y reinicialización de cachés.

- [Fase 8: escenarios, combate, IA y resolución de misión](fase_8_escenarios_combate_ia.md)

- [Cuarto escenario: Cruce de las ruinas](escenario_4_cruce_ruinas.md): mapa 20×20 girado, mismo despliegue y objetivo que Puente del desfiladero.
