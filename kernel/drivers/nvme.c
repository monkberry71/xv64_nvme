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
#include <kernel/bio.h>

// https://wiki.osdev.org/NVMe

struct nvme_controller first_nvme;

static uint32_t nvme_read32(struct nvme_controller *c, uint32_t off);
static uint64_t nvme_read64(struct nvme_controller *c, uint32_t off);
static void nvme_write32(struct nvme_controller *c, uint32_t off, uint32_t val);
static void nvme_write64(struct nvme_controller *c, uint32_t off, uint64_t val);

static void nvme_cache_cap(struct nvme_controller *c);
static void nvme_enable(struct nvme_controller *c);
static void nvme_disable(struct nvme_controller *c);

static void init_nvme_queue_pair(struct nvme_queue_pair *qp, int depth);
static void nvme_make_qp_admin(struct nvme_controller *c, struct nvme_queue_pair *qp);
static void nvme_make_qp_io(struct nvme_controller *c, struct nvme_queue_pair *qp, uint16_t qp_id);

static int nvme_enqueue_cmd(struct nvme_queue_pair *qp, struct nvme_sq_entry *cmd);
static int nvme_cq_try_dequeue(struct nvme_queue_pair *qp, struct nvme_cq_entry *out);
static int nvme_admin_submit(struct nvme_controller *c, struct nvme_sq_entry *cmd, struct nvme_cq_entry *out);

static void nvme_identify_controller(struct nvme_controller *c);
static void nvme_identify_namespace(struct nvme_controller *c, int ns_id, struct nvme_namespace *out);

static int nvme_rw_block(struct nvme_controller *c, uint8_t opcode, uint64_t lba, void* buf);
static int nvme_read_block(struct nvme_controller *c, uint64_t lba, void* buf);
static int nvme_write_block(struct nvme_controller *c, uint64_t lba, void* buf);

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
    qp->qp_id = -1; // -1 not valid, 
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

    init_lock(&qp->sq.lk, "sq lk");
    init_lock(&qp->cq.lk, "cq lk");
}

static void nvme_make_qp_admin(struct nvme_controller *c, struct nvme_queue_pair *qp) {
    qp->qp_id = 0; // admin qp_id is always 0
    qp->sq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id) * c->doorbell_stride);
    qp->cq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id + 1) * c->doorbell_stride);

    uint32_t aqa = ((qp->cq.size - 1) << 16) | (qp->sq.size - 1);

    nvme_write32(c, NVME_REG_AQA, aqa);
    nvme_write64(c, NVME_REG_ASQ, qp->sq.p_addr);
    nvme_write64(c, NVME_REG_ACQ, qp->cq.p_addr);

}

static void nvme_make_qp_io(struct nvme_controller *c, struct nvme_queue_pair *qp, uint16_t qp_id) {
    qp->qp_id = qp_id;
    qp->sq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id) * c->doorbell_stride);
    qp->cq.doorbell = (void*) ((uint8_t *) c->bar0_reg + 0x1000 + (2 * qp->qp_id + 1) * c->doorbell_stride);

    struct nvme_sq_entry cmd;
    memset(&cmd, 0, sizeof(cmd));

    // Make cq first
    cmd.cmd_id = 1;
    cmd.opcode = NVME_ADMIN_OPCODE_CREATE_IO_CQ;
    cmd.prp1 = qp->cq.p_addr;
    cmd.cdw10[0] = qp->qp_id | ((qp->cq.size - 1) << 16);
    cmd.cdw10[1] = BIT(0);

    if(nvme_admin_submit(c, &cmd, 0) < 0) {
        panic("nvme_make_qp_io: create cq failed");
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_OPCODE_CREATE_IO_SQ;
    cmd.prp1 = qp->sq.p_addr;
    cmd.cdw10[0] = qp->qp_id | ((qp->sq.size - 1) << 16);
    cmd.cdw10[1] = (qp->qp_id << 16) | BIT(0);

    if(nvme_admin_submit(c, &cmd, 0) < 0) {
        panic("nvme_make_qp_io: create sq failed");
    }
}

