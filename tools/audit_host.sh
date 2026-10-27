#!/usr/bin/env sh
# Ejecuta todas las validaciones reproducibles sin depender de SGDK instalado.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

printf '%s\n' '== Recursos SGDK y PNG =='
python3 tools/check_resources.py
printf '%s\n' '== Presupuesto estático de hardware Mega Drive =='
python3 tools/audit_hardware_budget.py
python3 tools/audit_sprite_scanlines.py
printf '%s\n' '== Suite host =='
sh tests/run_tests.sh
printf '%s\n' '== Suite ASan/UBSan =='
sh tests/run_tests_sanitized.sh
printf '%s\n' '== Estilo y GCC -fanalyzer =='
sh tools/check_source_quality.sh
printf '%s\n' 'PASS: recursos, presupuesto de hardware, densidad de sprites por línea, suites host, sanitizadores, estilo y analizador estático.'
printf '%s\n' 'NOTA: esta auditoría no compila una ROM SGDK ni sustituye la prueba en BlastEm.'
