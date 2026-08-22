#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/bump.h>
#include <kernel/string.h>
#include <kernel/mmu.h>
#include <kernel/mb2.h>
#include <kernel/cpu.h>
#include <kernel/kvm.h>
#include <kernel/kalloc.h>
#include <kernel/debug.h>
#include <kernel/gop.h>
#include <kernel/intr.h>
#include <kernel/lapic.h>
#include <kernel/x86_64.h>
#include <kernel/syscall.h>
#include <kernel/proc.h>
#include <kernel/acpi.h>
#include <kernel/pci.h>
#include <kernel/nvme.h>
#include <kernel/bio.h>

// We set edi as a mb2_info_phys in the entry code before jumping to main
int main(uint32_t mb2_info_phys) {
    serial_init();
    bump_init();
    preserve_mb2((void*)(uint64_t)mb2_info_phys);

    bsp_core_init();
    bsp_seg_init();

    kvm_alloc();
    kalloc_init();
    io_init();

    gop_init();

    tv_init();
    idt_init();
    bsp_lapic_init();
    acpi_init();
    pci_init();
    nvme_init();
    b_init();

    syscall_init();
    test_scheduler();
    for(;;);
}