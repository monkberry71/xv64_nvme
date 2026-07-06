format elf64

extrn all_intrs
public vectors
macro vector num {
    public vector#num
    vector#num:
        if ~ num in <8,10,11,12,13,14,17>
            push 0
        end if
        push num
        jmp all_intrs
}
section ".text"
use64
rept 256 num:0 {
    vector num
}

section ".data"
vectors:
    rept 256 num:0 {
        dq vector#num
    }