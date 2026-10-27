#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."

# El prefijo y el valor deben ocupar columnas disjuntas dentro de la zona visible.
grep -Fq 'VDP_drawTextBGFill(BG_B, "FPS: ", 27, 0, 5);' src/main.c
grep -Fq 'VDP_drawTextBGFill(BG_B, fpsText, 32, 0, 3);' src/main.c

# SYS_getFPS debe consultarse cada fotograma; VDP_showFPS no puede sustituirlo
# en el camino que solo refresca el texto cada ocho fotogramas.
if grep -Fq 'VDP_showFPS(' src/main.c; then
    echo 'FAIL: main.c vuelve a usar VDP_showFPS en una frecuencia menor que un frame'
    exit 1
fi
count=$(grep -Fc 'const u32 fps = SYS_getFPS();' src/main.c)
[ "$count" -eq 1 ] || {
    echo "FAIL: se esperaban una sola llamada a SYS_getFPS por presentGameFrame, encontradas $count"
    exit 1
}

awk '
    /static void presentGameFrame\(/ { in_function=1 }
    in_function && /const u32 fps = SYS_getFPS\(\);/ { sampled=1 }
    in_function && /fpsOverlayFrameCounter\+\+;/ { if (!sampled) exit 1; counter=1 }
    in_function && /^}/ { if (sampled && counter) ok=1; in_function=0 }
    END { exit !ok }
' src/main.c || {
    echo 'FAIL: SYS_getFPS no se consulta en cada presentación antes del refresco visual'
    exit 1
}

echo 'PASS: FPS muestreados cada frame, texto en BG_B y columnas 27..34 sin solapamiento.'
