#pragma once
#include <stdint.h>
#include <kernel/seg.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

#define RFLAGS_IF (1ULL << 9)
static inline uint64_t read_rflags(void) {
    uint64_t rflags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(rflags));
    return rflags;
}

// Clear int flag, ignore all int
static inline void cli(void) {
    __asm__ volatile("cli");
}

// Set int flag, receive all int
static inline void sti(void) {
    __asm__ volatile("sti");
}

static inline void lgdt(segment_desc_t gdt[], uint64_t size) {
    volatile struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__ ((packed)) gdtr = { size-1, (uint64_t) gdt};
    __asm__ volatile("lgdt %0" : : "m"(gdtr));
}

// MSRs
#define MSR_GS_BASE 0xC0000101 // gs base of this cpu
#define MSR_KERNEL_GS_BASE 0xC0000102 // gs base reserved for kernel mode
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t) high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" :: "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static inline uint64_t xchg(volatile uint64_t *addr, uint64_t new_val) {
    uint64_t res;
    __asm__ volatile("xchgq %0, %1" : "+m"(*addr), "=a"(res) : "1"(new_val) : );
    return res;
}

static inline void lcr3(uint64_t val) {
    // load(write) cr3
    __asm__ volatile("movq %0,%%cr3" : : "r" (val));
}

static inline ltr(uint16_t selector) {
    __asm__ volatile("ltr %0" : : "r"(selector));
}

