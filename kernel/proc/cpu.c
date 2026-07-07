#include <stdint.h>
#include <kernel/cpu.h>
#include <kernel/defs.h>
#include <kernel/bump.h>
#include <kernel/mmu.h>

struct cpu cpus[MAX_N_CPUS] = {0};

void bsp_core_init(void) {
    struct cpu *bsp_core = &cpus[0];
    bsp_core->self = bsp_core;
    
    // just for testing syscall, delete it later
    bsp_core->kernel_stack = (uint64_t) bump_alloc() + PGSIZE_4KB; 

    log_inits("bsp_core_init");
}