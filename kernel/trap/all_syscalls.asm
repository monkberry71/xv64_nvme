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

section ".text"
public all_syscalls
public syscall_ret
extrn syscall_dispatch
all_syscalls:
    swapgs
    mov [gs:8], rsp; save the original rsp to mycpu
    mov rsp, [gs:16]; get kstack from mycpu 

    ; We now need to push the original registers like intr
    ; intr pushed the registers like: 
    ; stack -->
    ; {[0] num, [1] err, [2] rip, [3] cs, [4] rflags, [5] ori_rsp, [6] ss}
    ; However, syscall itself saves 
    ; 1. The [5]original rip to rcx
    ; 2. The [4]original rflags to r11
    ; Also we don't need to save [3]cs and [6]ss, because it is written in the STAR that which segment we need when ret.
    ; There is no [1]err in syscall and the user will set rax as [0]num
    ; So we only need to save [5] ori_rsp
    ; But we are going to fill it like intr, just in case when debugging is needed

    push qword 0x16 ; skip ss
    push qword [gs:8] ; save the original rsp
    push qword r11 ; skip rflags
    push qword 0x8 ; skip cs
    push qword rcx ; skip rip
    push qword 64 ; xv6 used 64 as the intr num of syscall, so just saving as 64
    push qword rax ; skip num

    pushaq ; save gprs

    ; arg setting
    mov rdi, rsp;

    ; stack alignment setting 
    mov rbx, rsp
    and rsp, -16

    call syscall_dispatch
    
    mov rsp, rbx
syscall_ret:
    popaq
    
    add rsp, 56; pop all
    mov qword rsp, [rsp-16];
    swapgs
    sysretq


