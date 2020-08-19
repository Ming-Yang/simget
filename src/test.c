#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    for (long long a = 10000000000; a > 0; --a)
        __asm__ __volatile__(
            "nop"
        );
    return 0;
}
