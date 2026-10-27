#!/usr/bin/env python3
"""Checks the repository's lightweight C style rules without external packages."""
from pathlib import Path
from typing import List
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "src"
MAX_COLUMNS = 100


def main() -> int:
    files = sorted((*SOURCE_DIR.glob("*.c"), *SOURCE_DIR.glob("*.h")))
    issues: List[str] = []

    for path in files:
        try:
            raw = path.read_bytes()
            text = raw.decode("utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            issues.append(f"{path.relative_to(ROOT)}: no se puede leer: {exc}")
            continue

        if raw and not raw.endswith(b"\n"):
            issues.append(f"{path.relative_to(ROOT)}: falta salto de línea final")

        for number, line in enumerate(text.splitlines(), start=1):
            location = f"{path.relative_to(ROOT)}:{number}"
            if line.rstrip(" \t") != line:
                issues.append(f"{location}: espacios al final")
            if "\t" in line:
                issues.append(f"{location}: tabulación; usar cuatro espacios")
            if len(line) > MAX_COLUMNS:
                issues.append(
                    f"{location}: {len(line)} columnas (máximo {MAX_COLUMNS})"
                )

    if issues:
        for issue in issues:
            print(f"ERROR: {issue}", file=sys.stderr)
        print(f"FAIL: {len(issues)} problemas de estilo en {len(files)} archivos C/H.",
              file=sys.stderr)
        return 1

    print(f"PASS: estilo C/H; {len(files)} archivos, máximo {MAX_COLUMNS} columnas.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
