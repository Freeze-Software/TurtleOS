[EXTERN isr_handler]

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common_stub
%endmacro

isr_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call isr_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8   ; Double Fault has an error code
ISR_NOERRCODE 9
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack Fault
ISR_ERRCODE   13  ; General Protection Fault (#GP)
ISR_ERRCODE   14  ; Page Fault (#PF)
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17  ; Alignment Check
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30  ; Security Exception
ISR_NOERRCODE 31
