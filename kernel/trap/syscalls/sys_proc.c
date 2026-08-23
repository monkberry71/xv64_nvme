#include <stdint.h>
#include <kernel/proc.h>

int64_t sys_fork(void) {
    return fork();
}