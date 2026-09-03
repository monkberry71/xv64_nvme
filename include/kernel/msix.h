#pragma once
#include <stdint.h>
#include <kernel/helper.h>
#include <kernel/pci.h>

struct msix_cap {
    uint8_t id;
    uint8_t next;
    uint16_t msg_ctrl;
    uint32_t table;
    uint32_t pba;
};

struct msix_table_entry {
    uint32_t msg_addr_l;
    uint32_t msg_addr_h;
    uint32_t msg_data;
    uint32_t vector_ctrl;
};

void add_msix_entry(struct pci_func *nvme_f, uint16_t entry_index, uint32_t lapic_id);