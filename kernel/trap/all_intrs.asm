format elf64

macro pushaq {
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
}

macro popaq {
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
}

extrn intr
section ".text"
use64
public all_intrs
public intr_ret
all_intrs:
    ; stack -->
    ; [0]num / [1]err / [2]rip / [3]cs / [4]rflags / [5]ori_rsp / [6]ss
    test byte [rsp + 24], 3; Is the CS user?
    jz .skip_swapgs
    swapgs
.skip_swapgs:
    pushaq

    mov rdi, rsp

    ; If it was a kthread that received int,
    ; stack alignment is required
    mov rbx, rsp
    and rsp, -16 
    call intr
    mov rsp, rbx
intr_ret:
    popaq
    ; [0]num / [1]err / [2]rip / [3]cs / [4]rflags / [5]ori_rsp / [6]ss
    add rsp, 16; err and vector num 
    ; [0]rip / [1]cs / [2]rflags / [3]ori_rsp / [4]ss
    test byte [rsp + 8], 3
    jz .skip_swapgs_again
    swapgs
.skip_swapgs_again:
    iretq