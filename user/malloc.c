#include <stdint.h>
#include <user/user.h>

struct header {
    struct header *ptr;
    uint64_t unit_size;
};

static struct header base;
static struct header *free_p;

void free(void *ap) {
    struct header *bp = (struct header *)ap - 1;
    struct header *p;
    for(p = free_p; !(p < bp && bp < p->ptr); p = p->ptr) {
        if(p >= p->ptr && (p < bp || bp < p->ptr)) break;
        // p >= p->ptr means it is the end
        // (p < bp || bp < p->ptr) means our bp really belongs there
    }
    // p -- bp -- p_next

    struct header* p_next = p->ptr;
    if(bp + bp->unit_size == p_next) {
        // adjacent -> merge
        bp->unit_size += p_next->unit_size;
        bp->ptr = p_next->ptr;
    } else {
        // no merge, just link
        bp->ptr = p_next;
    }

    if(p + p->unit_size == bp) {
        // adja -> merge
        p->unit_size += bp->unit_size;
        p->ptr = bp->ptr;
    } else {
        // no merge, just link
        p->ptr = bp;
    }
    free_p = p;
}

static struct header* more_core(uint64_t nu) {
    if(nu < 4096) nu = 4096; // nah you will need more than that

    char *p = sbrk(nu * sizeof(struct header));
    if(p == (void*)-1) return 0;

    struct header* hp = p;
    hp->unit_size = nu;
    free(hp+1);
    return free_p;
}

void* malloc(uint64_t nbytes) {
    struct header *p;
    struct header *prev_p = free_p;
    uint64_t nunits = (nbytes + sizeof(struct header) - 1) / sizeof(struct header) + 1;
    // nunits = how many header will out-fit in the nbytes? + 1

    // first call if free_p was 0, create empty circular list
    // you can call it malloc_init maybe...?
    if(free_p == 0) {
        prev_p = &base;
        free_p = &base;
        base.ptr = &base;
        base.unit_size = 0;
    }

    for(p = free_p->ptr; ; prev_p = p, p = p->ptr) {
        
        // p seems to be good to go
        if(p->unit_size >= nunits) {
            if(p->unit_size == nunits) {
                prev_p->ptr = p->ptr;
                // original p goes completely out by prev_p->ptr is gone
            } else {
                // uint64_t header_jump_amount = p->unit_size - nunits;
                p->unit_size -= nunits;
                // p += header_jump_amount;
                p += p->unit_size;
                p->unit_size = nunits;
                // ori_p stays at prev_p->ptr, making the list valid still
            }
            free_p = prev_p;
            return (void*)(p+1);
        }

        if(p == free_p) {
            // no block found, grow the heap
            p = more_core(nunits);
            if(p == 0) return 0; 
        }
    }
}