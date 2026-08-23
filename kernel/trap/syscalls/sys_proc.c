#include <stdint.h>
#include <kernel/proc.h>

int64_t sys_fork(void) {
    return fork();
}

int64_t sys_wait(void) {
    return wait();
}

int64_t sys_exit(void) {
    exit();
    return 0; // no reach
}

int64_t sys_sbrk(void) {
    int64_t n = myproc()->tf->gprs.rdi;

    uint64_t addr = myproc()->sz;
    if(grow_proc(n) < 0) return -1;
    return addr;
}