#pragma once
#include <stdint.h>
#include <kernel/defs.h>
#include <kernel/seg.h>
#include <kernel/cpu.h>
#include <kernel/x86_64.h>
#include <kernel/debug.h>

struct cpu {
    struct cpu* self;
    // uint64_t user_rsp;
    // uint64_t kernel_stack;
    uint64_t lapic_id; // used for mycpu in the original xv6, we don't need it actually but just in case
    // struct context* scheduler
    segment_desc_t gdt[N_SEGS];
    struct task_state_segment tss;
    int cli_n;
    uint64_t was_intr_enabled;
    // struct proc *proc
};

extern struct cpu cpus[MAX_N_CPUS];

static inline struct cpu *mycpu(void) {
    struct cpu *c;
    if(read_rflags() & RFLAGS_IF) {
        panic("mycpu: called with interrupts enabled");
    }
    __asm__ volatile("movq %%gs:0, %0" : "=r"(c));
    return c;
}

// void cpu_init(void);
void bsp_core_init(void);