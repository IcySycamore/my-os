# entry.S — The first code that runs when QEMU boots our kernel.
#
# RISC-V boots all harts (cores) at the same address.
# We park all harts except hart 0.

.section .text.init
.globl _entry
_entry:
    # Which hart am I?
    csrr t0, mhartid
    bnez t0, park        # if hart != 0, park it

    # --- Hart 0: set up C runtime ---

    # Clear BSS (zero-initialized data)
    la t0, __bss_start
    la t1, __bss_end
1:
    bge t0, t1, 2f
    sd zero, (t0)
    addi t0, t0, 8
    j 1b
2:

    # Set up stack pointer (grows downward)
    la sp, __stack_top

    # Jump to C code
    j start

# Park non-boot harts here forever
park:
    wfi
    j park
