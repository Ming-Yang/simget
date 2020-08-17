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
#include <cJSON.h>
#include "perf_event_open.h"
#include "util.h"

int perf_open_fd;
int perf_child_pid;

int main(int argc, char **argv)
{
    if(argc<2)
    {
        printf("need dump config json file\n");
        return -1;
    }
    DumpCfg* cfg = get_cfg_from_json(argv[1]);
    printf("get config finish\n");

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
        execv(cfg->process.path, cfg->process.args);
    }
    else //parent
    {
        perf_child_pid = fork_pid;
        waitpid(perf_child_pid, NULL, WUNTRACED);

        // waitpid(fork_pid, NULL, WUNTRACED);
        // kill(fork_pid, SIGCONT);
        // waitpid(perf_child_pid, NULL, WUNTRACED);
        // kill(perf_child_pid, SIGSTOP);

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