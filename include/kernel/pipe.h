#pragma once
#include <stdint.h>
#include <kernel/file.h>
#include <kernel/spin_lock.h>

#define PIPE_SIZE 512

struct pipe {
    struct spin_lock lk;
    uint8_t data[PIPE_SIZE];
    uint64_t n_read;
    uint64_t n_write;
    int read_open; // read fd is still open
    int write_open; // write fd is still open
};

int64_t pipe_read(struct pipe *p, char *addr, uint64_t n);
int64_t pipe_write(struct pipe *p, char *addr, uint64_t n);
int pipe_alloc(struct file **f0, struct file **f1);
void pipe_close(struct pipe *p, int writable);