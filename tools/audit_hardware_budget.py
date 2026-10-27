#!/usr/bin/env python3
"""Comprueba límites estáticos de VRAM y sprites del proyecto Mega Drive."""

from __future__ import annotations

from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


class AuditError(Exception):
    """La configuración estática excede un límite de hardware."""


def define_int(source: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+([0-9]+)u?\s*$", source, re.M)
    if not match:
        raise AuditError(f"no se encuentra una definición literal válida de {name}")
    return int(match.group(1))


def png_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise AuditError(f"PNG inválido: {path.relative_to(ROOT)}")
    return struct.unpack_from(">II", data, 16)


def main() -> int:
    try:
        config = (ROOT / "src/game_config.h").read_text(encoding="utf-8")
        unit_types = (ROOT / "src/unit_types.h").read_text(encoding="utf-8")
        scenario = (ROOT / "src/scenario.h").read_text(encoding="utf-8")
        controller = (ROOT / "src/unit_controller.h").read_text(encoding="utf-8")

        vram_tiles = define_int(config, "VRAM_TILE_COUNT")
        unit_start = define_int(config, "UNIT_VRAM_TILE_INDEX")
        unit_size_pixels = define_int(config, "UNIT_SPRITE_SIZE_PIXELS")
        hardware_tile_pixels = define_int(config, "HARDWARE_TILE")
        unit_type_count = define_int(unit_types, "UNIT_TYPE_COUNT")
        cursor_start = define_int(config, "CURSOR_VRAM_TILE_INDEX")
        cursor_axis = define_int(config, "CELL_TILES")
        cursor_tiles = cursor_axis * cursor_axis
        sprite_limit = define_int(config, "MD_VDP_SPRITE_LIMIT")
        fixed_sprites = define_int(config, "FIXED_GAME_SPRITE_COUNT")
        logical_capacity = define_int(controller, "GAME_MAX_UNITS")
        max_scenario_units = define_int(scenario, "SCENARIO_UNIT_COUNT")

        terrain_width, terrain_height = png_dimensions(ROOT / "res/gfx/terrain_tiles.png")
        terrain_tiles = (terrain_width // 8) * (terrain_height // 8)
        # TERRAIN_VRAM_TILE_INDEX depends on SGDK's system/font layout, so the
        # source-level runtime checks remain authoritative for terrain placement.
        if unit_size_pixels % hardware_tile_pixels:
            raise AuditError("el tamaño del sprite de unidad no está alineado a tiles")
        unit_tiles_per_axis = unit_size_pixels // hardware_tile_pixels
        unit_tile_count = unit_type_count * unit_tiles_per_axis * unit_tiles_per_axis
        unit_end = unit_start + unit_tile_count
        sprite_budget = sprite_limit - fixed_sprites

        if terrain_width % 8 or terrain_height % 8:
            raise AuditError("el atlas de terreno no está alineado a tiles de 8x8")
        if unit_end > cursor_start:
            raise AuditError(f"los gráficos de unidad terminan en {unit_end}, invadiendo el cursor ({cursor_start})")
        if cursor_start + cursor_tiles > vram_tiles:
            raise AuditError("el cursor supera el final de la VRAM")
        if fixed_sprites >= sprite_limit or sprite_budget <= 0:
            raise AuditError("la reserva de sprites fijos deja un presupuesto inválido")
        if max_scenario_units > logical_capacity:
            raise AuditError("el escenario máximo supera la capacidad lógica de unidades")

        print(f"PASS: atlas de terreno {terrain_width}x{terrain_height}: {terrain_tiles} tiles")
        print(f"PASS: unidades VRAM [{unit_start}, {unit_end}): {unit_tile_count} tiles para {unit_type_count} tipos")
        print(f"PASS: cursor VRAM [{cursor_start}, {cursor_start + cursor_tiles}) dentro de {vram_tiles} tiles")
        print(f"PASS: presupuesto SAT {sprite_budget} unidades + {fixed_sprites} sprites fijos = {sprite_limit}")
        print(f"PASS: capacidad lógica {logical_capacity}; despliegue máximo {max_scenario_units}")
        print("AVISO: el límite por línea de exploración (20 sprites/linea) requiere prueba visual/emulador; no se deduce del presupuesto SAT total.")
        print("AVISO: la posición exacta del atlas de terreno depende de TILE_SYSTEM_INDEX/TILE_SYSTEM_LENGTH/FONT_LENGTH; la valida initWorldMap().")
        return 0
    except (AuditError, OSError, struct.error) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
