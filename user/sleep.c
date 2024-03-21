#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) 
{
    int time;
    if (argc != 2) {
        printf("Usage: sleep <number>\n");
        exit(1);
    }
    time = atoi(argv[1]);
    sleep(time);
    exit(0);
}