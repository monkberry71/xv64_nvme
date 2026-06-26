format elf64

KERN_BASE = 0xFFFFFFFF80000000

; Multiboot2 section for mb2 booting
section '.multiboot2' align 8
MB2_MAGIC = 0xE85250D6
MB2_ARCH = 0

mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd mb2_header_end - mb2_header ; size
    dd -(MB2_MAGIC + MB2_ARCH + (mb2_header_end - mb2_header)) ; hash
    dw 0
    dw 0
    dd 8
mb2_header_end:

section '.text.boot' 
public _start ; entry point
_start:
use32
    cli
    mov edi, ebx; keep the mb2 info struct pointer

    ; Let's ready for 64bit jump first

    ; Enable PAE, which makes 64bit addr space possible
    mov eax, cr4
    or eax, 1 shl 5; 5th bit is PAE;
    mov cr4, eax

    ; cr3 holds the physical addr of pml4 addr
    mov eax, entry_pml4
    mov cr3, eax

    ; Enable EFER long mode, to start a real lond mode
    mov ecx, 0xC0000080; the addr of EFER in MSR
    rdmsr
    or eax, 1 shl 8; LAE
    wrmsr

    ; set paging on
    mov eax, cr0
    or eax, 1 shl 31
    mov cr0, eax

    lgdt [gdtr_ptr]
    jmp 8:long_mode_entry

use64
long_mode_entry:
    mov ax, 0x10;
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top + KERN_BASE

    extrn main
    mov rax, main
    jmp rax
.halt:
    hlt
    jmp .halt


section '.rodata.boot' align 4096
macro GDT_desc limit, base, access, flags {
    dw limit
    dw base and 0xFFFF
    db (base shr 16) and 0xFF
    db access
    db flags
    db (base shr 24) and 0xFF
}
gdt:
    dq 0; null desc

    GDT_desc 0xFFFF, 0x0, 0x9A, 0xAF
    ; access (0x9A == 1001 1010) -> Present 1, DPL=00, S=1, Type (code, exec, read)
    ; flags (0xAF == 1010 1111)-> G=1, D=0, Long mode =1, AVL=0
    GDT_desc 0xFFFF, 0x0, 0x92, 0xAF
    ; access (0x92 == 1001 0010) -> Present 1, DPL 00, S 1, Type (data, r/w)
gdt_end:

gdtr_ptr:
    dw gdt_end - gdt - 1
    dd gdt


PDE_P = 1 shl 0 ; present
PDE_RW = 1 shl 1 ; writable
PDE_US = 1 shl 2 ; User
PDE_PWT = 1 shl 3 ; write-through
PDE_PCD = 1 shl 4; cache disable
PDE_A = 1 shl 5 ; accessed
PDE_PS = 1 shl 7; page size 
PDE_XD = 1 shl 63; no exe

macro pte_t addr, flags {
    dq addr + flags
}

align 4096
entry_pml4:
    pte_t p3_ident, PDE_P or PDE_RW; [0]
    times 510 dq 0
    pte_t p3_kernel, PDE_P or PDE_RW; [511]

align 4096
p3_ident:
    pte_t 0, PDE_P or PDE_RW or PDE_PS; [0~1G] -> [0~1G]
    times 511 dq 0; fill zero

align 4096
p3_kernel:
    times 510 dq 0;
    pte_t 0, PDE_P or PDE_RW or PDE_PS; [510]
    dq 0;

section '.bss.boot' align 4096
stack_bottom:
    rb 4096 * 4
stack_top: