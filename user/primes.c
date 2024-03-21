#include "kernel/types.h"
#include "user/user.h"

void work(int* num, int cnt) {
    if (cnt == 0) {
        return;
    }
    printf("prime %d\n", num[0]);
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(p[1]);
        int new_cnt = 0;
        while (read(p[0], &num[new_cnt], sizeof(int))) {
            new_cnt++;
        }
        close(p[0]);
        work(num, new_cnt);
    } else {
        close(p[0]);
        for (int i = 1; i < cnt; ++i) {
            if (num[i] % num[0]) {
                write(p[1], &num[i], sizeof(int));
            }
        }
        close(p[1]);
        wait(0);
    }
}

int main()
{
    int num[34] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                   22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                   32, 33, 34, 35};
    int cnt = 34;
    work(num, cnt);
    exit(0);
}