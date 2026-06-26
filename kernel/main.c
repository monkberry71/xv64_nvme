#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/bump.h>
#include <kernel/string.h>
#include <kernel/mmu.h>

int main(void) {
    uint32_t *test_writing_ptr = (void*)(KERN_BASE + 8);
    *test_writing_ptr = 0xDEADBEEF;

    serial_init();
    serial_printf("Hello %s, %d %d %d %p\n", "Someone", 1,2,3, test_writing_ptr);

    bump_init();
    char *bump_test_p = bump_alloc();
    char *next_bump = bump_alloc();
    // memset(bump_test_p, 0, PGSIZE_4KB);

    memcpy(bump_test_p, "BUMP TEST!", 11);
    serial_printf("%s %d", bump_test_p, next_bump - bump_test_p);
    

    for(;;);
}