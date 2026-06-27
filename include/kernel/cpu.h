#pragma once
#include <stdint.h>
#include <kernel/defs.h>
#include <kernel/seg.h>

struct cpu {
    struct cpu* self;
    // uint64_t user_rsp;
    // uint64_t kernel_stack;
    // uint64_t lapic_id;
    // struct context* scheduler
    segment_desc_t gdt[N_SEGS];
    // struct task_state ts;
    int cli_n;
    uint64_t was_intr_enabled;
    // struct proc *proc
};

extern struct cpu cpus[MAX_N_CPUS];

static inline struct cpu *mycpu(void) {
    struct cpu *c;
    __asm__ volatile("movq %%gs:0, %0" : "=r"(c));
    return c;
}

// void cpu_init(void);
void bsp_core_init(void);