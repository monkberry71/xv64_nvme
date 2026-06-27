#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/bump.h>
#include <kernel/string.h>
#include <kernel/mmu.h>
#include <kernel/mb2.h>
#include <kernel/cpu.h>

// We set edi as a mb2_info_phys in the entry code before jumping to main
int main(uint32_t mb2_info_phys) {
    serial_init();
    bump_init();
    preserve_mb2((void*)mb2_info_phys);
    bsp_core_init();
    bsp_seg_init();
    
    serial_printf("%p", mycpu());
    for(;;);
}