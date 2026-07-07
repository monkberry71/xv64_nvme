#include <stdint.h>
#include <kernel/x86_64.h>
#include <kernel/helper.h>
#include <kernel/mmu.h>
#include <kernel/kvm.h>
#include <kernel/debug.h>
#include <kernel/cpu.h>

#define APIC_ID (0x0020/4)
#define APIC_VER (0x0030/4)
#define APIC_TPR (0x0080/4)
#define APIC_EOI (0x00B0/4)
#define APIC_SVR (0x00F0/4)
    #define SVR_APIC_ENABLE (0x100)
#define APIC_ESR (0x0280/4)
#define APIC_ICR_LOW (0x0300/4)
    #define ICR_LOW_INIT (0x500)
    #define ICR_LOW_STARTUP (0x600)
    #define ICR_LOW_DELIVS (0x1000)
    #define ICR_LOW_ASSERT (0x4000)
    #define ICR_LOW_DEASSERT (0x0)
    #define ICR_LOW_LEVEL (0x8000)
    #define ICR_LOW_BCAST (0x80000)
    #define ICR_LOW_BUSY (0x1000)
    #define ICR_LOW_FIXED (0x0)
#define APIC_ICR_HIGH (0x0310/4)

// Local Vector Table
#define APIC_TIMER (0x0320/4)
    #define TIMER_X1 (0xB)
    #define TIMER_PERIODIC (0x20000)

#define APIC_PERF (0x0340/4)
#define APIC_LINT0 (0x0350/4)
#define APIC_LINT1 (0x0360/4)
#define APIC_ERROR (0x0370/4)

#define LVT_MASKED (0x10000)

// Timer
#define APIC_TICR (0x0380/4)
#define APIC_TCCR (0x0390/4)
#define APIC_TDCR (0x03E0/4) 


volatile uint32_t *lapic;

// lapic MMIO helper
static void lapic_write(uint64_t index, uint64_t val) {
    lapic[index] = val;
    (void)lapic[APIC_ID]; 
}

void lapic_eoi(void) {
    lapic_write(APIC_EOI, 0);
}

// Make a mapping for the physical addr of LAPIC
void lapic_mapping_init(void) {
    uint64_t apic_base = rdmsr(MSR_APIC_BASE);

    // The real physical addr of apic registers are page aligned
    apic_base = ROUND_DOWN(apic_base, PGSIZE_4KB);

    // get it mapped
    lapic = io_remap(apic_base, PGSIZE_4KB);
    if(!lapic) panic("lapic_mapping_init: cannot map lapic");
}

// per core lapic init
void lapic_per_core_init(void) {
    // Check if 
    uint64_t apic_base = rdmsr(MSR_APIC_BASE);

    // 12th bit is enable bit
    if(!(apic_base & BIT(11))) {
        wrmsr(MSR_APIC_BASE, apic_base | BIT(11));
    }

    // Enable the LAPIC with SVR register and set spurious vector number too
    lapic_write(APIC_SVR, SVR_APIC_ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

    // Timer setup
    lapic_write(APIC_TDCR, TIMER_X1);
    lapic_write(APIC_TIMER, TIMER_PERIODIC | (T_IRQ0 + IRQ_TIMER));
    lapic_write(APIC_TICR, 10000000);

    // Set LVTs
    lapic_write(APIC_PERF, LVT_MASKED);
    lapic_write(APIC_LINT0, LVT_MASKED);
    lapic_write(APIC_LINT1, LVT_MASKED);
    lapic_write(APIC_ERROR, T_IRQ0 + IRQ_ERROR);

    // Clear ESR, two times
    lapic_write(APIC_ESR, 0);
    lapic_write(APIC_ESR, 0);

    // EOI
    lapic_write(APIC_EOI, 0);

    // Allow all priority
    lapic_write(APIC_TPR, 0);

    mycpu()->lapic_id = lapic[APIC_ID] >> 24;
}

void bsp_lapic_init(void) {
    lapic_mapping_init();
    lapic_per_core_init();
    log_inits("bsp_lapic_init");
}