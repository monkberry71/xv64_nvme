#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/bump.h>
#include <kernel/string.h>
#include <kernel/mmu.h>
#include <kernel/mb2.h>
#include <kernel/cpu.h>
#include <kernel/kvm.h>

// We set edi as a mb2_info_phys in the entry code before jumping to main
int main(uint32_t mb2_info_phys) {
    serial_init();
    bump_init();
    preserve_mb2((void*)mb2_info_phys);

    bsp_core_init();
    bsp_seg_init();

    // Direct mapping and test
    kvm_alloc();

    // Test:
    // If the direct mapping works, we would see it by the kernel mapping too

    // Write as the kernel mapping, Read from the direct mapping
    char *test_p = bump_alloc();
    memcpy(test_p, "WRITTEN FROM KERNEL MAPPED ADDR", 32);

    uint64_t pa_of_test_p = V2P_KERN(test_p);
    void* direct_mapped_test_p = P2V_DIR(pa_of_test_p);

    serial_printf("%s\n", direct_mapped_test_p);

    // Write as the kernel mapping, Read from the kernel mapping
    memcpy(direct_mapped_test_p, "WRITTEN FROM DIRECT MAPPED ADDR", 32);
    serial_printf("%s", test_p);

    for(;;);
}