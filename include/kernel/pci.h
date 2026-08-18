#pragma once
#include <stdint.h>
#include <kernel/acpi.h>

struct pci_bar {
    uint64_t base;
    uint64_t size;
    uint8_t is_io;
    uint8_t is_64;
    uint8_t prefetchable;
};

struct pci_func {
    struct mcfg_entry *mcfg_entry;
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
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, struct pci_func *out);