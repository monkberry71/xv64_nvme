#include <stdint.h>
#include <kernel/seg.h>
#include <kernel/cpu.h>
#include <kernel/x86_64.h>
#include <kernel/string.h>
#include <kernel/defs.h>

segment_desc_t init_segment_desc(uint64_t access, uint64_t flag) {
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

void init_tss_desc(struct tss_descriptor *d, uint64_t base, uint32_t limit, uint8_t flags, uint8_t access) {
    d->base_0_15 = base & ((1ULL << 16) - 1);
    d->base_16_23 = (base >> 16) & ((1ULL << 8) - 1);
    d->base_24_31 = (base >> 24) & ((1ULL << 8) - 1);
    d->base_32_63 = (base >> 32) & ((1ULL << 32) - 1);

    d->limit_0_15 = limit & ((1UL << 16) - 1);
    d->limit_16_19_and_flags = (limit >> 16) & ((1ULL << 4) - 1);
    
    d->limit_16_19_and_flags |= ((flags & 0xF) << 4);

    d->access = access;

    d->reserved = 0;
}

uint8_t __attribute__ ((aligned(16))) ist0[KERNEL_STACK_SIZE];
void tss_init(struct cpu* c) {
    memset(&c->tss, 0, sizeof(c->tss));
    c->tss.ist[0] = (uint64_t)(ist0 + sizeof(ist0));
    c->tss.iopb = sizeof(c->tss);
}

void bsp_seg_init(void) {
    // https://wiki.osdev.org/Global_Descriptor_Table
    struct cpu *c = &cpus[0];
    
    // Access
    // //// P |DPL |S |Type 
    // 0x9A 1 |00  |1 |1010
    // 0x92 1 |00  |1 |0010

    // Flag
    // /// G |D/B |L |AVL
    // 0xA 1 |0   |1 |0
    // 0xC 1 |1   |0 |0

    c->gdt[SEG_KCODE] = init_segment_desc(0x9A, 0xA);
    c->gdt[SEG_KDATA] = init_segment_desc(0x92, 0xC);
    c->gdt[SEG_UDATA32] = init_segment_desc(0,0);
    c->gdt[SEG_UDATA] = init_segment_desc(0xF2, 0xC);
    c->gdt[SEG_UCODE] = init_segment_desc(0xFA, 0xA);

    lgdt(c->gdt, sizeof(c->gdt));

    wrmsr(MSR_GS_BASE, (uint64_t) c); // real gs base
    wrmsr(MSR_KERNEL_GS_BASE, 0); // CPU doesnt actually use this as gs base, it is mere storing only, but swapgs command will swap this and real gs_base regi

    tss_init(c);

    // Access
    // //// P |DPL |S |Type
    // 0x89 1 |00  |0 |1001

    // Flag
    // /// G |D/B |L |AVL
    // 0   0 |0   |0 |0
    init_tss_desc(
        (void*) &c->gdt[SEG_TSS],
        (uint64_t)&c->tss,
        sizeof(c->tss) - 1,
        0,
        0x89
    );

    ltr(SEG_TSS << 3);
}