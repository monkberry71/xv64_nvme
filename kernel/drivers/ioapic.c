#include <stdint.h>
#include <kernel/ioapic.h>
#include <kernel/kvm.h>
#include <kernel/mmu.h>
#include <kernel/intr.h>

// Default physical address of the I/O APIC.
#define IOAPIC 0xFEC00000ULL

#define REG_ID     0x00
#define REG_VER    0x01
#define REG_TABLE  0x10

#define INT_DISABLED  0x00010000

struct ioapic {
    uint32_t reg;
    uint32_t pad[3]; // padding for nothing
    uint32_t data;
};

static volatile struct ioapic *ioapic;

static uint32_t ioapic_read(int reg) {
    ioapic->reg = reg;
    return ioapic->data;
}

static void ioapic_write(int reg, uint32_t data) {
    ioapic->reg = reg;
    ioapic->data = data;
}

void ioapic_init(void) {
    ioapic = io_remap(IOAPIC, PGSIZE_4KB);
    int maxintr = (ioapic_read(REG_VER) >> 16) & 0xFF;

    for(int i = 0; i <= maxintr; i++) {
        ioapic_write(REG_TABLE + 2*i, INT_DISABLED | (T_IRQ0 + i));
        ioapic_write(REG_TABLE + 2*i + 1, 0);
    }
}

void ioapic_enable(int irq, int apic_id) {
    ioapic_write(REG_TABLE + 2*irq, T_IRQ0 + irq);
    ioapic_write(REG_TABLE + 2*irq + 1, (uint32_t)apic_id << 24);
}
