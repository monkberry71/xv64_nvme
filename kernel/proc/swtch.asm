format elf64
section ".text"
use64
public swtch
; void swtch(struct context **old, struct context *new)
swtch:
    ; only saves the callee-saved registers, because swtch is C function that follows SysV abi
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    ; stack -->
    ; r15 / r14 / r13 / r12 / rbx / rbp / rip(ret addr)

    mov qword [rdi], rsp ; *old = rsp

    ; Execution context is consist of rip and rsp
    ; rip will converge in this function, and changing rsp is eqaul to changing the contexts
    mov rsp, rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
