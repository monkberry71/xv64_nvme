format elf64

section ".text"

SYS_fork   equ 1
SYS_exit   equ 2
SYS_wait   equ 3
SYS_pipe   equ 4
SYS_read   equ 5
SYS_kill   equ 6
SYS_exec   equ 7
SYS_fstat  equ 8
SYS_chdir  equ 9
SYS_dup    equ 10
SYS_getpid equ 11
SYS_sbrk   equ 12
SYS_sleep  equ 13
SYS_uptime equ 14
SYS_open   equ 15
SYS_write  equ 16
SYS_mknod  equ 17
SYS_unlink equ 18
SYS_link   equ 19
SYS_mkdir  equ 20
SYS_close  equ 21
SYS_draw   equ 22

macro SYSCALL syscall_name {
    public syscall_name
    syscall_name:
        mov r10, rcx
        mov rax, SYS_#syscall_name
        syscall
        ret
}

SYSCALL fork
SYSCALL exit
; SYSCALL wait ;ok wait is reserved in fasm
; SYSCALL sys_wait
public sys_wait
sys_wait:
    mov r10, rcx
    mov rax, SYS_wait
    syscall
    ret

SYSCALL pipe
SYSCALL read
SYSCALL kill
SYSCALL exec
SYSCALL fstat
SYSCALL chdir
; SYSCALL dup ; dup too
public sys_dup
sys_dup:
    mov r10, rcx
    mov rax, SYS_dup
    syscall
    ret

SYSCALL getpid
SYSCALL sbrk
SYSCALL sleep
SYSCALL uptime
SYSCALL open
SYSCALL write
SYSCALL mknod
SYSCALL unlink
SYSCALL link
SYSCALL mkdir
SYSCALL close
SYSCALL draw