#!/usr/bin/env python3
"""Audita la densidad vertical de los despliegues iniciales frente al VDP."""

from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ARRAYS = (
    ("Punto de control", "worldUnitStarts"),
    ("Defensa del puente", "bridgeUnitStarts"),
    ("Puente del desfiladero", "bridgeSkirmishUnitStarts"),
)


def define_int(source: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)u?\s*$", source, re.M)
    if not match:
        raise ValueError(f"no se encuentra una definición literal válida de {name}")
    return int(match.group(1))


def main() -> int:
    config = (ROOT / "src/game_config.h").read_text(encoding="utf-8")
    scenario = (ROOT / "src/scenario.c").read_text(encoding="utf-8")
    cell_pixels = define_int(config, "HARDWARE_TILE") * define_int(config, "CELL_TILES")
    sprite_width = define_int(config, "UNIT_SPRITE_SIZE_PIXELS")
    sprite_height = sprite_width
    screen_width = define_int(config, "SCREEN_PIXELS_W")
    scanline_limit = 20
    fixed_sprites = define_int(config, "FIXED_GAME_SPRITE_COUNT")
    failed = False

    # Conservador: cuenta cualquier sprite cuyo rectángulo pueda tocar la
    # pantalla, incluso si solo aparece parcialmente en uno de los bordes.
    max_units_per_scanline = (screen_width + sprite_width + cell_pixels - 1) // cell_pixels
    worst_case = max_units_per_scanline + fixed_sprites
    if worst_case <= scanline_limit:
        print(
            f"PASS: límite geométrico conservador: hasta {max_units_per_scanline} "
            f"unidades por línea + {fixed_sprites} sprites fijos = "
            f"{worst_case}/{scanline_limit}"
        )
    else:
        print(
            f"FAIL: límite geométrico conservador: {max_units_per_scanline} "
            f"unidades por línea + {fixed_sprites} sprites fijos = "
            f"{worst_case}/{scanline_limit}"
        )
        failed = True

    for label, array_name in ARRAYS:
        array = re.search(
            rf"const\s+UnitStart\s+{array_name}\[[^]]+\]\s*=\s*\{{(.*?)\n\}};",
            scenario, re.S,
        )
        if not array:
            raise ValueError(f"no se encuentra el despliegue {array_name}")
        entries = re.findall(
            r"\{\s*\d+\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\d+\s*,"
            r"\s*UNIT_TYPE_\w+\s*,\s*TEAM_\w+\s*\}", array.group(1)
        )
        if not entries:
            raise ValueError(f"el despliegue {array_name} no contiene unidades reconocibles")

        scanline_counts: dict[int, int] = {}
        for _x, cell_y in entries:
            sprite_y = int(cell_y) * cell_pixels + (cell_pixels - sprite_height) // 2
            for scanline in range(sprite_y, sprite_y + sprite_height):
                scanline_counts[scanline] = scanline_counts.get(scanline, 0) + 1

        peak_line, peak_units = max(scanline_counts.items(), key=lambda item: item[1])
        conservative_peak = peak_units + fixed_sprites
        status = "PASS" if conservative_peak <= scanline_limit else "FAIL"
        print(
            f"{status}: {label}: {len(entries)} unidades; máximo inicial {peak_units} "
            f"unidades/línea (línea mundo {peak_line}); +{fixed_sprites} sprites fijos "
            f"= {conservative_peak}/{scanline_limit}"
        )
        if conservative_peak > scanline_limit:
            failed = True

    print("AVISO: la cota geométrica presupone que las unidades siguen la cuadrícula; la validación visual final requiere ROM en emulador/consola.")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
