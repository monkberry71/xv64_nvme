#pragma once
#include <stdint.h>
#include <kernel/x86_64.h>
#include <kernel/cpu.h>

struct spin_lock {
    uint64_t locked;

    // For debugging
    char *name;
    struct cpu *cpu;
};

void init_lock(struct spin_lock *lk, char *name);
void acquire(struct spin_lock *lk);
void release(struct spin_lock *lk);