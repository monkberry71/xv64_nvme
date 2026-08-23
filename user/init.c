#include <user/fcntl.h>
#include <user/user.h>

char *argv[] = {"sh", 0};

int main(void) {
    int fd = open("/dev/console", O_RDWR);
    if(fd < 0) {
        mknod("/dev/console", 1, 1); // CONSOLE_DEVNUM 1
        open("/dev/console", O_RDWR);
    }
    dup(0); // stdout 1
    dup(0); // stderr 2


    int pid = fork();
    if(pid == 0) {
        write(1, "child\n", 6);
        exit();
    }
    wait();
    write(1, "parent\n", 7);

    char c;

    for(;;) {
    int n = read(0, &c, 1);
    if(n < 0) {
        write(1, "read error\n", 11);
        continue;
    }
    if(n == 0) {
        write(1, "EOF\n", 4);
        continue;
    }
    write(1, &c, 1);
}

    // for(;;) {
    //     printf(1, "init: starting sh\n");
    //     int pid = fork();
    //     if(pid < 0) {
    //         printf(1, "init: fork failed\n");
    //         exit();
    //     }
    //     if(pid == 0) {
    //         exec("/nsh", argv);
    //         printf(1, "init: exec sh failed\n");
    //         exit();
    //     }
    //     int wpid;
    //     while((wpid=wait()) >= 0 && wpid != pid) 
    //         printf(1, "zombie!\n");
    // }
}