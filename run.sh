#!/usr/bin/env bash
# Compiles a .c file all the way through the toolchain and runs it,
# showing just the program's actual output -- not the compiler's IR/
# allocation debug printout, and not the emulator's interactive menu
# or full register dump.
#
# Usage: ./run.sh path/to/program.c
#
# Adjust AS_PREFIX below to match whatever RISC-V toolchain you installed
# -- e.g. "riscv64-unknown-elf" or "riscv-none-elf", whatever `which`
# found when you checked for an assembler.
set -e

AS_PREFIX="riscv64-linux-gnu"

if [ -z "$1" ]; then
    echo "Usage: $0 path/to/program.c"
    exit 1
fi

SRC="$1"
BASE="${SRC%.c}"

./build/compiler/compiler "$SRC" > /dev/null
"${AS_PREFIX}-as" -march=rv32im -mabi=ilp32 -o "${BASE}.o" "${SRC}.s"
"${AS_PREFIX}-objcopy" -O binary "${BASE}.o" "${BASE}.bin"

echo "3
${BASE}.bin" | ./build/emulator/emulator \
    | sed 's/^Enter the name of the \.bin file (test\.bin): //' \
    | grep -v -E "^(Select mode:|[0-9] - |x[0-9]+ = )" || true