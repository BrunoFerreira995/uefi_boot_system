.global _start
.intel_syntax noprefix

.section .text
_start:
    # Disable interrupts
    cli

    # Setup private kernel stack pointer
    # Align stack to 16-byte boundary as per System V ABI requirement
    lea rsp, [stack_top]

    # Call C++ kernel entry point
    # RDI contains the BootInfo* pointer passed by the bootloader
    call kernel_main

# Fallback loop
halt:
    cli
    hlt
    jmp halt

.section .bss
.align 16
stack_bottom:
    .skip 16384 # 16KB stack space
stack_top:
