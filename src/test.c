#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "util.h"

int main()
{
    for (long a = 1000000; a > 0; --a)
        __asm__ __volatile__(
            "nop"
        );
    printf("hi\n");
    return 0;
}

