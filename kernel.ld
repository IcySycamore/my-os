/*
 * kernel.ld — Linker script for our minimal RISC-V kernel.
 *
 * Memory layout on QEMU virt:
 *   RAM starts at 0x80000000
 *   We load our kernel there.
 */
OUTPUT_ARCH(riscv)
ENTRY(_entry)

SECTIONS
{
    . = 0x80000000;

    .text : {
        *(.text.init)    /* entry.S goes first */
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Mark end of kernel image for allocator */
    __end = .;

    /* Kernel stack (128KB — shared by all tasks) */
    . = ALIGN(16);
    __stack_top = . + 0x20000;
}
