#include <stdint.h>
#include <kernel/spin_lock.h>
#include <kernel/x86_64.h>
#include <kernel/debug.h>
#include <kernel/cpu.h>

// Push cli stack
void push_cli(void) {
    uint64_t rflags = read_rflags();
    // It may already been disabled when ncli == 0, for example, interrupt handlers automatically disabled it

    cli();

    struct cpu *c = mycpu();

    if(c->cli_n == 0) {
        // Block the intr for real and save prev state
        c->was_intr_enabled = rflags & RFLAGS_IF;
    }
    c->cli_n += 1;
}

// Pop cli stack
void pop_cli(void) {
    if(read_rflags() & RFLAGS_IF) {
        // already enabled
        panic("pop_cli: Interrupt is already enabled");
    }

    struct cpu *c = mycpu();

    c->cli_n -= 1;

    if(c->cli_n < 0) {
        panic("pop_cli: c->cli_n negative");
    }

    if(c->cli_n == 0 && c->was_intr_enabled) {
     // 1. no one wants to block int
        // 2. It was enabled previously
        // Then we can enable it
        sti();
    }
}

void init_lock(struct spin_lock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

uint64_t holding(struct spin_lock *lk) {
    push_cli();
    uint64_t r = lk->locked && (lk->cpu == mycpu());
    pop_cli();
    return r;
}

// void acquire(struct spin_lock *lk) {
//     serial_printf("acq enter cli_n=%d\n", mycpu()->cli_n);

//     push_cli();
//     serial_printf("acq after push cli_n=%d\n", mycpu()->cli_n);

//     if(holding(lk)) {
//         panic("acquire: deadlock");
//     }
//     serial_printf("acq after holding cli_n=%d\n", mycpu()->cli_n);

//     while(xchg(&lk->locked, 1) != 0);

//     __sync_synchronize();

//     lk->cpu = mycpu();
//     serial_printf("acq end cli_n=%d locked=%d\n", mycpu()->cli_n, lk->locked);
// }

void acquire(struct spin_lock *lk) {
    // If a lock is used by an intr handler, then the lock must not be used with intr enabled.
    // A thread holds the lock >> intr occurs >> intr handler on the same core tries to acquire the lock >> deadlock
    // so let's just assume every each lock is used by intr handler
    push_cli(); 

    if(holding(lk)) {
        panic("acquire: deadlock");
    }

    // Test and set, change lk->locked and 1 atomically and return the original lk->locked
    while(xchg(&lk->locked, 1) != 0);

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen after the lock is acquired.
    __sync_synchronize();

    lk->cpu = mycpu();

}

void release(struct spin_lock *lk) {
    if(!holding(lk)) {
        panic("release: nothing to release");
    }

    lk->cpu = 0;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other cores before the lock is released.
    // Both the C compiler and the hardware may re-order loads and
    // stores; __sync_synchronize() tells them both not to.
    __sync_synchronize();

    // It is equivalent to `lk->locked = 0`, but in C standard it might compile into non-atomic instructions (In x86_64, it is atomic)
    __asm__ volatile("movq $0, %0" : "=m"(lk->locked): );

    pop_cli();
}