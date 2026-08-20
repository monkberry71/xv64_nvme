#include <stdint.h>
#include <kernel/nvme.h>
#include <kernel/pci.h>
#include <kernel/debug.h>
#include <kernel/uart.h>
#include <kernel/kvm.h>
#include <kernel/kalloc.h>
#include <kernel/string.h>
#include <kernel/mmu.h>
#include <kernel/mem_layout.h>

// https://wiki.osdev.org/NVMe

static struct nvme_controller first_nvme;

static uint32_t nvme_read32(struct nvme_controller *c, uint32_t off) {
    return *(volatile uint32_t*) ((uint8_t *)c->bar0_reg + off);
}

static uint64_t nvme_read64(struct nvme_controller *c, uint32_t off) {
    uint32_t lo = nvme_read32(c, off);
    uint32_t hi = nvme_read32(c, off + 4);
    return ((uint64_t) hi << 32) | lo;
}

static void nvme_write32(struct nvme_controller *c, uint32_t off, uint32_t val) {
    *(volatile uint32_t *)((uint8_t*)c->bar0_reg + off) = val;
}

static void nvme_write64(struct nvme_controller *c, uint32_t off, uint64_t val) {
    nvme_write32(c, off, val & 0xFFFFFFFF);
    nvme_write32(c, off + 4, val >> 32);
}

static void nvme_cache_cap(struct nvme_controller *c) {
    uint64_t cap = nvme_read64(c, NVME_REG_CAP);

    c->max_queue_entries = (cap & 0xFFFF) + 1;
    c->cqr = (cap >> 16) & BIT(0);
    c->ams = (cap >> 17) & (BIT(0) | BIT(1));
    c->timeout = (cap >> 24) & 0xFF;
    c->doorbell_stride = (cap >> 32) & 0xF;

    c->doorbell_stride = (4 << c->doorbell_stride);
}

static void nvme_enable(struct nvme_controller *c) {
    uint32_t cc = 0;

    // Set the controller config
    // We making it 0x00460001, which was an original value
    cc |= BIT(0); // EN = 1 : enable 
    cc |= (0 << 4); // CSS NVM(0) 
    cc |= (0 << 7); // MPS 2^(12+0) byte
    cc |= (0 << 11); // AMS = round robin(0)
    cc |= (6 << 16); // IOSQES = sq entry size 2^6 byte
    cc |= (4 << 20); // IOCQES = cq entry size 2^4 byte
    
    nvme_write32(c, NVME_REG_CC, cc);
    while(( nvme_read32(c, NVME_REG_CSTS) & BIT(0)) == 0);
}

static void nvme_disable(struct nvme_controller *c) {
    uint32_t cc = nvme_read32(c, NVME_REG_CC);
    // CC.EN = 0
    cc &= ~BIT(0);
    nvme_write32(c, NVME_REG_CC, cc);

    // wait until CSTS.RDY == 0
    while(nvme_read32(c, NVME_REG_CSTS) & BIT(0));
}

// Just init struct itself, not actually register it to the controller
static void init_nvme_queue_pair(struct nvme_queue_pair *qp, int depth) {
    qp->sq.addr = kalloc();
    if(!qp->sq.addr) {
        panic("init_nvme_queue_pair: kalloc failed");
    }
    memset(qp->sq.addr, 0, PGSIZE_4KB);
    qp->sq.size = depth;
    qp->sq.p_addr = V2P_DIR(qp->sq.addr);

    qp->cq.addr = kalloc();
    if(!qp->cq.addr) {
        panic("init_nvme_queue_pair: kalloc failed");
    }
    memset(qp->cq.addr, 0, PGSIZE_4KB);
    qp->cq.size = depth;
    qp->cq.p_addr = V2P_DIR(qp->cq.addr);

    qp->sq.head = 0;
    qp->sq.tail = 0;
    qp->cq.head = 0;
    qp->cq.tail = 0; 
    qp->expected_phase_bit = 1;
}

static void nvme_make_admin(struct nvme_controller *c, struct nvme_queue_pair *qp) {
    qp->qp_id = 0;

    uint32_t aqa = ((qp->cq.size - 1) << 16) | (qp->sq.size - 1);

    nvme_write32(c, NVME_REG_AQA, aqa);
    nvme_write64(c, NVME_REG_ASQ, qp->sq.p_addr);
    nvme_write64(c, NVME_REG_ACQ, qp->cq.p_addr);

    qp->sq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id) * c->doorbell_stride);
    qp->cq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id + 1) * c->doorbell_stride);
}

static int nvme_admin_submit(struct nvme_controller *c, struct nvme_sq_entry *cmd, struct nvme_cq_entry *out) {
    struct nvme_queue_pair *aqp = &c->admin_queue;

    uint16_t cid = aqp->sq.tail;
    cmd->cmd_id = cid;

    struct nvme_sq_entry *sq = aqp->sq.addr;
    sq[aqp->sq.tail] = *cmd; // Write and ..

    aqp->sq.tail++;
    aqp->sq.tail %= aqp->sq.size;
    *aqp->sq.doorbell = aqp->sq.tail; // Ring the bell

    struct nvme_cq_entry *cq = aqp->cq.addr;

    // Wait until cq head's phase bit becomes expected bit
    while((cq[aqp->cq.head].status & BIT(0)) != aqp->expected_phase_bit);

    struct nvme_cq_entry cpl = cq[aqp->cq.head]; // Read and ..
    if(out) *out = cpl;

    uint16_t status = cpl.status >> 1;

    aqp->cq.head++;
    aqp->cq.head %= aqp->cq.size;

    // wrap-around happend, change the expected phase bit
    if(aqp->cq.head == 0) {
        aqp->expected_phase_bit ^= 1;
    }
    *aqp->cq.doorbell = aqp->cq.head; // Ring the bell

    if (status) {
        serial_printf("nvme admin status=%x\n", status);
        return -1;
    }

    return 0;
}

