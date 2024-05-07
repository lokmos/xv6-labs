#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int p[2]) {
    int prime;
    read(p[0], &prime, sizeof(prime));
    if (prime == -1) {
        exit(0);
    }
    printf("prime %d\n", prime);

    int p2[2];
    pipe(p2);

    if (fork() != 0) {
        close(p2[0]);
        int num;
        while (read(p[0], &num, sizeof(num)) && num != -1) {
            if (num % prime != 0) {
                write(p2[1], &num, sizeof(num));
            }
        }
        num = -1;
        write(p2[1], &num, sizeof(num));
        wait(0);
        exit(0);
    }
    else {
        close(p[0]);
        close(p2[1]);
        sieve(p2);
    }
}

int main(int argc, char *argv[]) {
    int p[2];
    pipe(p);

    if (fork() != 0) {
        close(p[0]);
        int i;
        for (i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        i = -1;
        write(p[1], &i, sizeof(i));
    }
    else {
        close(p[1]);
        sieve(p);
        exit(0);
    }
    wait(0);
    exit(0);
}