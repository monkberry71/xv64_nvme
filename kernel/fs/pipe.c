#include <stdint.h>
#include <kernel/pipe.h>
#include <kernel/spin_lock.h>
#include <kernel/kalloc.h>
#include <kernel/file.h>
#include <kernel/proc.h>

int pipe_alloc(struct file **f0, struct file **f1) {
    struct pipe *p = 0;

    *f0 = *f1 = 0;

    *f0 = file_alloc();
    if(*f0 == 0) goto bad;

    *f1 = file_alloc();
    if(*f1 == 0) goto bad;

    p = kalloc();
    if(!p) goto bad;

    p->read_open = 1;
    p->write_open = 1;
    p->n_write = 0;
    p->n_read = 0;

    init_lock(&p->lk, "pipe");

    (*f0)->type = FD_PIPE;
    (*f0)->readable = 1;
    (*f0)->writable = 0;
    (*f0)->pipe = p;

    (*f1)->type = FD_PIPE;
    (*f1)->readable = 0;
    (*f1)->writable = 1;
    (*f1)->pipe = p;

    return 0;

    bad:
    if(p) kfree(p);
    if(*f0) file_close(*f0);
    if(*f1) file_close(*f1);

    return -1;
}

void pipe_close(struct pipe *p, int writable) {
    acquire(&p->lk);
    if(writable) {
        p->write_open = 0;
        wakeup(&p->n_read);
    } else {
        p->read_open = 0;
        wakeup(&p->n_write);
    }

    if(p->read_open == 0 && p->write_open == 0) {
        release(&p->lk);
        kfree(p);
    } else {
        release(&p->lk);
    }
}

int64_t pipe_write(struct pipe *p, char *addr, uint64_t n) {
    acquire(&p->lk);
    for(int i=0; i<n; i++) {
        while(p->n_write == p->n_read + PIPE_SIZE) {
            // CV
            if(p->read_open == 0 || myproc()->killed) {
                release(&p->lk);
                return -1;
            }

            wakeup(&p->n_read); // buf is full, wakeup readers
            sleep(&p->n_write, &p->lk); // sleep
        }
        p->data[p->n_write++ % PIPE_SIZE] = addr[i];
    }

    wakeup(&p->n_read);
    release(&p->lk);
    return n;
}

int64_t pipe_read(struct pipe *p, char *addr, uint64_t n) {
    acquire(&p->lk);

    while(p->n_read == p->n_write && p->write_open) {
        // CV
        if(myproc()->killed) {
            release(&p->lk);
            return -1;
        }
        sleep(&p->n_read, &p->lk);
    }

    int i;
    for(i=0; i<n; i++) {
        if(p->n_read == p->n_write) break;
        addr[i] = p->data[p->n_read++ % PIPE_SIZE];
    }

    wakeup(&p->n_write);
    release(&p->lk);
    return i;
}