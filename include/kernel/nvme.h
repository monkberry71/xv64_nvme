#pragma once
#include <stdint.h>
#include <kernel/pci.h>
#include <kernel/spin_lock.h>

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_NVM 0x08
#define PCI_PROG_IF_NVME 0x02

#define NVME_REG_CAP 0x0
#define NVME_REG_VS 0x8
#define NVME_REG_CC 0x14
#define NVME_REG_CSTS 0x1c
#define NVME_REG_AQA 0x24
#define NVME_REG_ASQ 0x28
#define NVME_REG_ACQ 0x30

#define NVME_ADMIN_Q_DEPTH 64
#define NVME_IO_Q_DEPTH 64

#define NVME_ADMIN_OPCODE_CREATE_IO_SQ 0x01
#define NVME_ADMIN_OPCODE_CREATE_IO_CQ 0x05

#define NVME_IO_OPCODE_W 0x01
#define NVME_IO_OPCODE_R 0x02

struct nvme_queue {
    void* addr;
    uint64_t p_addr;
    uint64_t size;
    uint32_t head;
    uint32_t tail;
    volatile uint32_t *doorbell;
    struct spin_lock lk;
};

struct nvme_queue_pair {
    struct nvme_queue sq;
    struct nvme_queue cq;
    uint8_t expected_phase_bit;
    int16_t qp_id;
};

struct nvme_namespace {
    int32_t ns_id;
    uint64_t size; // number of logical blocks
    uint64_t capacity; // number of usable blocks
    uint32_t lba_size; // block byte size
};

struct buf;
struct nvme_block_req {
    int used;
    int done;
    int status;
    struct buf *buf;
};

// Cache Useful infos
struct nvme_controller {
    struct pci_func pci_func;
    void* bar0_reg;
    struct nvme_queue_pair admin_queue;
    struct nvme_queue_pair io_queue;
    struct nvme_namespace ns1;

    struct nvme_block_req reqs[NVME_IO_Q_DEPTH];
    struct spin_lock req_lk;

    uint16_t max_queue_entries;
    uint8_t cqr;
    uint8_t ams;
    uint8_t timeout;
    uint32_t doorbell_stride;
    // rest CAP fields are not very useful now...
};

struct nvme_sq_entry {
    uint8_t opcode;
    uint8_t flags;
    uint16_t cmd_id;

    uint32_t ns_id;
    uint64_t reserved;
    uint64_t metadata_pointer;

    uint64_t prp1;
    uint64_t prp2;

    // Command specific dwords
    uint32_t cdw10[6];
} __attribute__((packed));

struct nvme_cq_entry {
    uint32_t cdw0;
    uint32_t reserved;

    uint16_t sq_head;
    uint16_t sq_id;

    uint16_t cmd_id;
    uint16_t status; // phase bit here
} __attribute__((packed));

struct nvme_lbaf {
    uint16_t ms;
    uint8_t lbads;
    uint8_t rp;
} __attribute__((packed));



void nvme_init(void);
void nvme_rw(struct buf *b);
void nvme_intr(void);