// Push one cmd and ring the bell
static int nvme_enqueue_cmd(struct nvme_queue_pair *qp, struct nvme_sq_entry *cmd) {
    acquire(&qp->sq.lk);

    uint32_t next_tail = (qp->sq.tail + 1) % qp->sq.size;
    if(next_tail == qp->sq.head) {
        // full
        release(&qp->sq.lk);
        return -1;
    }
    
    // uint16_t cid = qp->sq.tail;
    // cmd->cmd_id = cid;
    struct nvme_sq_entry *sq = qp->sq.addr;
    sq[qp->sq.tail] = *cmd;
    qp->sq.tail = (qp->sq.tail + 1) % qp->sq.size;
    *qp->sq.doorbell = qp->sq.tail;

    release(&qp->sq.lk);

    return cmd->cmd_id;
}

// Try to dequeue the cq, returns the count of entries
static int nvme_cq_try_dequeue(struct nvme_queue_pair *qp, struct nvme_cq_entry *out) {
    acquire(&qp->cq.lk);

    volatile struct nvme_cq_entry *cq = qp->cq.addr;

    struct nvme_cq_entry cpl_coppied = cq[qp->cq.head];

    if((cpl_coppied.status & BIT(0)) != qp->expected_phase_bit) {
        release(&qp->cq.lk);
        return 0;
    }

    if(out) *out = cpl_coppied;

    qp->cq.head = (qp->cq.head + 1) % qp->cq.size;
    if(qp->cq.head == 0) {
        // phase bit wraparound
        qp->expected_phase_bit ^= 1;
    }

    *qp->cq.doorbell = qp->cq.head;
    
    release(&qp->cq.lk);

    acquire(&qp->sq.lk);
    qp->sq.head = cpl_coppied.sq_head; // update sq_head
    release(&qp->sq.lk);

    return 1;
}

// Use the helper, and spin wait the complete queue
static int nvme_admin_submit(struct nvme_controller *c, struct nvme_sq_entry *cmd, struct nvme_cq_entry *out) {
    struct nvme_queue_pair *aqp = &c->admin_queue;

    int cid = nvme_enqueue_cmd(aqp, cmd);
    if(cid < 0) return -1;

    struct nvme_cq_entry cpl;
    while(nvme_cq_try_dequeue(aqp, &cpl) == 0);

    if(out) *out = cpl;

    uint16_t status = cpl.status >> 1;

    if (status) {
        serial_printf("nvme admin status=%x\n", status);
        return -1;
    }

    return 0;
}

static void nvme_identify_controller(struct nvme_controller *c) {
    struct nvme_sq_entry idc; // identify command
    memset(&idc, 0, sizeof(idc));

    void* id = kalloc();
    if(!id) {
        panic("nvme_init: id buffer can't be alloc");
    }
    memset(id, 0, PGSIZE_4KB);

    idc.cmd_id = 1;
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
    
    kfree(id);
}

static void nvme_identify_namespace(struct nvme_controller *c, int ns_id, struct nvme_namespace *out) {

    struct nvme_sq_entry idc;
    memset(&idc, 0, sizeof(idc));

    void* id = kalloc();
    if(!id) {
        panic("nvme_identify_namespace: id buffer can't be alloc");
    }
    memset(id, 0, PGSIZE_4KB);

    idc.cmd_id = 1;
    idc.opcode = 0x06;
    idc.ns_id = ns_id;
    idc.prp1 = V2P_DIR(id);
    idc.cdw10[0] = 0;

    struct nvme_cq_entry res;

    if(nvme_admin_submit(c, &idc, &res) < 0) {
        panic("nvme_identify_namespace: identify controller failed");
    }

    uint8_t *b = id;

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

    if(out) {
        out->ns_id = ns_id;
        out->size = nsze;
        out->capacity = ncap;
        out->lba_size = lba_size;
    }

    kfree(id);
}

