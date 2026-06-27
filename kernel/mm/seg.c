#include <stdint.h>
#include <kernel/seg.h>
#include <kernel/cpu.h>
#include <kernel/x86_64.h>

segment_desc_t segment_desc_init(uint64_t access, uint64_t flag) {
    // limit and base are ignored
    if(access > 255) {
        // access must be 8bit
        return 0;
    }

    if(flag > 15) {
        // flag must be 4bit
        return 0;
    }
    return ((uint64_t) access << 40) | (((uint64_t) flag & 0xF) << 52);
}

void bsp_seg_init(void) {
    struct cpu *c = &cpus[0];
    
    // Access
    // //// P |DPL |S |Type 
    // 0x9A 1 |00  |1 |1010
    // 0x92 1 |00  |1 |0010

    // Flag
    // /// G |D/B |L |AVL
    // 0xA 1 |0   |1 |0
    // 0xC 1 |1   |0 |0

    c->gdt[SEG_KCODE] = segment_desc_init(0x9A, 0xA);
    c->gdt[SEG_KDATA] = segment_desc_init(0x92, 0xC);
    c->gdt[SEG_UDATA32] = segment_desc_init(0,0);
    c->gdt[SEG_UDATA] = segment_desc_init(0xF2, 0xC);
    c->gdt[SEG_UCODE] = segment_desc_init(0xFA, 0xA);

    lgdt(c->gdt, sizeof(c->gdt));

    wrmsr(MSR_GS_BASE, (uint64_t) c); // real gs base
    wrmsr(MSR_KERNEL_GS_BASE, 0); // CPU doesnt actually use this as gs base, it is mere storing only, but swapgs command will swap this and real gs_base regi
}