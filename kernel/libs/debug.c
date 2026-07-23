#include <stdint.h>
#include <kernel/debug.h>
#include <kernel/uart.h>
#include <kernel/x86_64.h>
#include <kernel/cpu.h>
#include <kernel/acpi.h>

// Note: mycpu calls panic, so panic should not call mycpu in any way possible
void panic(char *str) {
    cli();
    // See the note above
    struct cpu *c;
    __asm__ volatile("movq %%gs:0, %0" : "=r"(c));

    int nth_core = (c - cpus); 
    serial_printf("[core %d] %s", nth_core, str);
    for(;;) {
        __asm__ volatile("hlt");
    }
}

// Log init functions
void log_inits(char *str) {
    serial_printf("[INIT]: %s\n", str);
    return;
}