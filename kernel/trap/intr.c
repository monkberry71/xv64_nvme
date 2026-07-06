#include <stdint.h>
#include <kernel/intr.h>
#include <kernel/seg.h>
#include <kernel/x86_64.h>
#include <kernel/uart.h>
#include <kernel/debug.h>
#include <kernel/lapic.h>

struct gate_desc idt[256];
extern uint64_t vectors[256];

void init_gate_desc(struct gate_desc *g, uint8_t is_trap, uint8_t ist, uint16_t cs, uint64_t offset, uint8_t dpl) {
    // offset
    g->offset_0_15 = offset & ((1ULL << 16) - 1);
    g->offset_16_31 = (offset >> 16) & ((1ULL << 16) - 1);
    g->offset_32_63 = (offset >> 32) & ((1ULL << 32) - 1);

    // cs and ist
    g->cs = cs;
    g->ist = ist & ((1 << 3) - 1);

    // is_trap and dpl
    // -->
    // 0       3 |4 |5 6 |7
    // GATE_TYPE |0 |DPL |P
    uint8_t type = (is_trap ? GATE_TYPE_TRAP : GATE_TYPE_INT) | (dpl << 5) | (1 << 7);
    g->gate_type = type;
}

// Make a table. Execute it once on the bsp
void tv_init(void) {
    for(int i=0; i<256; i++) {
        init_gate_desc(&idt[i], 0, 0, SEG_KCODE << 3, vectors[i], 0);
    }
    init_gate_desc(&idt[T_DBLFLT], 0, 1, SEG_KCODE << 3, vectors[T_DBLFLT], 0);
}

// set idtr to the table we made. All cores should execute it
void idt_init(void) {
    lidt(idt, sizeof(idt));
}

void intr(struct trap_frame *tf) {
    if(tf->vector_no == T_DBLFLT) {
        panic("intr: double fault");
    }
    if(tf->vector_no == T_IRQ0 + IRQ_TIMER) {
        serial_printf("Timer\n");
        lapic_eoi();
        return;
    }

}
