format elf64

section ".text"
use64
public test_syscall
test_syscall:
    mov rax, 22 ; sys_draw test
    syscall
    ret