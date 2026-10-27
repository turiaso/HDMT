#!/usr/bin/env sh
# Build reproducible para una instalación local de SGDK en Linux/macOS.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ -z "${GDK:-}" ]; then
    echo "ERROR: define GDK con la ruta raíz de tu instalación SGDK." >&2
    echo 'Ejemplo: export GDK="/ruta/a/SGDK"' >&2
    exit 2
fi

if [ ! -f "$GDK/makefile.gen" ]; then
    echo "ERROR: no existe $GDK/makefile.gen; GDK no apunta a SGDK." >&2
    exit 2
fi

PATH="$GDK/bin:$PATH"
export PATH

if ! command -v make >/dev/null 2>&1; then
    echo "ERROR: falta la herramienta requerida: make" >&2
    exit 2
fi

cd "$ROOT"
printf 'Proyecto: %s\n' "$ROOT"
printf 'SGDK: %s\n' "$GDK"

if [ "$#" -eq 1 ] && [ "$1" = "clean" ]; then
    make -f "$GDK/makefile.gen" clean
    echo "PASS: salida de compilación limpiada."
    exit 0
fi

for tool in java m68k-elf-gcc; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: falta la herramienta requerida: $tool" >&2
        echo "Comprueba la instalación de SGDK y PATH. No se ha intentado compilar." >&2
        exit 2
    fi
done

printf 'Compilador: %s\n' "$(command -v m68k-elf-gcc)"
make -f "$GDK/makefile.gen" "$@"

ROM=
if [ -s out/rom.bin ]; then
    ROM=out/rom.bin
elif [ -s out/rom.gen ]; then
    ROM=out/rom.gen
fi

if [ -z "$ROM" ]; then
    echo "ERROR: la compilación terminó, pero no se encontró out/rom.bin ni out/rom.gen no vacío." >&2
    exit 1
fi

printf 'PASS: ROM generada: %s (%s bytes)\n' "$ROM" "$(wc -c < "$ROM" | tr -d ' ')"
