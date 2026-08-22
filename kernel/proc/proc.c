#include <stdint.h>
#include <kernel/defs.h>
#include <kernel/proc.h>
#include <kernel/spin_lock.h>
#include <kernel/debug.h>
#include <kernel/gop.h>
#include <kernel/kalloc.h>
#include <kernel/string.h>
#include <kernel/mem_layout.h>
#include <kernel/uart.h>
#include <kernel/sleep_lock.h>

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

struct proc* myproc(void) {
    push_cli();

    // In scheduler, proc is updated everytime
    struct proc *p = mycpu()->proc;

    pop_cli();
    return p;
}

// Basically a wrapper for swtch, but..
// 1. Check conditions before entering the scheduler
// 2. push to stack the field that resides cpu struct but actually belongs to a proc
void sched(void) {
    struct proc *p = myproc();

    // A process who wants to give up the cpu must:
    // 1. acquire proc_table lock, scheduler works in only proc_table lock enabled
    // 2. release any other lock it is holding
    // 3. update its own state
    // 4. Then, call sched

    // 1. Acquire proc_table.lk
    if(!holding(&proc_table.lk)) {
        panic("sched: proc_table.lk not acquired");
    }

    // 2. Release any other lock its holding
    // push_cli is called in only 3 places across the whole xv6
    // #1 myproc #2 switch_uvm #3 spin_lock #3.1 holding
    // #1 #2 #3.1 would not call yield-sched, thus cli_n == lock_n
    if(mycpu()->cli_n != 1) {
        panic("sched: sleep with lock acquired");
    }

    // 3. update its own state
    if(p->state == RUNNING) {
        panic("sched: state is not updated");
    }

    if(read_rflags() & RFLAGS_IF) {
        // and since a proc_table lock is held, intr must be disabled
        // just in case, check it
        panic("sched: interrupt enabled");
    }

    // Interrupt nesting state logically belongs to the thread or process,
    // proc->was_intr_enabled and proc->cli_n would be more appropriate
    // However, the scheduler thread doesn't have a proc struct.
    // so xv6 stores it in the cpu struct and swaps the value across sched()
    // cli_n should be always 1 in this func, so save only the was_intr_enabled
    // It is saved to the kernel stack because a local variable would be.
    uint64_t was_intr_enabled = mycpu()->was_intr_enabled;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->was_intr_enabled = was_intr_enabled;
}

// Basically a wrapper of sched, which is a wrapper of swtch....
// It actually do the work that sched checks
void yield(void) {
    acquire(&proc_table.lk);
    myproc()->state = RUNNABLE;
    sched();
    release(&proc_table.lk);
}

void sleep(void* chan, struct spin_lock *lk) {
    struct proc *p = myproc();

    if(p == 0) 
        panic("sleep: in scheduler");

    // CV pattern : An inner spinlock must protect 
    if(lk == 0) 
        panic("sleep: sleep condition was not proctected by a lock");
    
    // 0. We have acquired the sleep condition protection lock
    // 1. We must acquire ptable.lock in order to change a proc state
    // 2. Once we hold the ptable.lock, we won't miss any wakeup (wakeup acquires ptable.lock)
    // 3. After acquiring the ptable.lock, it is ok to release the spinlock
    if(lk != &proc_table.lk) {
        acquire(&proc_table.lk);
        release(lk);
    }

    // Sleep
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Clean the channel 
    p->chan = 0;

    // We must release ptable.lk first.
    // Up above, we acquired the locks in lk -> ptable.lock order
    // If we try to acquire lk before releasing the ptable.lock,
    // We are trying the order ptable.lock -> lk
    // Which obviously means a deadlock.
    if(lk != &proc_table.lk) {
        release(&proc_table.lk);
        acquire(lk);
    }
}

// Clean the SLEEPING state of certain-channel procs
// Pure version, doesn't acquire any locks
static void wakeup_pure(void *chan) {
    for(int i=0; i<N_PROCS; i++) {
        struct proc *p = &proc_table.procs[i];

        if(p->chan == chan && p->state == SLEEPING) 
            p->state = RUNNABLE;
    }
}

// Wrapper for wakeup_pure
// It acquires ptable.lock
void wakeup(void* chan) {
    acquire(&proc_table.lk);
    wakeup_pure(chan);
    release(&proc_table.lk);
}

