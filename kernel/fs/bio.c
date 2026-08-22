#include <stdint.h>
#include <kernel/bio.h>
#include <kernel/nvme.h>
#include <kernel/spin_lock.h>
#include <kernel/defs.h>
#include <kernel/sleep_lock.h>
#include <kernel/kalloc.h>
#include <kernel/debug.h>

struct {
    // This whole table lock protects info about which blocks are cached
    struct spin_lock lk;
    struct buf bufs[N_BUFS];

    struct buf head;
} bcache;
extern struct nvme_controller first_nvme;


void b_init(void) {
    init_lock(&bcache.lk, "bcache");

    // DLL
    // Make an empty list
    bcache.head.next = &bcache.head;
    bcache.head.prev = &bcache.head;

    for(int i=0; i<N_BUFS; i++) {
        struct buf *b = &bcache.bufs[i];

        // Insert this buf, b at the next of the head
        // head <-> head.ori_next
        //     \    /
        //       b
        b->next = bcache.head.next;
        b->prev = &bcache.head;

        // Cut the original connection
        // head - b - head.ori_next
        bcache.head.next->prev = b;
        bcache.head.next = b;

        init_sleep_lock(&b->lock, "buffer");
        b->dma_buf = kalloc();
        if(!b->dma_buf) {
            panic("binit: kalloc failed");
        }
        b->dev = &first_nvme;
    }
    log_inits("b_init");
}

// Buffer caches has two jobs
// 1. Sync access to disk blocks to ensure only one kthread at a time uses that block
// 2. Cache blocks, gotta go fast
// Syncing is happening in this bget
// All access to a certain block must be done through bcache
// block read? use bread. block write? use bwrite
// and those funcs need buf pointer; you must earn the one and only buf pointer reference via bget
// If someone has taken the block buf (some thread has the buf pointer), you must sleep until they wake you
// bread knows this and automatically calls the bget for you
// bread is just a mere wrapper for bget
// Actually, every other operations that uses struct buf pointer must check that you have the legal possesion of it (holding the sleep lock)
static struct buf* bget(uint64_t block_no) {
    acquire(&bcache.lk);

    // Does some buf cache have the block content?
    for(struct buf *b = bcache.head.next; b != &bcache.head; b = b->next) {
        if(b->dev != &first_nvme || b->block_no != block_no) continue;
        // Someone is accessing the block
        // You fisrt express that someone is waiting by ref_count++
        // and you acquire the lock or sleep until you can
        b->ref_count++;
        release(&bcache.lk);
        acquire_sleep(&b->lock); 
        return b;
    }

    // It is not on the cache
    // look for eviction
    for(struct buf *b = bcache.head.prev; b != &bcache.head; b = b->prev) {
        // There is some thread that using or wanting this block, so you can't just evict it
        if(b->ref_count != 0 || (b->flags & B_DIRTY)) continue;

        // At now we evict this block from the cache, now b is our block
        b->dev = &first_nvme;
        b->block_no = block_no;
        b->flags = 0;
        b->ref_count = 1;
        release(&bcache.lk); 

        acquire_sleep(&b->lock);
        return b;
    }
    panic("bget: cannot evict");
}

// It calls the bget for you before reading it
// btw, this is the only way to get the buf pointer actually
// What would you do with an empty buf's pointer anyway? write something blind?
struct buf* bread(uint64_t block_no) {
    struct buf *b = bget(block_no);
    if((b->flags & B_VALID) == 0) {
        nvme_rw(b);
    }
    return b;
}

// Set DIRTY bit and sync the disk with cache
void bwrite(struct buf *b) {
    if(!holding_sleep(&b->lock)) 
        panic("bwrite: illegal possesion of pointer");
    b->flags |= B_DIRTY;
    nvme_rw(b);
}

// release a locked buffer and move it to the MRU
// actually it is bput and bunlock
void brelse(struct buf *b) {
    if(!holding_sleep(&b->lock))
        panic("brelse: illegal possesion of pointer");
    release_sleep(&b->lock);

    acquire(&bcache.lk);
    b->ref_count--;

    if(b->ref_count == 0) {
        // no one is waiting for it

        // Pop the b
        b->next->prev = b->prev;
        b->prev->next = b->next;

        // Insert the b
        b->next = bcache.head.next;
        b->prev = &bcache.head;

        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
    release(&bcache.lk);
}

// test thread
void test_bcache(uint64_t arg) {
    struct buf *b = bread(arg);
    if((b->flags & B_VALID) == 0) 
        panic("test_bcache: no valid bit on buf");

    uint8_t *for_write = b->dma_buf;
    for_write[0] = 0xAB;
    for_write[1] = 0xCD;
    bwrite(b);
    brelse(b);
    
    // I will evict some bufs
    struct buf *bufs[N_BUFS];
    for(int i=0; i<N_BUFS; i++) {
        bufs[i] = bread(i+10);
    }
    for(int i=0; i<N_BUFS; i++) {
        brelse(bufs[i]);
    }

    struct buf *b2 = bread(arg);
    uint8_t *for_read = b2->dma_buf;
    if((for_read[0] != 0xAB) || (for_read[1] != 0xCD)) {
        panic("test_bcache: reread failed");
    }

    brelse(b2);
    for(;;);
}