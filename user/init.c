#include <user/fcntl.h>
#include <user/user.h>

char *argv[] = {"nsh", 0};

int main(void) {
    if(open("/dev/console", O_RDWR) < 0) {
        mknod("/dev/console", 1, 1); // CONSOLE_DEVNUM 1
        open("/dev/console", O_RDWR);
    }
    dup(0); // stdout 1
    dup(0); // stderr 2

    // write(1, "init", 5);

    for(;;) {
        printf(1, "init: starting sh\n");
        int pid = fork();
        if(pid < 0) {
            printf(1, "init: fork failed\n");
            exit();
        }
        if(pid == 0) {
            exec("/nsh", argv);
            printf(1, "init: exec sh failed\n");
            exit();
        }
        int wpid;
        while((wpid=wait()) >= 0 && wpid != pid) 
            printf(1, "zombie!\n");
    }
}