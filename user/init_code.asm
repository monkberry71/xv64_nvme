format elf64

section ".text"
public _start

_start:
    mov rax, 22
    syscall

.hang:
    jmp .hang