static void nvme_parse_namespace(void *buf) {
    uint8_t *b = buf;

    uint64_t nsze = *(uint64_t *)(b + 0x00);
    uint64_t ncap = *(uint64_t *)(b + 0x08);
    uint64_t nuse = *(uint64_t *)(b + 0x10);

    uint8_t nlbaf = b[0x19] + 1;
    uint8_t flbas = b[0x1a];
    uint8_t format = flbas & 0x0f;

    struct nvme_lbaf *lbafs = (struct nvme_lbaf *)(b + 0x80);
    struct nvme_lbaf *lbaf = &lbafs[format];

    uint32_t lba_size = 1U << lbaf->lbads;

    serial_printf("NVME ns nsze=%p ncap=%p nuse=%p nlbaf=%d format=%d lbads=%d ms=%d lba_size=%d\n",
                nsze, ncap, nuse, nlbaf, format, lbaf->lbads, lbaf->ms, lba_size);
}

void nvme_init(void) {

    if(pci_find_class(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_NVM, PCI_PROG_IF_NVME, &first_nvme.pci_func) < 0) {
        panic("nvme_init: nvme not found\n");
    }

    serial_printf("NVME found %d:%d.%d vendor=%x device=%x\n", first_nvme.pci_func.bus, first_nvme.pci_func.dev, first_nvme.pci_func.func, first_nvme.pci_func.vendor_id, first_nvme.pci_func.device_id);
    serial_printf("NVME BAR0 base=%p io=%d 64=%d prefetch=%d\n", first_nvme.pci_func.bars[0].base, first_nvme.pci_func.bars[0].is_io, first_nvme.pci_func.bars[0].is_64, first_nvme.pci_func.bars[0].prefetchable);

    if(first_nvme.pci_func.bars[0].is_io) {
        panic("nvme_init: BAR0 is not MMIO\n");
    }

    if(first_nvme.pci_func.bars[0].base ==0) {
        panic("nvme_init: BAR0 is zero\n");
    }

    pci_enable_device(&first_nvme.pci_func);

    first_nvme.bar0_reg = io_remap(first_nvme.pci_func.bars[0].base, 0x4000); // QEMU NVME regs are 16KB sized
    
    uint64_t cap = nvme_read64(&first_nvme, NVME_REG_CAP);
    uint32_t vs = nvme_read32(&first_nvme, NVME_REG_VS);
    uint32_t cc = nvme_read32(&first_nvme, NVME_REG_CC);
    uint32_t csts = nvme_read32(&first_nvme, NVME_REG_CSTS);
    
    serial_printf("NVME CAP=%p VS=%x CC=%x CSTS=%x\n", cap, vs, cc, csts);
    nvme_cache_cap(&first_nvme);
    
    nvme_disable(&first_nvme);

    // Making an Admin Queue Pair
    init_nvme_queue_pair(&first_nvme.admin_queue, NVME_ADMIN_Q_DEPTH);
    nvme_make_admin(&first_nvme, &first_nvme.admin_queue);

    nvme_enable(&first_nvme);

    serial_printf("NVME after enable CC=%x CSTS=%x AQA=%x ASQ=%p ACQ=%p\n",
            nvme_read32(&first_nvme, NVME_REG_CC),
            nvme_read32(&first_nvme, NVME_REG_CSTS),
            nvme_read32(&first_nvme, NVME_REG_AQA),
            nvme_read64(&first_nvme, NVME_REG_ASQ),
            nvme_read64(&first_nvme, NVME_REG_ACQ));


    // Send first identify controller command

    void *id = kalloc();
    if(!id) {
        panic("nvme_init: id buffer can't be alloc");
    }
    memset(id, 0, PGSIZE_4KB);

    struct nvme_sq_entry idc; // identify command
    memset(&idc, 0, sizeof(idc));

    idc.opcode = 0x06;
    idc.ns_id = 0;
    idc.prp1 = V2P_DIR(id);
    idc.cdw10[0] = 1;

    struct nvme_cq_entry res;
    if(nvme_admin_submit(&first_nvme, &idc, &res) < 0) {
        panic("nvme_init: identify controller failed");
    }

    uint16_t vid = *(uint16_t *)((uint8_t *)id + 0);
    uint16_t ssvid = *(uint16_t *)((uint8_t *)id + 2);
    uint8_t mdts = *((uint8_t *)id + 0x4d);

    serial_printf("NVME identify vid=%x ssvid=%x mdts=%d\n", vid, ssvid, mdts);

    // Let's pray that there be a namespace marked 1...
    
    idc.opcode = 0x06;
    idc.ns_id = 1;
    idc.prp1 = V2P_DIR(id);
    idc.cdw10[0] = 0;

    if(nvme_admin_submit(&first_nvme, &idc, &res) < 0) {
        panic("nvme_init: identify controller failed");
    }

    nvme_parse_namespace(id);

    log_inits("nvme_init");
}