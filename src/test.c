#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"

int main()
{
    long a = 123456789876;
    printf("%s\n", long2string(a));
    printf("%s\n", nstrjoin(3, "abc", "def", "ghi"));
    // for (long a = 10000000000; a > 0; --a)
    //     __asm__ __volatile__(
    //         "nop"
    //     );
    return 0;
}
