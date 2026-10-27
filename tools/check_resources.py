#!/usr/bin/env python3
"""Validate SGDK resource declarations and indexed PNG assets without third-party packages."""

from __future__ import annotations

from pathlib import Path
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
RESOURCE_FILE = ROOT / "res" / "resources.res"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DECLARATION_RE = re.compile(
    r'^\s*(PALETTE|TILESET|MAP|SPRITE)\s+'
    r'([A-Za-z_]\w*)\s+"([^"]+)"(.*)$'
)
EXPECTED_DIMENSIONS = {
    "gfx/terrain_tiles.png": (96, 24),
    "gfx/world_map.png": (968, 968),
    "gfx/bridge_map.png": (968, 968),
    "gfx/bridge_skirmish_map.png": (488, 488),
    "gfx/cell_cursor.png": (24, 24),
    "gfx/unit_soldier.png": (16, 16),
    "gfx/unit_scout.png": (16, 16),
    "gfx/unit_engineer.png": (16, 16),
    "gfx/unit_tank.png": (16, 16),
    "gfx/fog_tile.png": (8, 8),
}


class ValidationError(Exception):
    """A project asset or resource declaration is invalid."""


def read_png(path: Path) -> tuple[int, int, int, int]:
    """Validate PNG chunk integrity; return width, height, bit depth, color type."""
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise ValidationError(f"no se puede leer {path.relative_to(ROOT)}: {exc}") from exc

    if not data.startswith(PNG_SIGNATURE):
        raise ValidationError(f"{path.relative_to(ROOT)}: firma PNG inválida")

    offset = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = None
    has_palette = False
    has_iend = False

    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        end = offset + 12 + length
        if end > len(data):
            raise ValidationError(f"{path.relative_to(ROOT)}: chunk PNG truncado")
        chunk_data = data[offset + 8 : offset + 8 + length]
        stored_crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        actual_crc = zlib.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            name = chunk_type.decode("ascii", errors="replace")
            raise ValidationError(f"{path.relative_to(ROOT)}: CRC incorrecto en {name}")

        if chunk_type == b"IHDR":
            if length != 13 or width is not None:
                raise ValidationError(f"{path.relative_to(ROOT)}: IHDR inválido")
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk_data)
            )
            if compression != 0 or filtering != 0 or interlace not in (0, 1):
                raise ValidationError(f"{path.relative_to(ROOT)}: cabecera PNG no soportada")
        elif chunk_type == b"PLTE":
            has_palette = True
        elif chunk_type == b"IEND":
            if length != 0:
                raise ValidationError(f"{path.relative_to(ROOT)}: IEND inválido")
            has_iend = True
            offset = end
            break
        offset = end

    if width is None or height is None or not has_iend or offset != len(data):
        raise ValidationError(f"{path.relative_to(ROOT)}: estructura PNG incompleta o datos finales")
    if color_type != 3 or not has_palette:
        raise ValidationError(
            f"{path.relative_to(ROOT)}: SGDK requiere PNG indexado con PLTE "
            f"(color_type={color_type})"
        )
    return width, height, bit_depth, color_type


def read_resource_declarations() -> list[tuple[str, str, str, str]]:
    """Parse relevant resource lines; preserve the trailing arguments for checks."""
    try:
        lines = RESOURCE_FILE.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise ValidationError(f"no se puede leer res/resources.res: {exc}") from exc

    declarations = []
    seen_names: set[str] = set()
    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        match = DECLARATION_RE.match(line)
        if not match:
            raise ValidationError(
                f"res/resources.res:{line_number}: declaración no reconocida: {stripped}"
            )
        kind, name, asset, rest = match.groups()
        if name in seen_names:
            raise ValidationError(f"res/resources.res:{line_number}: recurso duplicado {name}")
        seen_names.add(name)
        declarations.append((kind, name, asset, rest.strip()))
    if not declarations:
        raise ValidationError("res/resources.res no contiene recursos")
    return declarations


def main() -> int:
    try:
        declarations = read_resource_declarations()
        resource_types = {name: kind for kind, name, _, _ in declarations}
        referenced_assets: set[str] = set()

        for kind, name, relative_asset, rest in declarations:
            asset = ROOT / "res" / relative_asset
            if not asset.is_file():
                raise ValidationError(f"{name}: falta el recurso {relative_asset}")
            referenced_assets.add(relative_asset)
            width, height, _, _ = read_png(asset)
            expected = EXPECTED_DIMENSIONS.get(relative_asset)
            if expected and (width, height) != expected:
                raise ValidationError(
                    f"{relative_asset}: tamaño {width}x{height}; se esperaba "
                    f"{expected[0]}x{expected[1]}"
                )
            if kind == "MAP":
                map_args = rest.split()
                if not map_args:
                    raise ValidationError(f"{name}: falta el tileset en la declaración MAP")
                tileset_name = map_args[0]
                if resource_types.get(tileset_name) != "TILESET":
                    raise ValidationError(
                        f"{name}: {tileset_name} no referencia un recurso TILESET declarado"
                    )
            print(f"PASS: {name:<24} {width:>4}x{height:<4} {relative_asset}")

        # Check all project PNGs, including legacy/mask assets not declared in resources.res.
        graphics = ROOT / "res" / "gfx"
        for asset in sorted(graphics.glob("*.png")):
            read_png(asset)

        missing_dimensions = sorted(set(EXPECTED_DIMENSIONS) - referenced_assets)
        if missing_dimensions:
            raise ValidationError(
                "estos assets esperados no están declarados en resources.res: "
                + ", ".join(missing_dimensions)
            )

        print(
            f"PASS: {len(declarations)} declaraciones SGDK, "
            f"{len(list(graphics.glob('*.png')))} PNG indexados íntegros y referencias válidas."
        )
        return 0
    except ValidationError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
