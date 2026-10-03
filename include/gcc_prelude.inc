# Prepended to ee-gcc's assembly output so the modern gas produces what Sony's 2000-era
# assembler did. It applies to every C file (and to any asm a C file pulls in with INCLUDE_ASM).
#
# No assembler flag or `.set` mode does the same job: `.set mips4` and up drop the hazard nops
# but also the R5900 instruction set and encodings, `-O1` stops all delay-slot filling (the
# original has most of it), and this binutils fork adds no option for either.
#
# Helper symbols are named .L* so that they stay out of the object's symbol table.

# ---------------------------------------------------------------------------------------------
# Instruction wrappers
#
# The two fixes below have to act right behind particular instructions, or need to know which
# instructions came before a branch. Macros cannot ask gas for either, so the mnemonics the
# compiler emits are wrapped (lists further down): a wrapper removes itself to reach the
# built-in instruction, does its bookkeeping, and comes back.
#
# 1. Hazard nops (__gp_forget). For the R5900 (MIPS III) the modern gas keeps one instruction
#    between an FPU compare and the bc1t/bc1f that tests it, and between mtc1/mfc1/ctc1/cfc1
#    and the next use of the register, inserting a nop where the compiler left none. It does
#    the same at an alignment or data directive after the mtc1 that `li.s` expands to. Sony's
#    assembler knew the R5900 needs none of that. gas adds the nop while assembling whatever
#    comes next, from what it remembers of the previous instruction; an empty data directive
#    makes it settle that early, and under `.set mips4` it settles on "no nop".
#    Side effect: gas no longer knows the instruction, so it cannot move it into the delay slot
#    of an unfilled branch that follows directly (`li.s $f0,<constant> / j $31`, for one).
#
# 2. Unfilled branches (.L__gp_state). In reorder mode the assembler fills an empty delay slot
#    by moving the instruction in front of the branch into it. Sony's assembler did not do that
#    when the instruction before THAT one was assembled under `.set noreorder` (its
#    "prev_prev_insn_unreordered" rule), which in compiler output means a delay slot the
#    compiler filled itself. The modern gas has no such rule. The case that shows up is a
#    return the compiler duplicated, taking a copy of the first instruction of the shared tail
#    for the delay slot:
#            j $31 / move $2,$3      <- filled by the compiler (.set noreorder)
#        $L26:
#            move $2,$3              <- stays in front of the return ...
#            j $31                   <- ... which gets a nop; modern gas: `jr $ra / move`
#    A label on the branch stops the swap, but it may only go where the rule applies:
#    everywhere else the swap is original too. Macros cannot see gas's reorder mode, and using
#    `.` to find out where things are stops the swap all by itself, so the wrappers recognise
#    the shape above by its text: a branch, then the same instruction twice, then a branch.
#      .L__gp_state   2 = behind a branch, 3 = one ordinary instruction later (its text is
#                     in __gp_same_as_last), 4 = the same instruction again, 0 = anything else
#    Not covered: an instruction kept in front of an unfilled branch for the same reason where
#    the delay slot before it holds a different instruction; that still comes out swapped.
#    Instructions whose mnemonic is in neither list below are not seen at all.
# ---------------------------------------------------------------------------------------------
.set .L__gp_state, 0

.macro __gp_forget
    .set push
    .set mips4
    .fill 0
    .set pop
.endm

.macro __gp_same_as_last text:vararg
.endm

# In front of an ordinary instruction; \text is the instruction as written.
.macro __gp_note text:vararg
    .if .L__gp_state == 2
        .set .L__gp_state, 3
        .purgem __gp_same_as_last
        .macro __gp_same_as_last other:vararg
            .ifeqs "\text", "\other"
                .set .L__gp_state, 4
            .endif
        .endm
    .else
        .if .L__gp_state == 3
            .set .L__gp_state, 0
            __gp_same_as_last \text
        .else
            .set .L__gp_state, 0
        .endif
    .endif
.endm

.macro __gp_insn mnem, forget=0
    .macro \mnem operands:vararg
        .purgem \mnem
        __gp_note \mnem \operands
        \mnem \operands
        .if \forget
            __gp_forget
        .endif
        __gp_insn \mnem, \forget
    .endm
.endm

# The label is harmless if the branch turns out to be in a noreorder block.
.macro __gp_branch mnem
    .macro \mnem operands:vararg
        .purgem \mnem
        .if .L__gp_state == 4
99770:
        .endif
        \mnem \operands
        .set .L__gp_state, 2
        __gp_branch \mnem
    .endm
.endm

