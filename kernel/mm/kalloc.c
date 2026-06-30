#include <stdint.h>
#include <kernel/bump.h>
#include <kernel/mb2.h>
#include <kernel/mmu.h>
#include <kernel/debug.h>
#include <kernel/mem_layout.h>
#include <kernel/string.h>
#include <kernel/spin_lock.h>
#include <kernel/uart.h>

// The free list, think of it as a whole page, on which `struct run *next` written 
struct run {
    struct run *next;
};

struct {
    struct spin_lock lk;
    struct run *free_list;
} kmem;

// Add a page to the free list, the addr must be direct mapped page
void kfree(void* dm_va) {
    void* direct_mapped_bump_line = P2V_DIR(get_bump_line());
    if(
        (uint64_t) dm_va % PGSIZE_4KB || // is it a page addr?
        dm_va < direct_mapped_bump_line || // doesn't touch the bump alloced area?
        V2P_DIR(dm_va) >= PHY_STOP // tries to free somewhere higher then PHY_STOP?
    ) {
        serial_printf("%p", dm_va);
        if((uint64_t) dm_va % PGSIZE_4KB) {
            panic("kfree: non-aligned");
        }
        if(dm_va < direct_mapped_bump_line) {
            panic("kfree: freeing below the bump line");
        }
        if(V2P_DIR(dm_va) >= PHY_STOP) {
            panic("kfree: freeing past the PHY_STOP, 128GB");
        }

    }


    memset(dm_va, 1, PGSIZE_4KB); // fill 1
    
    acquire(&kmem.lk);
    struct run *r = dm_va;
    r->next = kmem.free_list;
    kmem.free_list = r;
    release(&kmem.lk);
}

// Free the whole range of some direct mapping range
// dm_start | free_start | freeing space | dm_end
void free_range(void* dm_start, void* dm_end) {
    uint8_t *free_start = (void*) ROUND_UP(dm_start, PGSIZE_4KB);
    for(uint8_t *p = free_start; p + PGSIZE_4KB <= (uint8_t*) dm_end; p+=PGSIZE_4KB) {
        kfree(p);
    }
}

void kalloc_init(void) {
    kmem.free_list = 0;
    init_lock(&kmem.lk, "kmem");
    
    extern const struct mb2_info *reserved_mb2_info;
    struct mb2_it it;
    mb2_it_init(&it, reserved_mb2_info);

    for(; !it.end; mb2_it_next(&it)) {
        if(it.curr->type != MB2_TAG_MMAP) continue;

        struct mb2_tag_mm *mm_tag = (void*) it.curr;
        uint64_t entry_n = (mm_tag->tag.size - sizeof(struct mb2_tag_mm)) / mm_tag->entry_size;
        for(uint64_t i=0; i<entry_n; i++) {
            struct mb2_mmap_entry *e = (void*) (((uint8_t*) mm_tag->entries) + i * mm_tag->entry_size);
            // *e = &mm_tag->entries[i]
            if(e->type != MB2_MMAP_RAM) continue;

            // mb2 mmap could give us memory space below the bump line
            // in which our kernel and bump alloced mem stay
            uint64_t p_bump_line = (uint64_t)get_bump_line();
            uint64_t start = e->base_addr < p_bump_line ? p_bump_line : e->base_addr;
            uint64_t end = e->base_addr + e->length;

            free_range(P2V_DIR(start), P2V_DIR(end)); 
        }
    }
    bump_disable(); // We made kalloc, no need to use the bump anymore.
}

void* kalloc(void) {
    acquire(&kmem.lk);
    struct run *r = kmem.free_list;
    if(r) kmem.free_list = r->next;
    release(&kmem.lk);
    return (void*)r;
}