// no return
void scheduler(void) {
    struct cpu *c = mycpu();
    c->proc = 0; // proc equals to 0 means the scheduler

    for(;;) {
        // enable intr
        // 1. Timer intr will be ignored in this scheduler thread, check intr().
        // 2. If there is no RUNNABLE process, all sleeping,
        //    intr would be disabled forever and no one will be awaken
        //    Thus, enable intr for a small amount of time
        // 3. In this stub, proc_table lock is not acquired,
        //    so wakeup can edit the proc table and make some RUNNABLE process
        sti();

        acquire(&proc_table.lk);
        for(int i=0; i<N_PROCS; i++) {
            struct proc *p = &proc_table.procs[i];
            if(p->state != RUNNABLE) continue;

            c->proc = p;
            // switch_uvm
            p->state = RUNNING;
            swtch(&(c->scheduler), p->context);
            c->proc = 0;
        }
        release(&proc_table.lk);
    }
}

// It is a first code stub that a fresh thread will execute on first time
// If a thread was executed once, it will continue from sched and return to yield
// and release the proc_table lock. 
// However, if a thread has not executed once, this code will release the lock for us
// In original xv6, it is called fork_ret because every process except init will be created by fork
// We have a pure kernel thread, so let's name it slightly different
void first_ret(void) {
    static int run_first_init = 1;
    release(&proc_table.lk);

    if(run_first_init) {
        // Some init funcs must be run in the context of a process, not main()
        // (e.g., they call sleep or smth)
        // So run the init funcs here
        run_first_init = 0;
        // init funcs
    }

    // return to the caller, which is intr_ret if kthread or syscall_ret if user process, 
}

void intr_ret();
void make_kthread(kthread_t kthread_func, uint64_t kthread_arg) {
    struct proc *p = alloc_kthread();

    if(!p) {
        panic("make_kthread: alloc_kthread failed");
    }
    
    extern pte_t *kpml4;
    // kpml4 is a kernel mapped address, we need to change it to 
    // a direct mapped address.
    uint64_t p_kpml4 = V2P_KERN(kpml4);
    pte_t *dm_kpml4 = P2V_DIR(p_kpml4);
    p->pml4 = dm_kpml4;

    uint64_t sp = p->kstack + KERNEL_STACK_SIZE;

    // We need to make a trap frame to iretq,
    // Because we want to turn on the intr flag smoothly
    sp -= sizeof(struct trap_frame);
    p->tf = (void*) sp;
    memset(p->tf, 0, sizeof(struct trap_frame));
    p->tf->rip = (uint64_t) kthread_func;
    p->tf->cs = (SEG_KCODE << 3);
    p->tf->rflags = RFLAGS_IF;
    p->tf->rsp = (uint64_t)(p->kstack + KERNEL_STACK_SIZE);
    p->tf->ss = (SEG_KDATA << 3);
    p->tf->gprs.rdi = kthread_arg;

    sp -= 8;
    *(uint64_t*)sp = (uint64_t) intr_ret;

    sp -= sizeof(struct context);
    p->context = (void*) sp;
    memset(p->context, 0, sizeof(struct context));
    p->context->rip = (uint64_t) first_ret;

    // stack -->
    // struct context and the last field rip = first_ret | intr_ret | struct trap_frame

    acquire(&proc_table.lk);
    p->state = RUNNABLE;
    release(&proc_table.lk);
}

void proc_draw(uint64_t arg) {
    uint64_t proc_y;
    switch(arg) {
        case GOP_BLU:
            proc_y = 100;
            break;
        case GOP_RED:
            proc_y = 150;
            break;
        case GOP_GRN:
            proc_y = 200;
            break;
        default:
            proc_y = 50;
    }
    for(;;) {
        for(int i=0; i<100; i++) {
            gop_draw_rect(100, proc_y, 50+i, 50, arg);
        }

        gop_draw_rect(100, proc_y, 450, 50, GOP_BLK);
    }
}

void init_test_sleeplock(void);
void test_sleep_lock_kthread(uint64_t);
void test_nvme_rw(uint64_t);
void test_bcache(uint64_t);
void test_scheduler(void) {
    make_kthread(proc_draw, GOP_BLU);
    make_kthread(proc_draw, GOP_RED);
    make_kthread(proc_draw, GOP_GRN);

    // init_test_sleeplock();
    // make_kthread(test_sleep_lock_kthread, 0);
    // make_kthread(test_sleep_lock_kthread, 1);
    // make_kthread(test_sleep_lock_kthread, 2);
    make_kthread(test_nvme_rw, 0);
    make_kthread(test_bcache, 3);

    scheduler();
}
