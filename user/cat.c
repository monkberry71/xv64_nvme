#include <user/user.h>

char buf[512];

int main(int argc, char **argv) {
    int fd;

    if(argc < 2) {
        while((fd = read(0, buf, sizeof(buf))) > 0)
            write(1, buf, fd);
        exit();
    }

    fd = open(argv[1], 0);
    if(fd < 0) {
        printf(1, "cat: cannot open %s\n", argv[1]);
        exit();
    }

    int n;
    while((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, n);

    close(fd);
    exit();
}