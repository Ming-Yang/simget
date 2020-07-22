#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <wait.h>
#include <stdbool.h>
#include "perf_event_open.h"
#include "util.h"

// const char *process_path = "./coremark.exe";
// char* const process_args[] = {"coremark.exe", "0x0", "0x0", "0x66", "0", "7", "1", "2000", NULL};

// const char *process_path = "./test";
// char* const process_args[] = {"test", NULL};

const char *process_path = "./perf_ov_restore";
char* const process_args[] = {"perf_ov_restore", NULL};

int perf_open_fd;
int perf_child_pid;

int main(int argc, char **argv)
{
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.enable_on_exec = 1;

    pid_t fork_pid = fork();
    if (fork_pid < 0)
    {
        perror("vfork failed");
        return -1;
    }
    else if (fork_pid == 0) //child
    {
        raise(SIGSTOP);
        execv(process_path, process_args);
    }
    else //parent
    {
        // perf_child_pid = fork_pid;
        // waitpid(perf_child_pid, NULL, WUNTRACED);

        waitpid(fork_pid, NULL, WUNTRACED);
        kill(fork_pid, SIGCONT);
        waitpid(perf_child_pid, NULL, WUNTRACED);
        kill(perf_child_pid, SIGSTOP);

        perf_open_fd = perf_event_open(&pe, perf_child_pid, -1, -1, 0);
        if (perf_open_fd == -1)
        {
            fprintf(stderr, "Error opening leader %llx\n", pe.config);
            kill(perf_child_pid, SIGTERM);
            return -1;
        }

        // resume child process
        ioctl(perf_open_fd, PERF_EVENT_IOC_RESET, 0);
        kill(perf_child_pid, SIGCONT);
        waitpid(perf_child_pid, NULL, 0);
        ioctl(perf_open_fd, PERF_EVENT_IOC_DISABLE, 0);
        
        long long inst_counts=-1;
        read(perf_open_fd, &inst_counts, sizeof(long long));
        printf("total inst counts:");
        print_long(inst_counts);

        close(perf_open_fd);
    }
}
