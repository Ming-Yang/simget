#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include "criu_util.h"
#include "util.h"
#include <string.h>
#include <sys/ioctl.h>
#include "perf_event_open.h"

DumpCfg *cfg;
pid_t perf_child_pid;
int perf_warmup_fd, perf_inst_fd, perf_cycle_fd;
FILE *restore_out_file;
long get_insts_from_dir_name(char *);
int warmup_finish, event_finish;

static void perf_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_inst_fd, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_DISABLE, 0);

    if (info->si_code != POLL_IN) // Only POLL_IN should happen.
    {
        perror("wrong signal\n");
        exit(-1);
    }
    if (event_finish == 1)
        fprintf(stderr, "event finish again!");
    event_finish = 1;
}

static void warmup_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_warmup_fd, PERF_EVENT_IOC_DISABLE, 0);

    if (warmup_finish == 1)
        fprintf(stderr, "warmup finish again!");
    warmup_finish = 1;
}

int main(int argc, char **argv)
{
    cfg = get_cfg_from_json(argv[1]);
    // if (cfg->process.process_pid > 0)
    //     kill(cfg->process.process_pid, SIGKILL);
    restore_out_file = fopen(nstrjoin(2, cfg->image_dir, "_restore_out.log"), "a+");
    int dir_fd = set_image_restore_criu(cfg->image_dir);
    set_sched(getpid(), cfg->process.affinity);
    // while (kill(cfg->process.process_pid, 0) == 0)
    //     ;
    perf_child_pid = image_restore_criu(dir_fd);
    set_sched(perf_child_pid, cfg->process.affinity);
#ifdef _DEBUG
    printf("restore child pid:%d\n", perf_child_pid);
#endif

    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.enable_on_exec = 1;

    long dump_offset = get_insts_from_dir_name(cfg->image_dir) - (cfg->simpoint.points[cfg->simpoint.current] * cfg->process.ov_insts - cfg->process.ov_insts * (int)cfg->process.warmup_ratio);
    if (cfg->simpoint.points[cfg->simpoint.current] == 0 || (int)cfg->process.warmup_ratio == 0) // no warmup at all
        pe.sample_period = cfg->process.ov_insts;
    else if (cfg->simpoint.points[cfg->simpoint.current] - (int)cfg->process.warmup_ratio <= 0) // shorten warmup
        pe.sample_period = cfg->simpoint.points[cfg->simpoint.current] * cfg->process.ov_insts - cfg->process.irq_offset;
    else // normal warmup
        pe.sample_period = cfg->process.ov_insts * (int)cfg->process.warmup_ratio - cfg->process.irq_offset - dump_offset;
    pe.wakeup_events = 100;

    // signal handler:
    // Configure signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = warmup_event_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    if (cfg->simpoint.points[cfg->simpoint.current] == 0 || (int)cfg->process.warmup_ratio == 0) // no warmup at all
    {
        printf("no warmup :");
    }
    else // warmup, need call warmup_event_handler
    {
        perf_warmup_fd = perf_event_open(&pe, perf_child_pid, -1, -1, 0);
        if (perf_warmup_fd == -1)
        {
            PRINT_DEBUG_INFO;
            fprintf(stderr, "Error opening leader %llx\n", pe.config);
            kill(perf_child_pid, SIGTERM);
            return -1;
        }

        // Setup signal handler
        if (sigaction(SIGUSR1, &sa, 0) < 0)
        {
            perror("sigaction");
            kill(perf_child_pid, SIGTERM);
            exit(-1);
        }

        // Setup event handler for overflow signals
        fcntl(perf_warmup_fd, F_SETFL, O_NONBLOCK | O_ASYNC);
        fcntl(perf_warmup_fd, __F_SETSIG, SIGUSR1);
        fcntl(perf_warmup_fd, F_SETOWN, getpid());

        ioctl(perf_warmup_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(perf_warmup_fd, PERF_EVENT_IOC_ENABLE, 0);

        kill(perf_child_pid, SIGCONT);

        while (!warmup_finish)
        {
            pause();
        }

        long inst_counts = 0;
        if (read(perf_warmup_fd, &inst_counts, sizeof(long)) == -1)
        {
            fprintf(stderr, "read perf inst empty!\n");
            kill(perf_child_pid, SIGTERM);
            exit(-1);
        }
        else
        {
            printf("after %ld warmup :", inst_counts);
        }

        close(perf_warmup_fd);
    }

    struct perf_event_attr pe_insts = pe, pe_cycles = pe;

    pe_insts.sample_period = cfg->process.ov_insts - cfg->process.irq_offset;
    perf_inst_fd = perf_event_open(&pe_insts, perf_child_pid, -1, -1, 0);
    if (perf_inst_fd == -1)
    {
        PRINT_DEBUG_INFO;
        fprintf(stderr, "Error opening leader %llx\n", pe_insts.config);
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }

    pe_cycles.config = PERF_COUNT_HW_CPU_CYCLES;
    pe_cycles.enable_on_exec = 1;
    pe_cycles.exclude_idle = 1;
    perf_cycle_fd = perf_event_open(&pe_cycles, perf_child_pid, -1, -1, 0);
    if (perf_cycle_fd == -1)
    {
        PRINT_DEBUG_INFO;
        fprintf(stderr, "Error opening leader %llx\n", pe_cycles.config);
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }

    // signal handler:
    // Configure signal handler
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

    ioctl(perf_inst_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_RESET, 0);

    ioctl(perf_inst_fd, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_ENABLE, 0);
    kill(perf_child_pid, SIGCONT);

    while (!event_finish)
    {
        pause();
    }

    long inst_counts = 0, cycle_counts = 0;
    if (read(perf_inst_fd, &inst_counts, sizeof(long)) == -1)
    {
        fprintf(stderr, "read perf inst empty!\n");
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }
    else
    {
        fprintf(restore_out_file, "%ld ", inst_counts);
        printf("%ld ", inst_counts);
    }

    if (read(perf_cycle_fd, &cycle_counts, sizeof(long)) == -1)
    {
        fprintf(stderr, "read perf cycle empty!\n");
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }
    else
    {
        fprintf(restore_out_file, "%ld ", cycle_counts);
        printf("%ld ", cycle_counts);
    }

    fprintf(restore_out_file, "%f\n", cfg->simpoint.weights[cfg->simpoint.current]);
    printf("%f\n", cfg->simpoint.weights[cfg->simpoint.current]);

    kill(perf_child_pid, SIGKILL); // SIGTERM cannot be caught by parrent wait()

    return 0;
}

long get_insts_from_dir_name(char *path)
{
    int start_idx = strlen(path);
    for (; path[start_idx] != '/'; --start_idx)
        ;
    return atol(path + start_idx + 1);
}
