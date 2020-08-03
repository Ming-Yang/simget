#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int *p = malloc(sizeof(int));
    int *q = malloc(sizeof(int));
    printf("%p,%p\n", p, q);
    for (int i = 0; i < 128; ++i)
        p[i] = i;

    printf("%d\n", *q);

    for (long long a = 1000000000; a > 0; --a)
    {
        int x = 0;
        ++x;
    }

    // printf("%lld\n", res);
    return 0;
}