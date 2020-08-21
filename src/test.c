#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"

long get_insts_from_dir_name(char *path)
{
    int start_idx = strlen(path);
    for (; path[start_idx] != '/'; --start_idx)
        ;
    return atol(path + start_idx + 1);
}

int main()
{
    long a = 123456789876;
    printf("%s\n", long2string(a));
    printf("%s\n", nstrjoin(3, "abc", "def", "ghi"));
    printf("%ld\n", get_insts_from_dir_name("/home/ip/h/1234567898765"));
    // for (long a = 10000000000; a > 0; --a)
    //     __asm__ __volatile__(
    //         "nop"
    //     );
    return 0;
}