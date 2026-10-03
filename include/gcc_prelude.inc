# Prepended to ee-gcc's assembly output so the modern gas encodes it the way Sony's 2000-era
# assembler did.

# Register moves were 64-bit.
.macro move dst, src
    daddu \dst, \src, $0
.endm

# `break N` (emitted for division-by-zero checks) put N in the low code field.
.macro break code
    .word (\code << 6) | 0xD
.endm
