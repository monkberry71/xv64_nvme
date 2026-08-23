#include <stdint.h>
#include <kernel/proc.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/pipe.h>

static int fd_alloc(struct file *f) {
    struct proc *cur_p = myproc();

    for(int fd = 0; fd < N_OFILES; fd++) {
        if(cur_p->ofile[fd] == 0) {
            cur_p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

#define CHECKFD(fd) if((fd) < 0 || (fd) >= N_OFILES || (myproc()->ofile[(fd)]) == 0) return -1

int64_t sys_dup(void) {
    int old_fd = myproc()->tf->gprs.rdi;
    CHECKFD(old_fd);

    struct file *f = myproc()->ofile[old_fd];

    int fd = fd_alloc(f);
    if(fd < 0) return -1;
    file_dup(f);
    return fd;
}

int64_t sys_read(void) {
    int fd = myproc()->tf->gprs.rdi;
    CHECKFD(fd);

    char *p = (void*)myproc()->tf->gprs.rsi;
    uint64_t n = myproc()->tf->gprs.rdx;

    struct file *f = myproc()->ofile[fd];

    return file_read(f, p, n);
}

int64_t sys_write(void) {
    int fd = myproc()->tf->gprs.rdi;
    CHECKFD(fd);

    char *p = (void*)myproc()->tf->gprs.rsi;
    uint64_t n = myproc()->tf->gprs.rdx;

    struct file *f = myproc()->ofile[fd];

    return file_write(f, p, n);
}

int64_t sys_close(void) {
    int fd = myproc()->tf->gprs.rdi;
    CHECKFD(fd);
    struct file *f = myproc()->ofile[fd];
    myproc()->ofile[fd] = 0;
    file_close(f);
    return 0;
}

int64_t sys_fstat(void) {
    int fd = myproc()->tf->gprs.rdi;
    CHECKFD(fd);
    struct stat *st = (void*) myproc()->tf->gprs.rsi;
    struct file *f = myproc()->ofile[fd];

    if(!st) return -1;
    return file_stat(f, st);
}

int64_t sys_chdir(void) {
    char *path = (void*) myproc()->tf->gprs.rdi;
    struct inode *ip = namei(path);

    if(!ip) return -1;

    ilock(ip);
    if(ip->type != T_DIR) {
        // chdir to non-dir
        iunlock(ip);
        iput(ip);
        return -1;
    }
    iunlock(ip);

    iput(myproc()->cwd);
    myproc()->cwd = ip;
    return 0;
}

// Create locked inode of the path
static struct inode* create(char *path, uint16_t type, uint16_t major, uint16_t minor) {
    char name[DIR_SIZE];

    struct inode *dp = namei_parent(path, name);
    if(!dp) return 0;

    ilock(dp);

    struct inode *ip = dir_lookup(dp, name, 0);

    if(ip) {
        iunlock(dp);
        iput(dp);
        ilock(ip);
        if(type == T_FILE && ip->type == T_FILE) return ip;
        iunlock(ip);
        iput(ip);
        return 0;
    }

    ip = ialloc(type);
    if(!ip)
        panic("create: ialloc failed");

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->n_link = 1;
    iupdate(ip);

    if(type == T_DIR) {
        dp->n_link++;
        iupdate(dp);

        if(dir_link(ip, ".", ip->inum) < 0 || dir_link(ip, "..", dp->inum) < 0) {
            panic("create: dots dir entry failed");
        }
    }

    if(dir_link(dp, name, ip->inum) < 0)
        panic("create: cant belong to the parent");
    
    iunlock(dp);
    iput(dp);

    return ip;
}

int64_t sys_open(void) {
    char *path = (void *) myproc()->tf->gprs.rdi;
    uint64_t omode = myproc()->tf->gprs.rsi;

    struct inode *ip;

    if(omode & O_CREATE) {
        // create
        ip = create(path, T_FILE, 0, 0);
        if(!ip) return -1;
    } else {
        // edit
        ip = namei(path);
        if(!ip) return -1;
        ilock(ip);
        if(ip->type == T_DIR && omode != O_RDONLY) {
            // open cannot edit dir
            iunlock(ip);
            iput(ip);
            return -1;
        }
    }

    struct file *f = file_alloc();
    if(!f) {
        iunlock(ip);
        iput(ip);
        return -1;
    }

    int fd = fd_alloc(f);
    if(fd < 0) {
        file_close(f);
        iunlock(ip);
        iput(ip);
        return -1;
    }

    iunlock(ip);

    // we dont need the ftable lock, because when file_alloc, with lock, it increase the ref, so other thread wont pick it
    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY); // wr only on -> no read
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR); // WRONLY or RDRW -> writable

    return fd;
}

int64_t sys_mkdir(void) {
    char *path = (void*) myproc()->tf->gprs.rdi;

    struct inode *ip = create(path, T_DIR, 0, 0);
    if(!ip) return -1;
    iunlock(ip);
    iput(ip);
    return 0;
}

int64_t sys_pipe(void) {
    int *fd = (void*) myproc()->tf->gprs.rdi;

    struct file *rf, *wf;
    if(pipe_alloc(&rf, &wf) < 0) return -1;

    int fd0 = fd_alloc(rf);
    if(fd0 < 0) {
        file_close(rf);
        file_close(wf);
        return -1;
    }

    int fd1 = fd_alloc(wf);
    if(fd1 < 0) {
        myproc()->ofile[fd0] = 0;
        file_close(rf);
        file_close(wf);
        return -1;
    }

    fd[0] = fd0;
    fd[1] = fd1;

    return 0;
}