static int nvme_rw_block(struct nvme_controller *c, uint8_t opcode, uint64_t lba, void* buf) {
    struct nvme_sq_entry cmd;
    struct nvme_cq_entry cpl;
    memset(&cmd, 0, sizeof(cmd));
    memset(&cpl, 0, sizeof(cpl));

    struct nvme_queue_pair *qp = &c->io_queue;

    cmd.cmd_id = -1; // Avoid req_table id, since this is only for test rw 
    cmd.opcode = opcode;
    cmd.ns_id = c->ns1.ns_id;
    cmd.prp1 = V2P_DIR(buf);

    cmd.cdw10[0] = (uint32_t) lba;
    cmd.cdw10[1] = (uint32_t) (lba >> 32);
    cmd.cdw10[2] = 0; // length - 1

    if(nvme_enqueue_cmd(qp, &cmd) < 0) return -1;

    while(nvme_cq_try_dequeue(qp, &cpl) == 0);

    uint16_t status = cpl.status >> 1;
    if(status) {
        serial_printf("nvme rw status=%x\n", status);
        return -1;
    }

    return 0;
}

static int nvme_read_block(struct nvme_controller *c, uint64_t lba, void* buf) {
    return nvme_rw_block(c, NVME_IO_OPCODE_R, lba, buf);
}

static int nvme_write_block(struct nvme_controller *c, uint64_t lba, void* buf) {
    return nvme_rw_block(c, NVME_IO_OPCODE_W, lba, buf);
}

int nvme_req_alloc(struct nvme_controller *c, struct buf *b) {
    acquire(&c->req_lk);

    for(int i=0; i < NVME_IO_Q_DEPTH; i++) {
        if(c->reqs[i].used) continue;

        struct nvme_block_req *r = &c->reqs[i];

        r->used = 1;
        r->done = 0;
        r->status = 0;
        r->buf = b;
        release(&c->req_lk);
        return i;
    }

    // not found
    release(&c->req_lk);
    return -1;
}

void nvme_req_reap_cpl(struct nvme_controller *c, struct nvme_cq_entry *cpl) {
    acquire(&c->req_lk);

    struct nvme_block_req *r = &c->reqs[cpl->cmd_id];
    if(!r->used) {
        panic("nvme_complete_io: unused request");
    }

    r->status = cpl->status >> 1;

    if(r->status == 0 && r->buf) {
        r->buf->flags |= B_VALID; 
        r->buf->flags &= ~B_DIRTY;
    }

    r->done = 1;
    
    release(&c->req_lk);
}

void nvme_req_free(struct nvme_controller *c, int req_id) {
    struct nvme_block_req *r = &c->reqs[req_id];

    acquire(&c->req_lk);
    r->done = 0;
    r->status = 0;
    r->buf = 0;
    r->used = 0;
    release(&c->req_lk);
}

// Sync buf with cisk
void nvme_rw(struct buf *b) {
    if(!b) 
        panic("nvme_rw: no buf");
    if(!holding_sleep(&b->lock)) 
        panic("nvme_rw: buf not locked");

    struct nvme_controller *c = b->dev;
    
    struct nvme_sq_entry cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_id = nvme_req_alloc(c, b);
    if(cmd.cmd_id < 0) 
        panic("nvme_rw: no req slot");

    if(b->flags & B_DIRTY) {
        // need to write
        cmd.opcode = NVME_IO_OPCODE_W;
    } else if((b->flags & B_VALID) == 0) {
        // need to read
        cmd.opcode = NVME_IO_OPCODE_R;
    } else {
        // why did you call me in the first place
        panic("nvme_rw: nothing to do");
    }

    cmd.ns_id = c->ns1.ns_id;
    cmd.prp1 = V2P_DIR(b->dma_buf);
    cmd.cdw10[0] = (uint32_t) b->block_no;
    cmd.cdw10[1] = (uint32_t) (b->block_no >> 32);
    cmd.cdw10[2] = 0; // length - 1

    if(nvme_enqueue_cmd(&c->io_queue, &cmd) < 0)
        panic("nvme_rw: enqueue failed");
    
    struct nvme_cq_entry cpl;
    while(!c->reqs[cmd.cmd_id].done) {
        if(nvme_cq_try_dequeue(&c->io_queue, &cpl)) {
            nvme_req_reap_cpl(c, &cpl);
        }
    }

    if(c->reqs[cmd.cmd_id].status)
        panic("nvme_rw: cmd failed"); 

    nvme_req_free(c, cmd.cmd_id);
}

