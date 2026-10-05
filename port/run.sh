#!/bin/sh
# Runs the PC build in a window with the GPU renderer, playing a replay (default: the validation replay) or,
# with "demo", the game's own attract battle. Escape or closing the window quits.
#   port/run.sh [replay-file | demo]
cd "$(dirname "$0")/.."
case "${1:-gamedata/validation/replay01.bin}" in
    demo) unset BT3_REPLAY ;;
    *) export BT3_REPLAY="${1:-gamedata/validation/replay01.bin}" ;;
esac
BT3_GS=gpu exec port/build/bt3
