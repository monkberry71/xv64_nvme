#include <stdint.h>
#include <kernel/cpu.h>
#include <kernel/defs.h>
#include <kernel/bump.h>
#include <kernel/mmu.h>
#include <kernel/debug.h>
#include <kernel/acpi.h>

void bsp_core_init(void) {
    struct cpu *bsp_core = &cpus[0];
    bsp_core->self = bsp_core;

    log_inits("bsp_core_init");
}