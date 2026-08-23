#pragma once

void ioapic_init(void);
void ioapic_enable(int irq, int apic_id);
