#include <stdint.h>
#include <kernel/mb2.h>
#include <kernel/mem_layout.h>
#include <kernel/mmu.h>
#include <kernel/bump.h>
#include <kernel/string.h>

struct mb2_info* reserved_mb2_info;

// Move the mb2 info struct to our bump allocated region, we may lost it
void preserve_mb2(void* mb2_info_phys) {
    struct mb2_info *mb2_info_v = P2V_KERN(mb2_info_phys);
    reserved_mb2_info = bump_alloc();
    uint32_t mb2_total_size = mb2_info_v->total_size;
    for(uint32_t alloc = PGSIZE_4KB; alloc < mb2_total_size; alloc += PGSIZE_4KB) {
        bump_alloc();
    }

    memcpy(reserved_mb2_info, mb2_info_v, mb2_total_size);
}

// mb2 iterator for iterating tags
void mb2_it_init(struct mb2_it *it, const struct mb2_info *info) {
    it->curr = (void*)info->tags; // (void*) casting for const evade
    it->end = 0;
}

void mb2_it_next(struct mb2_it *it) {
    uint64_t curr_tag_addr = (uint64_t) it->curr;
    uint64_t next_tag_addr = ROUND_UP(curr_tag_addr + it->curr->size, 8);
    it->curr = (void*)next_tag_addr;
    if(it->curr->type == 0) {
        // terminator tag 0
        it->end = 1;
    }
}