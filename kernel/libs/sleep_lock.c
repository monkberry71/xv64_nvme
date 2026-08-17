#include <stdint.h>
#include <kernel/sleep_lock.h>
#include <kernel/spin_lock.h>
#include <kernel/proc.h>
#include <kernel/uart.h>

void init_sleep_lock(struct sleep_lock *lk, char *name) {
    init_lock(&lk->inner_lk, "sleeplock CV inner lock");
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
}

void acquire_sleep(struct sleep_lock *lk) {
    // CV pattern
    acquire(&lk->inner_lk);
    while(lk->locked) {
        sleep(lk, &lk->inner_lk);
    }

    lk->locked = 1;
    lk->pid = myproc()->pid;
    release(&lk->inner_lk);
}

void release_sleep(struct sleep_lock *lk) {
    acquire(&lk->inner_lk);
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk);
    release(&lk->inner_lk);
}

int holding_sleep(struct sleep_lock *lk) {
    acquire(&lk->inner_lk);
    int r = lk->locked && (lk->pid == myproc()->pid);
    release(&lk->inner_lk);
    return r;
}

// TEST

#define N_SLEEPLOCK_TEST_ITERS 500

static struct sleep_lock test_lock;
static int test_counter = 0;

void init_test_sleeplock(void) {
    init_sleep_lock(&test_lock, "test_lock");
}

void test_sleep_lock_kthread(uint64_t arg) {
    for(int i = 0; i < N_SLEEPLOCK_TEST_ITERS; i++) {
        acquire_sleep(&test_lock);
        test_counter++;
        serial_printf("[sleeplock test] thread %d: counter=%d\n", (int)arg, test_counter);
        release_sleep(&test_lock);
        for(int i=0; i<4000; i++);
    }

    serial_printf("[sleeplock test] thread %d: done\n", (int)arg);
    for(;;);
}
