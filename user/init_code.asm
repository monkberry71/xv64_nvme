format elf64

section ".text" executable

public _start
_start:
    mov rax, 7
    mov rdi, init_path
    mov rsi, argv
    syscall
.loop:
    jmp .loop

section ".rodata"
; console_path db "/console", 0
; message db "write syscall to console works", 10
; message_len = $ - message
init_path db "/init",0
arg0 db "init",0

align 8
argv dq arg0, 0
