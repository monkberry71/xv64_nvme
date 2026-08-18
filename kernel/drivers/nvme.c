#include <stdint.h>
#include <kernel/nvme.h>
#include <kernel/pci.h>
#include <kernel/debug.h>
#include <kernel/uart.h>

void nvme_init(void) {
    struct pci_func f;

    if(pci_find_class(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_NVM, PCI_PROG_IF_NVME, &f) < 0) {
        panic("nvme_init: nvme not found\n");
    }

    serial_printf("NVME found %d:%d.%d vendor=%x device=%x\n", f.bus, f.dev, f.func, f.vendor_id, f.device_id);
    serial_printf("NVME BAR0 base=%p io=%d 64=%d prefetch=%d\n", f.bars[0].base, f.bars[0].is_io, f.bars[0].is_64, f.bars[0].prefetchable);

    if(f.bars[0].is_io) {
        panic("nvme_init: BAR0 is not MMIO\n");
    }

    if(f.bars[0].base ==0) {
        panic("nvme_init: BAR0 is zero\n");
    }
}