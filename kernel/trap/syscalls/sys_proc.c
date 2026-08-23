#include <stdint.h>
#include <kernel/proc.h>

int64_t sys_fork(void) {
    return fork();
}

int64_t sys_wait(void) {
    return wait();
}