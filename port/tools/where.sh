#!/bin/sh
# Runs port/build/bt3 under gdb for N seconds (default 3), interrupts it and prints where it is.
# Also prints the backtrace if it crashes first. Usage: port/tools/where.sh [seconds]
cd "$(dirname "$0")/../.."
( sleep "${1:-3}"; pkill -INT -x bt3 ) &
gdb -q -batch -ex "set debuginfod enabled off" -ex "set confirm off" -ex run -ex bt -ex kill port/build/bt3 2>&1 |
    grep -E "^#|signal|not implemented|exited|bt3:" | cut -c1-200 | head -${2:-14}
wait
