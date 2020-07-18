#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>

void print_long(long long val)
{
    int val_table[20];
    int table_eles = 0;
    while (val > 0)
    {
        val_table[table_eles] = val % 1000;
        val /= 1000;
        table_eles++;
    }
    for (int i = table_eles - 1; i >= 0; --i)
    {
        if (i == table_eles - 1)
            printf("%d", val_table[i]);
        else
            printf("%03d", val_table[i]);

        if (i != 0)
            printf(",");
    }
    printf("\n");
}

#endif // __UTIL_H