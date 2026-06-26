#include <stdint.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>

int main(void) {
    uint32_t *test_writing_ptr = (void*)(KERN_BASE + 8);
    *test_writing_ptr = 0xDEADBEEF;

    serial_init();
    serial_printf("Hello %s, %d %d %d %p", "Someone", 1,2,3, test_writing_ptr);

    for(;;);
}