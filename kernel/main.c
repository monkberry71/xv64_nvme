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

// We set edi as a mb2_info_phys in the entry code before jumping to main
int main(uint32_t mb2_info_phys) {
    serial_init();
    bump_init();
    preserve_mb2((void*)(uint64_t)mb2_info_phys);

    bsp_core_init();
    bsp_seg_init();

    kvm_alloc();
    kalloc_init();

    // Test: kalloc
    uint32_t *test = kalloc();
    memcpy(test, "DEADBEEF\n", 10);
    serial_printf("%s", test);

    char* p1 = kalloc();
    char* p2 = kalloc();
    if(p1 == p2) {
        panic("kalloc test: kalloc failed");
    }

    memset(p1, 0xAA, PGSIZE_4KB);
    if(*p1 != *(p1+PGSIZE_4KB-1)) {
        panic("kalloc test: page filling failed");
    }
    memset(p2, 0xBB, PGSIZE_4KB);
    if(*p2 != *(p2+PGSIZE_4KB-1)) {
        panic("kalloc test: page filling failed");
    }
    kfree(p1);
    void* p3 = kalloc();
    if(p1 != p3) {
        panic("kalloc test: kfree failed");
    }

    int count = 0;
    while(kalloc()) count++;
    serial_printf("%d", count);

    for(;;);
}