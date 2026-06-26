#include <stdint.h>
#include <kernel/bump.h>
#include <kernel/mmu.h>
#include <kernel/mem_layout.h>
#include <kernel/string.h>
#include <kernel/debug.h>

// Very simple bump memory allocator.
// Used for
// 1. Multiboot2 preserving
// 2. Allocating the basic pages for kernel mapping

// At the very first time of the booting, the entry code will only give us the two basic mappings.
// But we need to make a real kalloc func, so we need to make a direct mapping of all available physical addrs
// But we need an allocated page for the direct mapping page table. Thus we build some basic allocator

static struct bump_mem boot_bump_mem;

int bump_init(void) {
    boot_bump_mem.enabled = 1;

    // _kernel_phys_end is made by the linker
    // In the symbol table, the entry is symbol name with the address of it
    // Thus, if we just declare it like normal variable, (for example, `extern uint64_t _kernel_phys_end`)
    // The compiler will load a value from that address when we use that symbol, which is something not that we wanted.
    // But array names will decay to the address of it, so we use char []
    extern char _kernel_phys_end[];

    // the end is aleady aligned, don't worry about it
    boot_bump_mem.bump_line = (uint64_t) _kernel_phys_end;
    return 0;
}

// The bump allocator keeps the physical addr, but when it gives virtual kernel mapping addr
void* bump_alloc(void) {
    if(!boot_bump_mem.enabled) {
        panic("Bad bump allocator usage");
    }
    uint64_t p = boot_bump_mem.bump_line;
    boot_bump_mem.bump_line += PGSIZE_4KB;
    
    memset((void*)P2V_KERN(p), 0, PGSIZE_4KB); // bump allocated pages will be used for page table, so we need to set 0 beforehandly
    return (void*) P2V_KERN(p);
}

// We must not use bump allocator after we are able to use kalloc, it might mess up the kalloc
void bump_disable(void) {
    boot_bump_mem.enabled = 0;
}

// We might need it when we makin kalloc
void* get_bump_line(void) {
    return (void*) boot_bump_mem.bump_line;
}