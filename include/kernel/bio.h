#pragma once
#include <stdint.h>
#include <kernel/sleep_lock.h>
#include <kernel/helper.h>
struct nvme_controller;
struct buf {
    uint64_t flags; // VALID AND DIRTY
    struct nvme_controller* dev;
    uint64_t block_no;
    struct sleep_lock lock;
    uint32_t ref_count;
    
    // LRU Double linked list
    struct buf *prev;
    struct buf *next;

    // Data buffer
    void* dma_buf;
};

#define B_VALID BIT(1)
#define B_DIRTY BIT(2)

void b_init(void);
struct buf *bread(uint64_t block_no);
void bwrite(struct buf *b);
void brelse(struct buf *b);