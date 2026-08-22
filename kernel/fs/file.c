#include <stdint.h>
#include <kernel/file.h>
#include <kernel/spin_lock.h>
#include <kernel/fs.h>
#include <kernel/defs.h>
#include <kernel/debug.h>

struct dev_sw devs[N_DEVS];

// ok it is not a cache or something,
// it is a pure memory table
struct {
    struct spin_lock lk;
    struct file files[N_FILES];
} ftable;

void file_init(void) {
    init_lock(&ftable.lk, "ftable");
}

// there is no file_open func,
// since file opening may include something like
// new inode creating
// file.c only does file table management
// sys_open will open the file with file_alloc

// Allocate a file struct
struct file* file_alloc(void) {
    acquire(&ftable.lk);
    for(int i=0; i<N_FILES; i++) {
        struct file *f = &ftable.files[i];
        if(f->ref) continue;

        f->ref = 1;
        release(&ftable.lk);
        return f;
    }
    release(&ftable.lk);
    return 0;
}

struct file* file_dup(struct file *f) {
    acquire(&ftable.lk);
    if(!f->ref) 
        panic("file_dup: nothing to dup");
    
    f->ref++;
    release(&ftable.lk);
    return f;
}

void file_close(struct file *f) {
    acquire(&ftable.lk);
    if(!f->ref)
        panic("file_close: nothing to close");
    
    if(--f->ref) {
        release(&ftable.lk);
        return;
    }

    // f->ref was 1, now 0
    struct file ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lk);

    if(ff.type == FD_PIPE) {
        // pipe close
    } else if(ff.type == FD_INODE) {
        iput(ff.ip);
    }
}

int file_stat(struct file *f, struct stat *st) {
    if(f->type != FD_INODE) 
        return -1;
    
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
}

int64_t file_read(struct file *f, uint8_t *addr, uint64_t n) {
    if(f->readable == 0) return -1;
    if(f->type == FD_PIPE) {
        // pipe read
        return -1;
    }
    if(f->type == FD_INODE) {
        ilock(f->ip);
        int64_t bytes_read = readi(f->ip, addr, f->off, n);
        if(bytes_read > 0) {
            f->off += bytes_read;
        }
        iunlock(f->ip);
        return bytes_read;
    }
    panic("file_read: cannot identify file type");
}

int64_t file_write(struct file *f, uint8_t *addr, uint64_t n) {
    if(f->writable == 0) return -1;
    if(f->type == FD_PIPE) {
        return -1;
    }
    if(f->type == FD_INODE) {
        ilock(f->ip);
        int64_t bytes_written = writei(f->ip, addr, f->off, n);
        if(bytes_written > 0) {
            f->off += bytes_written;
        }
        iunlock(f->ip);
        return bytes_written;
    }
    panic("file_write: cannot identify file type");
}
