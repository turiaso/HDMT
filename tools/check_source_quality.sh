#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

python3 tools/check_source_quality.py

CC=${CC:-gcc}
for source in src/*.c; do
    "$CC" -std=c99 -Wall -Wextra -Werror -Wformat=2 -Wno-main \
        -fanalyzer -fsyntax-only -Itests/stubs -Isrc "$source"
done

printf '%s\n' 'PASS: GCC -fanalyzer; todos los módulos C compilan sin diagnósticos.'
