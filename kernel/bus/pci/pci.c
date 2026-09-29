#include <stdint.h>
#include <kernel/pci.h>
#include <kernel/acpi.h>
#include <kernel/debug.h>
#include <kernel/uart.h>
#include <kernel/mem_layout.h>
#include <kernel/kvm.h>

struct mcfg *mcfg;
static struct pci_ecam_window windows[16] = {0};
static int n_windows = 0;

// In the ECAM window to which the MCFG entry points,
// read the certain register of the config space
static uint32_t pci_read32_ecam(struct pci_ecam_window *win, uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset) {
    uint64_t bus_offset = bus - win->start_bus;
    uint64_t dev_offset = dev;
    uint64_t func_offset = func;
    uint64_t addr = ((uint64_t) win->va + (bus_offset << 20) + (dev_offset << 15) + (func_offset << 12)) + offset;

    volatile uint32_t *p = (void*) addr;
    return *p;
}

static void pci_write32_ecam(struct pci_ecam_window *win, uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset, uint32_t val) {
    uint64_t bus_offset = bus - win->start_bus;
    uint64_t dev_offset = dev;
    uint64_t func_offset = func;
    uint64_t addr = ((uint64_t) win->va + (bus_offset << 20) + (dev_offset << 15) + (func_offset << 12)) + offset;

    volatile uint32_t *p = (void*) addr;
    *p = val;
}

// public helpers that operates on struct pci_func
uint32_t pci_func_read32(struct pci_func *f, uint16_t offset) {
    return pci_read32_ecam(f->window, f->bus, f->dev, f->func, offset);
}

void pci_func_write32(struct pci_func *f, uint16_t offset, uint32_t val) {
    pci_write32_ecam(f->window, f->bus, f->dev, f->func, offset, val);
}

// Setup basic
void pci_enable_device(struct pci_func *f) {
    uint32_t reg = pci_func_read32(f, 0x04);

    uint16_t cmd = reg & 0xFFFF;
    uint16_t status = reg >> 16;

    cmd |= BIT(1); // Memoery Space
    cmd |= BIT(2); // Bus Master 
    cmd |= BIT(10); // Disable the legacy intr

    reg = ((uint32_t) status << 16) | cmd;
    pci_func_write32(f, 0x04, reg);
}

// Set the bar entries
static void pci_fill_bars(struct pci_func *f) {
    for(int i=0; i<6; i++) {
        uint32_t raw = pci_read32_ecam(f->window, f->bus, f->dev, f->func, 0x10 + i * 4);

        // 0 -->
        // Memory Bar? | Memory Bar Type | Prefetchable | Base_addr
        // 1 | 2 | 1 | ...
        f->bars[i].base = 0;
        f->bars[i].size = 0x4000; // Ok this is miserable but NVMe has at least 0x4000 sized bar0...
        f->bars[i].is_io = raw & 1;
        f->bars[i].is_64 = 0;
        f->bars[i].prefetchable = 0;

        if(raw == 0) continue;
        if(raw & BIT(0)) {
            // Port IO Bar
            f->bars[i].base = raw & (~0x3ULL);
        } else {
            // MMIO Bar
            uint8_t type = (raw >> 1) & 0x3;
            f->bars[i].prefetchable = (raw >> 3) & 1;

            if(type == 0x2) {
                // 10 means 64bit
                uint32_t high = pci_read32_ecam(f->window, f->bus, f->dev, f->func, 0x10 + (i+1) * 4);
                f->bars[i].base = ((uint64_t) high << 32) | (raw & ~0xFULL);
                f->bars[i].is_64 = 1;
                i++;
            } else {
                // 00 means 32bit
                f->bars[i].base = raw & ~0xFULL;
            }
        }
    }
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, struct pci_func *out) {
    for(int i=0; i<n_windows; i++) {
        struct pci_ecam_window *w = &windows[i];

        for(int bus = w->start_bus; bus <= w->end_bus; bus++) {
            for(int dev = 0; dev < 32; dev++) {
                uint32_t id0 = pci_read32_ecam(w, bus, dev, 0, 0);
                uint16_t vendor0 = id0 & 0xFFFF;

                // If Function[0] is FFFF(empty), the whole device is actually empty
                if(vendor0 == 0xFFFF) continue; 

                uint32_t hdr0 = pci_read32_ecam(w, bus, dev, 0, 0xC);
                uint8_t header_type0 = (hdr0 >> 16) & 0xFF;
                uint8_t n_func = (header_type0 & 0x80) ? 8 : 1;

                for(uint8_t f = 0; f < n_func; f++) {
                    uint32_t id = pci_read32_ecam(w, bus, dev, f, 0);
                    uint16_t vendor = id & 0xFFFF;

                    if(vendor == 0xFFFF) continue;

                    uint16_t device = (id >> 16) & 0xFFFF;
                    // class_reg == Rev | Prog IF | SubCls | Cls
                    uint32_t class_reg = pci_read32_ecam(w, bus, dev, f, 0x8);

                    uint8_t this_cls = (class_reg >> 24) & 0xFF;
                    uint8_t this_subcls = (class_reg >> 16) & 0xFF;
                    uint8_t this_prog_if = (class_reg >> 8) & 0xFF;

                    if(this_cls == class_code && this_subcls == subclass && this_prog_if == prog_if) {
                        // Found
                        uint8_t header_type = pci_read32_ecam(w, bus, dev, f, 0xC) >> 16;

                        out->window = w;
                        out->segment = w->segment;
                        out->bus = bus;
                        out->dev = dev;
                        out->func = f;
                        out->vendor_id = vendor;
                        out->device_id = device;
                        out->class_code = this_cls;
                        out->subclass = this_subcls;
                        out->prog_if = this_prog_if;
                        out->header_type = header_type;

                        pci_fill_bars(out);
                        return 0;
                    }
                }
            }
        }
    }
    return -1;
}

int pci_find_cap(struct pci_func *f, uint8_t id_to_find) {
    uint32_t cmd_status = pci_func_read32(f, 0x04);
    uint16_t status = cmd_status >> 16;

    if((status & BIT(4)) == 0) return -1;

    uint8_t cap = pci_func_read32(f, 0x34) & 0xFF;
    cap &= ~0x3;

    while(cap) {
        uint32_t hdr = pci_func_read32(f, cap);

        uint8_t id = hdr & 0xFF;
        if(id == id_to_find) return cap;

        uint8_t next = (hdr >> 8) & 0xFF;
        cap = next & ~0x3;
    }

    return -1;
}


// Fill the windows array
void pci_init(void) {
    mcfg = xsdt_find_table("MCFG");

    if(!mcfg) {
        panic("pci_init: can't find MCFG table");
    }

    struct mcfg_it it;
    mcfg_it_init(&it, mcfg);

    int num;
    for(num = 0; !it.end; mcfg_it_next(&it)) { // foreach mcfg
        if(num >= 16) 
            panic("pci_init: there is more MCFG entries than 16");

        uint64_t bus_count = it.curr->end_bus - it.curr->start_bus + 1;
        uint64_t size = bus_count << 20; // 1MB per bus in ECAM

        windows[num].pa = it.curr->base_address;
        windows[num].va = io_remap(it.curr->base_address, size);
        windows[num].segment = it.curr->segment_group;
        windows[num].start_bus = it.curr->start_bus;
        windows[num].end_bus = it.curr->end_bus;

        serial_printf("MCFG base=%p seg=%d bus=%d - %d\n",
            windows[num].pa,
            windows[num].segment,
            windows[num].start_bus,
            windows[num].end_bus
        );
        num++;
    }
    n_windows = num;

    log_inits("pci_init");
}