void nvme_init(void) {

    if(pci_find_class(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_NVM, PCI_PROG_IF_NVME, &first_nvme.pci_func) < 0) {
        panic("nvme_init: nvme not found\n");
    }

    serial_printf("PCI:NVME found %d:%d.%d vendor=%x device=%x\n", first_nvme.pci_func.bus, first_nvme.pci_func.dev, first_nvme.pci_func.func, first_nvme.pci_func.vendor_id, first_nvme.pci_func.device_id);
    serial_printf("PCI:NVME BAR0 base=%p io=%d 64=%d prefetch=%d\n", first_nvme.pci_func.bars[0].base, first_nvme.pci_func.bars[0].is_io, first_nvme.pci_func.bars[0].is_64, first_nvme.pci_func.bars[0].prefetchable);

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
    nvme_make_qp_admin(&first_nvme, &first_nvme.admin_queue);

    nvme_enable(&first_nvme);

    serial_printf("NVME after enable CC=%x CSTS=%x AQA=%x ASQ=%p ACQ=%p\n",
            nvme_read32(&first_nvme, NVME_REG_CC),
            nvme_read32(&first_nvme, NVME_REG_CSTS),
            nvme_read32(&first_nvme, NVME_REG_AQA),
            nvme_read64(&first_nvme, NVME_REG_ASQ),
            nvme_read64(&first_nvme, NVME_REG_ACQ));


    // Send first identify controller command
    nvme_identify_controller(&first_nvme);

    // Let's pray that there be a namespace marked 1...
    nvme_identify_namespace(&first_nvme, 1, &first_nvme.ns1);

    // Make a IO queue pair 1    
    init_nvme_queue_pair(&first_nvme.io_queue, NVME_IO_Q_DEPTH);
    nvme_make_qp_io(&first_nvme, &first_nvme.io_queue, 1);

    void* test_page = kalloc();
    if(!test_page) {
        panic("nvme_init: test_page alloc failed");
    }
    
    nvme_read_block(&first_nvme, 0, test_page);
    uint8_t *p = test_page;
    serial_printf("NVME read bytes: %s\n", p);

    memcpy(test_page, "REWRITE", 8);

    nvme_write_block(&first_nvme, 0, test_page);
    nvme_read_block(&first_nvme, 0, test_page);
    serial_printf("NVME read bytes: %s\n", p);

    // Req table init
    init_lock(&first_nvme.req_lk, "first_nvme req table lock");

    log_inits("nvme_init");
}

void test_nvme_rw(uint64_t arg) {
    struct buf b;
    init_sleep_lock(&b.lock, "nvme_rw test kthread's test buf sleep lock");
    b.flags = 0;
    b.dma_buf = kalloc();
    b.dev = &first_nvme;
    b.ref_count = 1;
    b.block_no = 1;

    acquire_sleep(&b.lock);
    nvme_rw(&b);
    serial_printf("%s\n", b.dma_buf);
    
    memcpy(b.dma_buf, "0123456789", 11);
    b.flags |= B_DIRTY;
    nvme_rw(&b);
    release_sleep(&b.lock);
    serial_printf("%s\n", b.dma_buf);
    for(;;);
}