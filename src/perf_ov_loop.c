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

int perf_inst_fd, perf_cycle_fd;
FILE* loop_out_file;
int perf_child_pid;
long long loop_insts, target_insts;
int irq_offset;
struct perf_event_attr pe_insts, pe_cycles;

static void perf_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    static long long last_insts = 0, last_cycles = 0;
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_inst_fd, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_DISABLE, 0);

    if (info->si_code != POLL_IN) // Only POLL_IN should happen.
    {
        perror("wrong signal\n");
        exit(-1);
    }

    long long inst_counts = 0, cycle_counts = 0;
    if (read(perf_inst_fd, &inst_counts, sizeof(long long)) == -1)
    {
        fprintf(stderr, "read perf inst empty!\n");
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }
    else
    {
        fprintf(loop_out_file, "%lld ", inst_counts - last_insts);
        target_insts += loop_insts;
        pe_insts.sample_period = target_insts - irq_offset;
        last_insts = inst_counts;
    }

    if (read(perf_cycle_fd, &cycle_counts, sizeof(long long)) == -1)
    {
        fprintf(stderr, "read perf cycle empty!\n");
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }
    else
    {
        fprintf(loop_out_file, "%lld\n", cycle_counts - last_cycles);
        last_cycles = cycle_counts;
    }

    ioctl(perf_inst_fd, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_ENABLE, 0);
    kill(perf_child_pid, SIGCONT);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("need dump config json file\n");
        return -1;
    }
    DumpCfg *cfg = get_cfg_from_json(argv[1]);
    loop_insts = cfg->process.ov_insts;
    target_insts = loop_insts;
    irq_offset = cfg->process.irq_offset;
    loop_out_file = fopen(cfg->loop.out_file, "w+");
    if (loop_out_file < 0)
    {
        perror("loop output file open error");
        loop_out_file = stdout;
    }
#ifdef _DEBUG
    printf("get config finish\n");
    print_dump_cfg(cfg);
#endif
    memset(&pe_insts, 0, sizeof(struct perf_event_attr));
    pe_insts.type = PERF_TYPE_HARDWARE;
    pe_insts.size = sizeof(struct perf_event_attr);
    pe_insts.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe_insts.disabled = 1;
    // pe_insts.inherit = 1;
    pe_insts.exclude_kernel = 1;
    pe_insts.exclude_hv = 1;
    pe_insts.exclude_idle = 1;
    pe_insts.enable_on_exec = 1;
    // pe_insts.precise_ip = 1;

    pe_insts.sample_period = cfg->process.ov_insts - cfg->process.irq_offset;
    pe_insts.wakeup_events = 100;

    memset(&pe_cycles, 0, sizeof(struct perf_event_attr));
    pe_cycles.type = PERF_TYPE_HARDWARE;
    pe_cycles.size = sizeof(struct perf_event_attr);
    pe_cycles.config = PERF_COUNT_HW_CPU_CYCLES;
    pe_cycles.disabled = 1;
    // pe_cycles.inherit = 1;
    pe_cycles.exclude_kernel = 1;
    pe_cycles.exclude_hv = 1;
    pe_cycles.exclude_idle = 1;
    pe_cycles.enable_on_exec = 1;
    // pe_cycles.precise_ip = 1;

    perf_child_pid = fork();
    if (perf_child_pid < 0)
    {
        perror("fork failed");
        return -1;
    }
    else if (perf_child_pid == 0) //child
    {
        set_sched(getpid(), cfg->process.affinity);
        redirect_io(cfg);

        raise(SIGSTOP);
        execv(cfg->process.path_file, cfg->process.argv);
    }
    else //parent
    {
        waitpid(perf_child_pid, NULL, WUNTRACED);
        cfg->process.child_pid = perf_child_pid;
        perf_inst_fd = perf_event_open(&pe_insts, perf_child_pid, -1, -1, 0);
        if (perf_inst_fd == -1)
        {
            fprintf(stderr, "Error opening leader %llx\n", pe_insts.config);
            kill(perf_child_pid, SIGTERM);
            return -1;
        }
        cfg->process.perf_fd = perf_inst_fd;

        perf_cycle_fd = perf_event_open(&pe_cycles, perf_child_pid, -1, -1, 0);
        if (perf_cycle_fd == -1)
        {
            fprintf(stderr, "Error opening leader %llx\n", pe_cycles.config);
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
        if (sigaction(SIGIO, &sa, 0) < 0)
        {
            perror("sigaction");
            kill(perf_child_pid, SIGTERM);
            exit(-1);
        }

        // Setup event handler for overflow signals
        fcntl(perf_inst_fd, F_SETFL, O_NONBLOCK | O_ASYNC);
        fcntl(perf_inst_fd, __F_SETSIG, SIGIO);
        fcntl(perf_inst_fd, F_SETOWN, getpid());

        // init criu
        printf("config finish, resume child process...\n");

        // resume child process
        ioctl(perf_inst_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(perf_cycle_fd, PERF_EVENT_IOC_RESET, 0);
        kill(perf_child_pid, SIGCONT);

        waitpid(perf_child_pid, NULL, 0);

        long long inst_counts = 0, cycle_counts = 0;
        if (read(perf_inst_fd, &inst_counts, sizeof(long long)) == -1)
        {
            fprintf(stderr, "read perf inst empty!\n");
        }
        else
        {
            fprintf(loop_out_file, "%lld ", inst_counts);
            printf("total insts:");
            print_long(inst_counts);
        }

        if (read(perf_cycle_fd, &cycle_counts, sizeof(long long)) == -1)
        {
            fprintf(stderr, "read perf cycle empty!\n");
        }
        else
        {
            fprintf(loop_out_file, "%lld", cycle_counts);
            printf("total cycles:");
            print_long(cycle_counts);
        }
        close(perf_inst_fd);
    }
}
