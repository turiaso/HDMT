# Informe — Puente del desfiladero (20×20)

## Entrega

Se añade un tercer escenario seleccionable sin ramas específicas en `main.c`:

- **Nombre:** Puente del desfiladero.
- **Dimensiones:** 20×20 celdas lógicas.
- **Despliegue:** 6 unidades azules y 6 rojas.
- **Composición por bando:** 2 infanterías, 1 explorador, 1 ingeniero y 2 blindados.
- **Objetivo:** mantener el puente central en `(10,9)` durante 3 finales de turno.

## Mapa

El río norte-sur y el paso de puente se basan en la misma familia de terreno de `Defensa del puente`. Se usa una sección central de 20×20 del mapa de 40×40, con su correspondiente recorte lógico y un recurso gráfico independiente `res/gfx/bridge_skirmish_map.png`. La disposición incluye bosque, agua, camino y rocas. El registro de escenarios sigue usando buffers compartidos de RAM: solo un mapa se inicializa y juega a la vez.

## Verificaciones

`tests/scenario_test.c` valida ahora los tres escenarios, dimensiones, composición, IDs consecutivos, despliegues sin solapamiento, transitabilidad, rutas al objetivo y recurso gráfico. El test del escenario compacto confirma que el puente es transitable y que el agua fuera del cruce bloquea el movimiento.

La validación host se registra tras ejecutar `sh tests/run_tests.sh` y `sh tests/run_tests_sanitized.sh`. No se afirma una compilación ROM SGDK ni una ejecución en emulador en este informe.

## Correcciones posteriores: menú y acción de movimiento

- El selector de misiones ahora separa cada bloque en cinco filas (título,
  descripción y resumen), de modo que el resumen de la tercera misión termina
  en la fila 18 y no se pisa con los controles inferiores de la fila 21.
- Completar una orden de movimiento consume el movimiento restante de esa unidad
  durante el turno. La unidad puede seleccionarse de nuevo para consultar sus
  datos, pero no puede iniciar una segunda orden hasta que comience su siguiente
  turno. El presupuesto de movimiento y los costes por terreno siguen limitando
  la ruta que puede ejecutar en esa única orden.
- Regresión comprobada: tras completar una ruta, el alcance queda vacío y una
  nueva orden devuelve `UNIT_ACTION_NO_MOVEMENT_POINTS`; el inicio del siguiente
  turno restaura los puntos.
- Validación repetida: suite completa, ASan/UBSan y GCC `-fanalyzer`, todo OK.
