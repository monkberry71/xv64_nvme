#include <stdint.h>
#include <kernel/pci.h>
#include <kernel/msix.h>
#include <kernel/debug.h>
#include <kernel/kvm.h>
#include <kernel/intr.h>
#include <kernel/cpu.h>

static uint16_t pci_msix_read_msg_ctrl(struct pci_func *f, uint8_t cap) {
    return pci_func_read32(f, cap) >> 16;
}

static void pci_msix_write_ctrl(struct pci_func *f, uint8_t cap, uint16_t ctrl) {
    uint32_t word0 = pci_func_read32(f, cap);
    word0 = (word0 & 0x0000FFFF) | ((uint32_t)ctrl << 16);
    pci_func_write32(f, cap, word0);
}

void add_msix_entry(struct pci_func *nvme_f, uint16_t entry_index, uint32_t lapic_id) {
    int msix_cap = pci_find_cap(nvme_f, 0x11);

    if(msix_cap < 0) 
        panic("nvme_msix_init: no cap found");

    struct msix_cap cap;
    uint32_t first_32 = pci_func_read32(nvme_f, msix_cap);

    cap.id = first_32 & 0xFF;
    cap.next = (first_32 >> 8) & 0xFF;
    cap.msg_ctrl = (first_32 >> 16) & 0xFFFF;
    cap.table = pci_func_read32(nvme_f, msix_cap + 4);
    cap.pba = pci_func_read32(nvme_f, msix_cap + 8);

    int count = (cap.msg_ctrl & 0x7FF) + 1;
    if(entry_index < 0 || entry_index >= count)
        panic("nvme_msix_init: entry_index is OOB");

    // Mask first
    cap.msg_ctrl |= BIT(14);
    pci_msix_write_ctrl(nvme_f, msix_cap, cap.msg_ctrl);

    // Set MSI-X Table entry
    uint8_t bar = cap.table & 0x7;
    if(bar >= 6) 
        panic("add_msix_entry: bad MSI-X table BIR");
    uint32_t table_off = cap.table & ~0x7U;

    uint64_t table_pa = nvme_f->bars[bar].base + table_off;
    if(!table_pa)
        panic("add_msix_entry: MSI-X table pa is zero");
    uint32_t table_size = ((cap.msg_ctrl & 0x7FF) + 1) * 16;

    void* msix_table_addr = io_remap(table_pa, table_size);

    volatile struct msix_table_entry *tab = msix_table_addr;

    tab[entry_index].vector_ctrl = BIT(0);
    tab[entry_index].msg_addr_l = 0xFEE00000 | (lapic_id << 12);
    tab[entry_index].msg_addr_h = 0;
    tab[entry_index].msg_data = T_NVME;
    tab[entry_index].vector_ctrl = 0;

    // Unmask and enable MSI-X
    cap.msg_ctrl |= BIT(15);
    cap.msg_ctrl &= ~BIT(14);
    pci_msix_write_ctrl(nvme_f, msix_cap, cap.msg_ctrl);
    
}
