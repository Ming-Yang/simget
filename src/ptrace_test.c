#include <sys/ptrace.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

int main()
{
    long long counter = 1; // machine instruction counter
    int wait_val;          // child's return value
    int pid;               // child's process id
    int dat;

    switch (pid = fork())
    { // copy entire parent space in child
    case -1:
        perror("fork");
        break;

    case 0: // child process starts
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        /* 
           must be called in order to allow the
           control over the child process and trace me (child)
           0 refers to the parent pid
        */

        execl("./test", "test", NULL);
        /* 
           executes the testprogram and causes
           the child to stop and send a signal
           to the parent, the parent can now
           switch to PTRACE_SINGLESTEP
        */
        break;
        // child process ends
    default: // parent process starts
        wait(&wait_val);
        if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0) != 0)
            perror("ptrace");
        /* 
                            switch to singlestep tracing and 
                            release child
                            if unable call error.
                         */
        wait(&wait_val);
        // parent waits for child to stop at next
        // instruction (execl())
        while (wait_val == 1407)
        {
            counter++;
            if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0) != 0)
                perror("ptrace");
            /* 
                            switch to singlestep tracing and 
                            release child
                            if unable call error.
                         */
            wait(&wait_val);
            // wait for next instruction to complete  */
        }
        /*
          continue to stop, wait and release until
          the child is finished; wait_val != 1407
          Low=0177L and High=05 (SIGTRAP)
        */
    }
    printf("Number of machine instructions : %lld\n", counter);
    return 0;
} // end of switch