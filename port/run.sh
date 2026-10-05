#!/bin/sh
# Runs the PC build in a window with the GPU renderer, playing a replay (default: the validation replay), with
# "demo" the game's own attract battle, or with "menu" the game from its menus. Escape or closing the window quits.
#   port/run.sh [replay-file | demo | menu]
cd "$(dirname "$0")/.."
case "${1:-gamedata/validation/replay01.bin}" in
    demo) unset BT3_REPLAY; export BT3_DEMO=1 ;;
    menu) unset BT3_REPLAY BT3_DEMO ;;
    *) export BT3_REPLAY="${1:-gamedata/validation/replay01.bin}" ;;
esac
BT3_GS=gpu exec port/build/bt3
