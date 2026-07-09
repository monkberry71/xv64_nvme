#include <stdint.h>
#include <kernel/defs.h>
#include <kernel/proc.h>
#include <kernel/spin_lock.h>
#include <kernel/debug.h>
#include <kernel/gop.h>
#include <kernel/kalloc.h>
#include <kernel/string.h>

struct {
    struct spin_lock lk;
    struct proc procs[N_PROCS];
} proc_table;

void process_init(void) {
    init_lock(&proc_table.lk, "ptable");
    log_inits("process_init");
}

uint64_t next_pid = 1;

struct proc* alloc_kthread(void) {
    acquire(&proc_table.lk);
    for(int i=0; i<N_PROCS; i++) {
        struct proc *p = &proc_table.procs[i];
        if(p->state != UNUSED) continue;

        p->state = EMBRYO;
        p->pid = next_pid++;

        // p is selected and not UNUSED, no one will edit the p
        release(&proc_table.lk);

        void* new_stack = kalloc();
        if(!new_stack) {
            p->state = UNUSED;
            return 0;
        }
        p->kstack = (uint64_t) new_stack;
        return p;


    }
    release(&proc_table.lk);
    return 0; // failed
}

void make_kthread(void* thread_func) {
    struct proc *p = alloc_kthread();

    if(!p) {
        panic("make_kthread: alloc_kthread failed");
    }
    
    extern pte_t* kpml4;
    p->pml4 = kpml4;

    uint64_t sp = p->kstack + KERNEL_STACK_SIZE;
    sp -= sizeof(struct context);
    p->context = (void*) sp;
    memset(p->context, 0, sizeof(struct context));
    p->context->rip = (uint64_t) thread_func;

    acquire(&proc_table.lk);
    p->state = RUNNABLE;
    release(&proc_table.lk);
}

void proc_a(void) {
    for(;;) {
        gop_draw_rect(300,300,50,50, 0);
        gop_draw_rect(300,300,50,50, GOP_BLU);
        swtch(&proc_table.procs[0].context,proc_table.procs[1].context); 
    }
}

void proc_b(void) {
    for(;;) {
        gop_draw_rect(300,300,50,50, 0);
        gop_draw_rect(300,300,50,50, GOP_GRN);
        swtch(&proc_table.procs[1].context,proc_table.procs[2].context); 
    }
}

void proc_c(void) {
    for(;;) {
        gop_draw_rect(300,300,50,50, 0);
        gop_draw_rect(300,300,50,50, GOP_RED);
        swtch(&proc_table.procs[2].context,proc_table.procs[0].context); 
    }
}

void test_swtch(void) {
    make_kthread(proc_a);
    make_kthread(proc_b);
    make_kthread(proc_c);

    struct context *no_use;
    swtch(&no_use, (proc_table.procs[0]).context);
}