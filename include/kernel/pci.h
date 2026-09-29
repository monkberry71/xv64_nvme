#pragma once
#include <stdint.h>
#include <kernel/acpi.h>

struct pci_ecam_window {
    uint64_t pa;
    volatile uint8_t *va;
    uint16_t segment;
    uint8_t start_bus;
    uint8_t end_bus;
};

struct pci_bar {
    uint64_t base;
    uint64_t size;
    uint8_t is_io;
    uint8_t is_64;
    uint8_t prefetchable;
};

struct pci_func {
    struct pci_ecam_window *window;
    void* base;
    uint16_t segment;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;

    struct pci_bar bars[6];
};

void pci_init(void);

uint32_t pci_func_read32(struct pci_func *f, uint16_t offset);
void pci_func_write32(struct pci_func *f, uint16_t offset, uint32_t val);
void pci_enable_device(struct pci_func *f);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, struct pci_func *out);
int pci_find_cap(struct pci_func *f, uint8_t id_to_find);