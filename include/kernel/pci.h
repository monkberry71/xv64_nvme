#pragma once
#include <stdint.h>

struct pci_func {
    struct mcfg_entry *mcfg;
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
};

void pci_init(void);