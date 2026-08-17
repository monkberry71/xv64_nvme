#pragma once
#include <stdint.h>
#include <kernel/mmu.h>

enum proc_state { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE};

struct context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
};

struct proc {
    uint64_t sz;
    pte_t *pml4;
    uint64_t kstack;
    enum proc_state state;
    uint64_t pid;
    struct proc *parent;
    struct trap_frame *tf;
    struct context *context;
    void* chan;
    int killed;
    // struct file *ofile[N_FILE];
    // struct inode *cwd;
    char name[32];
};

void swtch(struct context **, struct context *);
void make_kthread(void* thread_func, uint64_t kthread_arg);
void test_scheduler(void);
struct proc* myproc(void);
void yield(void);
void sleep(void* chan, struct spin_lock *lk);
void wakeup(void* chan);

void (*kthread) (uint64_t kthread_arg);