#include <stdint.h>
#include <kernel/uart.h>

void panic(char* str) {
    for(;;) {
        __asm__ volatile("hlt");
    }
}