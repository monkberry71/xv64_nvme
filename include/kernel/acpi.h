#pragma once
#include <stdint.h>
#include <kernel/defs.h>
#include <kernel/cpu.h>

struct rsdp {
    char signature[8]; // "RSD PTR "
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision; // 2 = ACPI 2.0
    uint32_t rsdt_addr; // for regacy 32bit

    uint32_t length; 
    uint64_t xsdt_addr; 
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4]; 
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct xsdt {
    struct acpi_sdt_header header;
    uint64_t table_ptrs[];
} __attribute__((packed));

struct madt {
    struct acpi_sdt_header header; // "APIC"
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_lapic {
    struct madt_entry_header header;
    uint8_t acpi_proc_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_it {
    struct madt_entry_header *curr;
    uint8_t *limit;
    int end;
};

struct mcfg_entry {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
} __attribute__((packed));

struct mcfg {
    struct acpi_sdt_header header;
    uint64_t reserved;
    struct mcfg_entry entries[];
} __attribute__((packed));

struct mcfg_it {
    struct mcfg_entry *curr;
    struct mcfg_entry *limit; // last entry's next addr
    int end;
};

extern struct cpu cpus[MAX_N_CPUS];

void* xsdt_find_table(const char sig[4]);
void acpi_init(void);

void mcfg_it_init(struct mcfg_it *it, const struct mcfg *mcfg);
void mcfg_it_next(struct mcfg_it *it);