.irp mnem, j, jal, jr, jalr, b, bal, beq, bne, beqz, bnez, blez, bgtz, bltz, bgez, bgt, bge, blt, ble, bgtu, bgeu, bltu, bleu, beql, bnel, beqzl, bnezl, blezl, bgtzl, bltzl, bgezl, bgtl, bgel, bltl, blel, bgtul, bgeul, bltul, bleul, bltzal, bgezal, bc1t, bc1f, bc1tl, bc1fl
    __gp_branch \mnem
.endr

# Not listed: move, break, cvt.w.s and sqrt.s (defined below on top of these) and what only
# hand-written asm uses (COP0, COP2/VU, most of MMI, cache, syscall).
.irp mnem, nop, lui, li, dli, la, dla, add, addu, addi, addiu, sub, subu, neg, negu, abs, dadd, daddu, daddi, daddiu, dsub, dsubu, dneg, dnegu, dabs, and, andi, or, ori, xor, xori, nor, not, sll, srl, sra, sllv, srlv, srav, dsll, dsrl, dsra, dsll32, dsrl32, dsra32, dsllv, dsrlv, dsrav, slt, slti, sltu, sltiu, seq, sne, sgt, sgtu, sge, sgeu, sle, sleu, movz, movn, mult, multu, mult1, multu1, mul, div, divu, div1, divu1, rem, remu, madd, maddu, madd1, maddu1, mfhi, mflo, mthi, mtlo, mfhi1, mflo1, mthi1, mtlo1, mfsa, mtsa, lb, lbu, lh, lhu, lw, lwu, ld, lwl, lwr, ldl, ldr, lq, sb, sh, sw, sd, swl, swr, sdl, sdr, sq, ulh, ulhu, ulw, uld, ush, usw, usd, por, pand, pxor, pnor, pcpyld, pcpyud, lwc1, swc1, l.s, s.s, add.s, sub.s, mul.s, div.s, abs.s, neg.s, mov.s, cvt.s.w, trunc.w.s, max.s, min.s, madd.s, msub.s, adda.s, suba.s, mula.s, madda.s, msuba.s, rsqrt.s
    __gp_insn \mnem
.endr

# The instructions gas would put a hazard nop behind. They stay real instructions, in any
# operand syntax; the compares keep the R5900 function codes gas already uses for this CPU.
#
# `li.s`: gas assembles it itself, as lui $at / mtc1 $at when the low 16 bits of the constant
# are zero and as a load from .lit4 otherwise (under -G0 it would be .rodata; not seen yet).
# Both match the original, so every constant is handled and there is no error case; the mtc1
# form is why it is in this list.
.irp mnem, c.f.s, c.eq.s, c.lt.s, c.le.s, mtc1, mfc1, ctc1, cfc1, li.s
    __gp_insn \mnem, 1
.endr

# ---------------------------------------------------------------------------------------------
# Encodings
# ---------------------------------------------------------------------------------------------

# Register moves were 64-bit.
.macro move dst, src
    daddu \dst, \src, $0
.endm

# `break N` (emitted for division-by-zero checks) put N in the low code field, which is where
# the two-operand form `break 0, N` has it.
.macro break code, code2
    __gp_note break \code \code2
    .ifb \code2
        .word (\code << 6) | 0xD
    .else
        .word (\code << 16) | (\code2 << 6) | 0xD
    .endif
.endm

# The R5900's only float-to-int conversion truncates, and Sony's assembler took the name
# `cvt.w.s` for it. The modern gas only knows it as `trunc.w.s` (same encoding).
.macro cvt.w.s dst, src
    trunc.w.s \dst, \src
.endm

# \sym = the number of register operand \reg, written `$N` or `$fN` as the compiler does;
# 99 for anything else. For instructions that have to be emitted as raw words.
.macro __gp_regnum sym, reg
    .set \sym, 99
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
        .ifc \reg,$\n
            .set \sym, \n
        .endif
        .ifc \reg,$f\n
            .set \sym, \n
        .endif
    .endr
.endm

# `sqrt.s fd, ft`: the R5900 takes the operand from the ft field (bits 16-20); the modern gas
# puts it in fs (bits 11-15) even for this CPU.
.macro sqrt.s fd, ft
    __gp_regnum .L__gp_fd, \fd
    __gp_regnum .L__gp_ft, \ft
    .if (.L__gp_fd > 31) || (.L__gp_ft > 31)
        .error "gcc_prelude.inc: sqrt.s \fd, \ft: operands must be written $fN"
    .endif
    __gp_note sqrt.s \fd \ft
    .word 0x46000004 | (.L__gp_ft << 16) | (.L__gp_fd << 6)
.endm
