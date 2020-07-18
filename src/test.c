#include <stdio.h>

int main()
{
    long long res=0;
    for(long long i=0;i<1000000000;++i)
    {
        res+=i;
    }

    printf("%lld\n", res);
    return 0;
}