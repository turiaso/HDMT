#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

for function in activateCursorCell updateMovingUnitFrame updateEnemyTeamFrame updatePlayerTeamFrame updateGameFrame; do
    body=$(sed -n "/^static void ${function}(/,/^}/p" src/main.c)
    if [ -z "$body" ] || ! printf '%s\n' "$body" | grep -q 'GameTurn_canIssueCommands(&gameTurn)'; then
        printf 'FAIL: %s no bloquea órdenes después del resultado\n' "$function" >&2
        exit 1
    fi
done

printf '%s\n' 'PASS: entrada del jugador, IA, movimiento y loop de frames bloqueados tras finalizar la misión.'
