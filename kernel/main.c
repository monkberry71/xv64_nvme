#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/bump.h>
#include <kernel/string.h>
#include <kernel/mmu.h>
#include <kernel/mb2.h>

// We set edi as a mb2_info_phys in the entry code before jumping to main
int main(uint32_t mb2_info_phys) {
    serial_init();
    bump_init();
    preserve_mb2((void*)mb2_info_phys);
    
    // mb2 iterating test
    extern struct mb2_info *reserved_mb2_info;
    struct mb2_it test_it;
    mb2_it_init(&test_it, reserved_mb2_info);
    for(; !test_it.end; mb2_it_next(&test_it)) {
        serial_printf("mb2 tag : %d\n", test_it.curr->type);
    }
    for(;;);
}