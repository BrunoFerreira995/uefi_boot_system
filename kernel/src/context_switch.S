.global SchedulerContextSwitch

.intel_syntax noprefix

.section .text
SchedulerContextSwitch:
    mov [rdi + 0], r15
    mov [rdi + 8], r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp
    mov [rdi + 48], rsp

    mov eax, DWORD PTR [rip + CpuXsaveMask]
    mov edx, DWORD PTR [rip + CpuXsaveMask + 4]
    test eax, eax
    jz .save_fx
    xsave64 [rdi + 64]
    jmp .restore_registers
.save_fx:
    fxsave64 [rdi + 64]
.restore_registers:

    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]
    mov rsp, [rsi + 48]
    test eax, eax
    jz .restore_fx
    xrstor64 [rsi + 64]
    ret
.restore_fx:
    fxrstor64 [rsi + 64]
    ret

.extern CpuXsaveMask

.section .note.GNU-stack,"",@progbits
