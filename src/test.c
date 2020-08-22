#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "util.h"

int main()
{
    pid_t pid = fork();
    if(pid == 0)
    {
        sleep(100);
    }
    else
    {
        sleep(1);
        kill(pid, SIGTERM);
        printf("send\n");
        waitpid(pid, NULL, 0);
    }
    
    // for (long a = 10000000000; a > 0; --a)
    //     __asm__ __volatile__(
    //         "nop"
    //     );
    return 0;
}

