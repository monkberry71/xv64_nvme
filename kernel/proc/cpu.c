#include <stdint.h>
#include <kernel/cpu.h>
#include <kernel/defs.h>

struct cpu cpus[MAX_N_CPUS] = {0};

void bsp_core_init(void) {
    cpus[0].self = &cpus[0];
}