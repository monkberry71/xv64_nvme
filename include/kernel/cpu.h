#pragma once
#include <stdint.h>
#include <stddef.h>
#include <kernel/defs.h>
#include <kernel/seg.h>
#include <kernel/cpu.h>
#include <kernel/x86_64.h>
#include <kernel/debug.h>

struct cpu {
    struct cpu* self;
    // When syscall is called, user rsp is not automatically saved.
    // If we want to push it to the kstack, we need to change the rsp register itself first
    // so we need to store it on the cpu struct, the only place where just-kernel-entered-via-syscall thread
    // can reliably and temporarily save the user_rsp. It is just a scratch pad before pushing it on the kstack
    uint64_t user_rsp_temp;
    // Like tss.rsp[0], syscall needs to know where the kernel stack is.
    // We can use tss.rsp[0], but for now let's use a separate field while testing,
    // since intr references the tss.rsp[0] 
    uint64_t kstack_temp;
    uint64_t lapic_id; // used for mycpu in the original xv6, we don't need it actually but just in case
    struct context* scheduler;
    segment_desc_t gdt[N_SEGS];
    struct task_state_segment tss; 
    int cli_n;
    uint64_t was_intr_enabled;
    struct proc *proc;
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