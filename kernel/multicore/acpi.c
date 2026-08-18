#include <stdint.h>
#include <kernel/acpi.h>
#include <kernel/cpu.h>
#include <kernel/string.h>
#include <kernel/mem_layout.h>
#include <kernel/debug.h>
#include <kernel/mb2.h>
#include <kernel/uart.h>
#include <kernel/defs.h>

struct cpu cpus[MAX_N_CPUS];
uint8_t lapic_ids[MAX_N_CPUS];
int n_cpu;
struct xsdt *xsdt;

static int checksum_ok(const void* table, uint32_t len) {
    uint8_t sum = 0;
    const uint8_t *p = table;
    for(uint32_t i = 0; i < len; i++) {
        sum += p[i];
    }
    return sum == 0;
}

static struct xsdt* find_xsdt(void) {
    struct mb2_it it;
    extern struct mb2_info *reserved_mb2_info;
    mb2_it_init(&it, reserved_mb2_info);

    for(; !it.end; mb2_it_next(&it)) {
        if(it.curr->type != MB2_TAG_ACPI) continue;

        struct mb2_tag_acpi *mm_tag = (void*) it.curr;
        struct rsdp *rsdp = (void*) mm_tag->rsdp;

        if(memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
            panic("find_xsdt: rsdp signature can't be found");
        }
        
        if(!checksum_ok(rsdp, rsdp->length)) {
            panic("find_xsdt: rsdp checksum failed");
        }

        return P2V_DIR(rsdp->xsdt_addr);
    }
    panic("find_xsdt: no ACPI tag from mb2 bootloader");
}

void* xsdt_find_table(const char sig[4]) {
    int table_length = (xsdt->header.length - sizeof(struct acpi_sdt_header)) / sizeof(uint64_t);
    for(int i=0; i<table_length; i++) {
        const struct acpi_sdt_header* table_header = P2V_DIR(xsdt->table_ptrs[i]);
        if(memcmp(&table_header->signature, sig, 4) == 0 && checksum_ok(table_header, table_header->length)) {
            return (void*) table_header;
        }
    }
    return 0;
}

void madt_it_init(struct madt_it *it, const struct madt *madt) {
    it->curr = (struct madt_entry_header*) madt->entries;
    it->limit = (uint8_t*) madt + madt->header.length;
    it->end = ((uint8_t*) it->curr >= it->limit);
}

void madt_it_next(struct madt_it *it) {
    it->curr = (struct madt_entry_header*) ((uint8_t*) it->curr + it->curr->length);
    if((uint8_t*) it->curr >= it->limit) {
        it->end = 1;
    }
}

void mcfg_it_init(struct mcfg_it *it, const struct mcfg *mcfg) {
    it->curr = (struct mcfg_entry *) mcfg->entries;
    it->limit = (void*) ((uint8_t*) mcfg + mcfg->header.length);
    it->end = it->curr >= it->limit;
}

void mcfg_it_next(struct mcfg_it *it) {
    it->curr++;
    if(it->curr >= it->limit) {
        it->end = 1;
    }
}

void acpi_init(void) {
    xsdt = find_xsdt();
    struct madt *madt = xsdt_find_table("APIC");

    if(!madt) {
        panic("acpi_init: MADT not found");
    }

    struct madt_it it;
    madt_it_init(&it, madt);
    n_cpu = 0;
    for(; !it.end; madt_it_next(&it)) {
        if(it.curr->type == 0) {
            struct madt_lapic *lapic = (void*) it.curr;
            if(n_cpu < MAX_N_CPUS && lapic->flags & 1) {
                lapic_ids[n_cpu++] = lapic->apic_id;
            }
        }
    }

    // We assume that bsp_lapic_init is done. 
    int cpus_i = 1;
    for(int i=0; i<n_cpu; i++) {
        if(lapic_ids[i] == mycpu()->lapic_id) continue;

        if(cpus_i < MAX_N_CPUS) {
            cpus[cpus_i++].lapic_id = lapic_ids[i];
        }
    }

    serial_printf("Cores : %d\n", n_cpu);
    log_inits("acpi_init");
}

