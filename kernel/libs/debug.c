#include <stdint.h>
#include <kernel/uart.h>

void panic(char* str) {
    serial_printf("%s", str);
    for(;;) {
        __asm__ volatile("hlt");
    }
}