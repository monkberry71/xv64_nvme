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

extern struct cpu cpus[MAX_N_CPUS];

void acpi_init(void);


