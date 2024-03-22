#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

char buf[1024];

char* get_line()
{
    int i = 0;
    char c;
    while (read(0, &c, 1) > 0 && i < 1024 - 1) {
        if (c == '\n') {
            break;
        }
        buf[i++] = c;
    }
    if (i == 1024 - 1) {
        fprintf(2, "xarg: input too long\n");
        exit(1);
    }
    buf[i] = '\0';
    return buf;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: xarg command arguments\n");
        exit(1);
    }

    char* args[MAXARG];
    args[0] = argv[1];
    int i = 0; 
    for (i = 2; i < argc; ++i) {
        args[i - 1] = argv[i];
    }

    char* line;
    while (line = get_line(), line[0] != '\0') {
        if (fork() == 0) {
            args[i-1] = line;
            if (exec(argv[1], args) < 0) {
                fprintf(2, "xargs: exec %s failed\n", argv[1]);
                exit(1);
            }
            break;
        } else {
            wait(0);
        }
    }

    exit(0);
}