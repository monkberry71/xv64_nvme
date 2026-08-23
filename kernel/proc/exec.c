#include <stdint.h>
#include <kernel/fs.h>
#include <shared/fs.h>
#include <kernel/file.h>
#include <kernel/mmu.h>
#include <kernel/elf.h>
#include <kernel/uvm.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <kernel/uart.h>
#include <kernel/debug.h>

void syscall_ret(void);

static void enter_user_from_tf(struct trap_frame *tf) {
    __asm__ volatile("movq %0, %%rsp; jmp syscall_ret" :: "r"(tf) : "memory");
    for(;;);
}

int64_t exec(char *path, char **argv) {
    struct inode *ip = namei(path);
    if(!ip) {
        serial_printf("exec: failed to find inode\n");
        return -1;
    }

    ilock(ip);

    pte_t *pml4 = 0;

    // if sz starts from 4096, alloc_uvm won't make 0 page
    uint64_t sz = PGSIZE_4KB;

    struct elf_header elf;
    int64_t res = readi(ip, (void*) &elf, 0, sizeof(elf));
    if(res != sizeof(elf)) goto bad;
    if(elf.magic != ELF_MAGIC) goto bad;
    if(elf.ph_entry_size != sizeof(struct prog_header)) goto bad;

    pml4 = setup_uvm();
    if(!pml4) goto bad;

    for(int i = 0; i < elf.ph_num; i++) {
        uint64_t offset = elf.ph_off + i * elf.ph_entry_size;

        struct prog_header ph;
        int64_t bytes_r = readi(ip, (void*) &ph, offset, sizeof(ph));
        if(bytes_r != sizeof(ph)) goto bad;
        if(ph.type != ELF_PROG_LOAD) continue;
        // Must:
        // mem_sz(bytes the segment occupies in mem) >= file_sz (bytes the segment exists in the ELF file)
        // bss will be zero filled!!
        if(ph.mem_sz < ph.file_sz) goto bad;
        // OF
        if(ph.mem_sz >= UINT64_MAX - ph.vaddr) goto bad;

        sz = alloc_uvm(pml4, sz, ph.vaddr + ph.mem_sz);
        if(sz == 0) goto bad;

        // null page protection
        if(ph.vaddr < PGSIZE_4KB) goto bad;
        // not aligend
        if(ph.vaddr % PGSIZE_4KB) goto bad;
        
        if(load_uvm(pml4, (void *) ph.vaddr, ip, ph.offset, ph.file_sz) < 0) goto bad;
    }
    iunlock(ip);
    iput(ip);
    ip = 0;

    sz = ROUND_UP(sz, PGSIZE_4KB);
    sz = alloc_uvm(pml4, sz, sz + 2*PGSIZE_4KB);
    if(!sz) goto bad;

    clear_pte_u(pml4, (void*)(sz - 2*PGSIZE_4KB)); // stack canary
    
    // Ex) argv = {"echo", "hello", "world", 0}
    // User stack goal 
    // 0 -->
    // argv[0] | argv[1] | argv[2] | argv[3] | alignment pad | "echo\0hello\0world\0"
    uint64_t sp = sz;
    int argc = 0;
    
    // Count argc
    while(argv[argc]) {
        if(argc >= MAX_ARGS) goto bad;
        argc++;
    }

    // Copy the actual strs to the user stack; "echo\0hello\0world\0"
    uint64_t arg_ptrs[MAX_ARGS+1];
    arg_ptrs[argc] = 0;
    for(int i = argc-1; i>=0; i--) {
        uint64_t len = strlen(argv[i]) + 1;
        
        sp -= len;
        if(copy_out(pml4, (void *) sp, argv[i], len) < 0) goto bad;
        arg_ptrs[i] = sp;
    }

    // Align before placing arg_ptrs
    uint64_t argv_length = (argc + 1) * sizeof(uint64_t);
    sp -= argv_length; // 16N  -- sp ---(argv_length) --- "echo..."
    sp = ROUND_DOWN(sp, 16); // sp=16N --- (argv_length + ?) --- "echo"
    uint64_t argv_user = sp; // ^start argv here 
    if(copy_out(pml4, (void *) argv_user, arg_ptrs, argv_length) < 0) goto bad;
    
    sp -= 8;
    uint64_t fake_ret = 0;
    if(copy_out(pml4, (void *) sp, &fake_ret, 8) < 0) goto bad;

    // Final
    // 16N-8 -- 16N 
    // fake_ret  | argv [0] | argv[1] | argv[2] | NULL | PAD | "echo..."    

    // get the program name, last slash's next char is the starting point of the name
    char *p_name, *s;
    for(p_name = s = path; *s; s++) {
        if(*s == '/') p_name = s+1;
    }

    struct proc *cur_p = myproc();
    strncpy(cur_p->name, p_name, s - p_name);

    pte_t *old_pml4 = cur_p->pml4;
    uint64_t old_sz = cur_p->sz;
    cur_p->pml4 = pml4;
    cur_p->sz = sz;
    cur_p->tf->gprs.rcx = elf.entry; // main
    cur_p->tf->rsp = sp;
    cur_p->tf->gprs.r11 = BIT(9); // enable intr, rflags restored
    cur_p->tf->gprs.rdi = argc;
    cur_p->tf->gprs.rsi = argv_user;

    switch_uvm(cur_p);
    free_vm(old_pml4, old_sz);
    return 0;

bad:
    if(pml4) free_vm(pml4, sz);
    if(ip) {
        iunlock(ip);
        iput(ip);
    }
    return -1;
}

void test_exec(uint64_t arg) {
    (void)arg;

    struct proc *p = myproc();
    pte_t *dummy_pml4 = setup_uvm();
    if(!dummy_pml4)
        panic("test_exec: setup_uvm failed");

    p->tf = (struct trap_frame *)p->kstack;
    memset(p->tf, 0, sizeof(*p->tf));
    p->pml4 = dummy_pml4;
    p->sz = PGSIZE_4KB;

    char *argv[] = { "init", "exec-test", 0 };
    serial_printf("test_exec: exec /init\n");

    if(exec("/init", argv) < 0)
        panic("test_exec: exec failed");

    serial_printf("test_exec: entering user mode\n");
    enter_user_from_tf(p->tf);
}
