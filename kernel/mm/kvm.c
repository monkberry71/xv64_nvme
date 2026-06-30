#include <stdint.h>
#include <kernel/kvm.h>
#include <kernel/bump.h>
#include <kernel/mmu.h>
#include <kernel/mem_layout.h>
#include <kernel/x86_64.h>
#include <kernel/debug.h>
#include <kernel/spin_lock.h>
#include <kernel/paging.h>

// We can divide the whole Virtual addr space into 2 equal pieces, user vs kernel
// so the pml4 of any process will have 0~255 entry as user space, and 256~511 as kernel space
// this functions manages the higher half kernel space

// We need direct mapping before making the kalloc, so we use bump alloc in this file except io_remap.
// io_remap uses map_pages, which uses kalloc

// static pte_t kpml4[512];
// This pml4 is for scheduler process or general kthreads
static pte_t *kpml4 = 0;

// Alloc the whole pml4 and set cr3 as the pml4
void kvm_alloc(void) {
    kpml4 = setup_kvm();
    switch_kvm();
}

// set cr3
void switch_kvm(void) {
    lcr3(V2P_KERN(kpml4));
}

static int insert_direct_map(pte_t *pml4);
static int insert_kernel_map(pte_t *pml4);
// It does two simple things
// 1. get page for kpml4
// 2. insert mapping on kpml4
pte_t* setup_kvm(void) {
    pte_t *pml4 = bump_alloc();
    // bump alloc just panics when an error occurs, so don't need to handle errors

    if(insert_direct_map(pml4) < 0 || insert_kernel_map(pml4) < 0) {
        panic("Can't setup kvm");
    }
    return pml4;
}

static int insert_direct_map(pte_t *pml4) {
    if(pml4[PML4_IDX(DIRECT_BASE)] != 0) {
        // Your pml4 is not clean
        return -1;
    }

    pte_t *pdpt = bump_alloc();
    pml4[PML4_IDX(DIRECT_BASE)] = V2P_KERN(pdpt) | DM_ENTRY_FLAGS;

    // I think max ram 128G is enough... PHY_STOP >> 30 is 128GB >> 30, 128GB / 1GB, 128
    for(int i=0; i<(PHY_STOP >> 30); i++) {
        uint64_t phy_addr = i * PGSIZE_1GB;
        pte_t pdpt_entry = phy_addr | PTE_PS | DM_ENTRY_FLAGS; // enable PS bit, it is going to be 1GB mapping
        
        // PDPT_IDX(0xFFFF 8880 0000 0000) == 0
        pdpt[PDPT_IDX(DIRECT_BASE) + i] = pdpt_entry;
    }
    return 0;
}

static int insert_kernel_map(pte_t *pml4) {
    if(pml4[PML4_IDX(KERN_BASE)] != 0) {
        // Your pml4 is not clean
        return -1;
    }

    pte_t *pdpt = bump_alloc();
    pml4[PML4_IDX(KERN_BASE)] = V2P_KERN(pdpt) | KM_ENTRY_FLAGS;

    // Kernel is only 2GB
    for(int i=0; i<2; i++) {
        uint64_t phy_addr = i * PGSIZE_1GB;
        pte_t pdpt_entry = phy_addr | PTE_PS | KM_ENTRY_FLAGS; // enable PS bit, it is going to be 1GB mapping
        
        // PDPT_IDX(0xFFFF 8880 0000 0000) == 0
        pdpt[PDPT_IDX(KERN_BASE) + i] = pdpt_entry;
    }
    return 0;
}

// make a mappage for mmio

static struct {
    struct spin_lock lk;
    uint64_t bump_line;
} io_remap_alloc;

void io_init(void) {
    init_lock(&io_remap_alloc.lk, "io_alloc_lk");
    io_remap_alloc.bump_line = IO_REMAP_BASE;
}

// Make a mapping for mmio, with a size
void* io_remap(uint64_t pa, uint64_t size) {

    if(size == 0) return 0;
    uint64_t pa_offset = PAGE_OFFSET(pa);

    acquire(&io_remap_alloc.lk);
    uint64_t va = io_remap_alloc.bump_line + pa_offset;
    map_pages(
        kpml4,
        (void*) va,
        size,
        ROUND_DOWN(pa, PGSIZE_4KB),
        PTE_W | PTE_G | PTE_PCD | PTE_PWT
    );
    
    io_remap_alloc.bump_line = ROUND_UP(va + size, PGSIZE_4KB);
    release(&io_remap_alloc.lk);

    return (void*) va;
}