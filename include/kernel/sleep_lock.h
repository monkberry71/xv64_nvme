#include <stdint.h>
#include <kernel/spin_lock.h>

struct sleep_lock {
    unsigned locked; // Is the lock held?
    struct spin_lock inner_lk; // Spinlock that protects the to-go condition

    char *name; // name of this lock
    uint64_t pid; // process that holds this lock
};

void init_sleep_lock(struct sleep_lock *lk, char *name);
void acquire_sleep(struct sleep_lock *lk);
void release_sleep(struct sleep_lock *lk);
int holding_sleep(struct sleep_lock *lk);