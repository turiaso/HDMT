# Escenario 4: Cruce de las ruinas

## Diseño

- Mapa lógico y gráfico de 20×20 celdas.
- 6 unidades por equipo: 2 infanterías, 1 explorador, 1 ingeniero y 2 blindados.
- Reutiliza exactamente el despliegue del tercer escenario para comparar partidas en otro trazado.
- Objetivo central en (10, 9): mantener el cruce durante 3 finales de turno propios.
- El río discurre de este a oeste; el cruce central de camino atraviesa el mapa de norte a sur.
- La imagen gráfica rota las posiciones de los tiles, no los píxeles dentro de cada tile, para conservar coincidencias con el tileset de SGDK.

## Menú

El selector muestra cuatro opciones. Cada bloque usa cuatro filas, empieza en la fila 5 y termina en la 19; las instrucciones permanecen en las filas 22 y 24. Así el resumen de la cuarta misión no pisa los controles.

## Pruebas

`tests/scenario_test.c` valida dimensiones, despliegues, casillas transitables, ruta de todas las unidades al objetivo y la condición de mantener el punto para ambos equipos. `tests/ai_scenario_smoke_test.c` simula el turno rojo y verifica que no haya ocupaciones duplicadas ni referencias huérfanas. `tests/scenario_menu_test.c` comprueba la cuarta opción, navegación circular y separación entre texto e instrucciones.
