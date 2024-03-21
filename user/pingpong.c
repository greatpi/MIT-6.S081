#include "kernel/types.h"
#include "user/user.h"

int p2c[2], c2p[2];
char buf[1];

int main() 
{
    pipe(p2c);
    pipe(c2p);
    if (fork() == 0) {
        close(c2p[0]);
        close(p2c[1]);
        read(p2c[0], buf, 1);
        write(c2p[1], "a", 1);
        printf("%d: received ping\n", getpid());
        close(c2p[1]);
        close(p2c[0]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);
        write(p2c[1], "a", 1);
        read(c2p[0], buf, 0);
        wait(0);
        printf("%d: received pong\n", getpid());
        close(p2c[1]);
        close(c2p[0]);
        exit(0);
    }
}