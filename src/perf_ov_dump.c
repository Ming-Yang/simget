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
#include "criu_util.h"

int perf_open_fd;
int perf_child_pid;

static void perf_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_open_fd, PERF_EVENT_IOC_DISABLE, 0);

    if (info->si_code != POLL_IN) // Only POLL_IN should happen.
    {
        perror("wrong signal\n");
        exit(-1);
    }

    image_dump_criu(perf_child_pid);

    long inst_counts = 0;
    if (read(perf_open_fd, &inst_counts, sizeof(long)) == -1)
    {
        fprintf(stderr, "read perf event empty!\n");
    }
    else
    {
        printf("dump image at insts:");
        print_long(inst_counts);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("need dump config json file\n");
        return -1;
    }
    DumpCfg *cfg = get_cfg_from_json(argv[1]);
#ifdef _DEBUG
    printf("get config finish\n");
    print_dump_cfg(cfg);
#endif
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    // pe.inherit = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.exclude_idle = 1;
    pe.enable_on_exec = 1;
    // pe.precise_ip = 1;

    pe.sample_period = cfg->process.ov_insts - cfg->process.irq_offset;
    pe.wakeup_events = 100;

    perf_child_pid = fork();
    if (perf_child_pid < 0)
    {
        perror("fork failed");
        return -1;
    }
    else if (perf_child_pid == 0) //child
    {
        set_sched(getpid(), cfg->process.affinity);
        detach_from_shell(cfg);

        raise(SIGSTOP);
        execv(cfg->process.path_file, cfg->process.argv);
    }
    else //parent
    {
        waitpid(perf_child_pid, NULL, WUNTRACED);
        perf_open_fd = perf_event_open(&pe, perf_child_pid, -1, -1, 0);
        if (perf_open_fd == -1)
        {
            fprintf(stderr, "Error opening leader %llx\n", pe.config);
            kill(perf_child_pid, SIGTERM);
            return -1;
        }

        // signal handler:
        // Configure signal handler
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_sigaction = perf_event_handler;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;

        // Setup signal handler
        if (sigaction(SIGIO, &sa, NULL) < 0)
        {
            perror("sigaction");
            kill(perf_child_pid, SIGTERM);
            exit(-1);
        }

        // Setup event handler for overflow signals
        fcntl(perf_open_fd, F_SETFL, O_NONBLOCK | O_ASYNC);
        fcntl(perf_open_fd, __F_SETSIG, SIGIO);
        fcntl(perf_open_fd, F_SETOWN, getpid());

        // init criu
        set_image_dump_criu(perf_child_pid, cfg->image_dir, false);
        printf("config finish, resume child process...\n");

        // resume child process
        ioctl(perf_open_fd, PERF_EVENT_IOC_RESET, 0);
        kill(perf_child_pid, SIGCONT);

        waitpid(perf_child_pid, NULL, 0);
        close(perf_open_fd);
    }
}
