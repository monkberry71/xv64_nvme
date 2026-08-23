#include <stdint.h>

struct stat;
struct rtcdate;

// system calls
int64_t fork(void);
int64_t exit(void) __attribute__((noreturn));
int64_t sys_wait(void);
static inline int64_t wait(void) {
    return sys_wait();
}
int64_t pipe(int*);
int64_t write(int, const void*, int64_t);
int64_t read(int, void*, int64_t);
int64_t close(int);
int64_t kill(int);
int64_t exec(char*, char**);
int64_t open(const char*, int);
int64_t mknod(const char*, short, short);
int64_t unlink(const char*);
int64_t fstat(int fd, struct stat*);
int64_t link(const char*, const char*);
int64_t mkdir(const char*);
int64_t chdir(const char*);
int64_t sys_dup(int);
static inline int64_t dup(int fd) {
    return sys_dup(fd);
}
int64_t getpid(void);
char* sbrk(int);
int64_t sleep(int);
int64_t uptime(void);

// ulib.c
int64_t stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memcpy(void*, const void*, uint64_t);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint64_t strlen(const char*);
void* memset(void*, int, uint64_t);
void* malloc(uint64_t);
void free(void*);
int atoi(const char*);
