.global CpuLoadGdt
.global CpuLoadIdt
.global CpuLoadTss
.global CpuExceptionStub0
.global CpuExceptionStub1
.global CpuExceptionStub2
.global CpuExceptionStub3
.global CpuExceptionStub4
.global CpuExceptionStub5
.global CpuExceptionStub6
.global CpuExceptionStub7
.global CpuExceptionStub8
.global CpuExceptionStub9
.global CpuExceptionStub10
.global CpuExceptionStub11
.global CpuExceptionStub12
.global CpuExceptionStub13
.global CpuExceptionStub14
.global CpuExceptionStub15
.global CpuExceptionStub16
.global CpuExceptionStub17
.global CpuExceptionStub18
.global CpuExceptionStub19
.global CpuExceptionStub20
.global CpuExceptionStub21
.global CpuExceptionStub22
.global CpuExceptionStub23
.global CpuExceptionStub24
.global CpuExceptionStub25
.global CpuExceptionStub26
.global CpuExceptionStub27
.global CpuExceptionStub28
.global CpuExceptionStub29
.global CpuExceptionStub30
.global CpuExceptionStub31
.global CpuIrqStub32
.global CpuIrqStub33
.global CpuIrqStub34
.global CpuIrqStub35
.global CpuIrqStub36
.global CpuIrqStub37
.global CpuIrqStub38
.global CpuIrqStub39
.global CpuIrqStub40
.global CpuIrqStub41
.global CpuIrqStub42
.global CpuIrqStub43
.global CpuIrqStub44
.global CpuIrqStub45
.global CpuIrqStub46
.global CpuIrqStub47
.extern CpuExceptionHandler
.extern CpuIrqHandler

.intel_syntax noprefix

.section .text
CpuLoadGdt:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    push 0x08
    lea rax, [rip + 1f]
    push rax
    retfq
1:
    ret

CpuLoadIdt:
    lidt [rdi]
    ret

CpuLoadTss:
    mov ax, di
    ltr ax
    ret

.macro EXCEPTION_STUB_NO_ERROR vector
CpuExceptionStub\vector:
    push 0
    push \vector
    jmp CpuExceptionCommon
.endm

.macro EXCEPTION_STUB_ERROR vector
CpuExceptionStub\vector:
    push \vector
    jmp CpuExceptionCommon
.endm

EXCEPTION_STUB_NO_ERROR 0
EXCEPTION_STUB_NO_ERROR 1
EXCEPTION_STUB_NO_ERROR 2
EXCEPTION_STUB_NO_ERROR 3
EXCEPTION_STUB_NO_ERROR 4
EXCEPTION_STUB_NO_ERROR 5
EXCEPTION_STUB_NO_ERROR 6
EXCEPTION_STUB_NO_ERROR 7
EXCEPTION_STUB_ERROR 8
EXCEPTION_STUB_NO_ERROR 9
EXCEPTION_STUB_ERROR 10
EXCEPTION_STUB_ERROR 11
EXCEPTION_STUB_ERROR 12
EXCEPTION_STUB_ERROR 13
EXCEPTION_STUB_ERROR 14
EXCEPTION_STUB_NO_ERROR 15
EXCEPTION_STUB_NO_ERROR 16
EXCEPTION_STUB_ERROR 17
EXCEPTION_STUB_NO_ERROR 18
EXCEPTION_STUB_NO_ERROR 19
EXCEPTION_STUB_NO_ERROR 20
EXCEPTION_STUB_ERROR 21
EXCEPTION_STUB_NO_ERROR 22
EXCEPTION_STUB_NO_ERROR 23
EXCEPTION_STUB_NO_ERROR 24
EXCEPTION_STUB_NO_ERROR 25
EXCEPTION_STUB_NO_ERROR 26
EXCEPTION_STUB_NO_ERROR 27
EXCEPTION_STUB_NO_ERROR 28
EXCEPTION_STUB_ERROR 29
EXCEPTION_STUB_ERROR 30
EXCEPTION_STUB_NO_ERROR 31

CpuExceptionCommon:
    mov rdi, rsp
    call CpuExceptionHandler
    add rsp, 16
    iretq

.macro IRQ_STUB vector
CpuIrqStub\vector:
    push 0
    push \vector
    jmp CpuIrqCommon
.endm

IRQ_STUB 32
IRQ_STUB 33
IRQ_STUB 34
IRQ_STUB 35
IRQ_STUB 36
IRQ_STUB 37
IRQ_STUB 38
IRQ_STUB 39
IRQ_STUB 40
IRQ_STUB 41
IRQ_STUB 42
IRQ_STUB 43
IRQ_STUB 44
IRQ_STUB 45
IRQ_STUB 46
IRQ_STUB 47

CpuIrqCommon:
    mov rdi, rsp
    call CpuIrqHandler
    add rsp, 16
    iretq

.section .note.GNU-stack,"",@progbits
