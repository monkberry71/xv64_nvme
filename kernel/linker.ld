ENTRY(_start)
KERN_BASE = 0xFFFFFFFF80000000;

SECTIONS {
    . = 0x100000;
    
    /* entry codes that executes in pre-paging phase,
    so we need to link it with physical addr */

    .boot ALIGN(4096) : {
        KEEP(*(.multiboot2))
        *(.text.boot)
        *(.rodata.boot)
    }

    .bss.boot ALIGN(4096) (NOLOAD) : {
        *(.bss.boot)
    }

    /* after paging is enabled, we use virtual addr of kernel,
    which is just KERN_BASE added to physical addr
    */
    . += KERN_BASE;

    .text ALIGN(4096) : AT(ADDR(.text) - KERN_BASE)  {
        *(.text*)
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERN_BASE)  {
        *(.rodata*)
    }

    .data ALIGN(4096) : AT(ADDR(.data) - KERN_BASE)  {
        *(.data*)
    }

    .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss) - KERN_BASE)  {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4096);
    _kernel_phys_end = . - KERN_BASE;
    /* for bump alloc */
}