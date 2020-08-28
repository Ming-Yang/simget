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
        fprintf(restore_out_file, "%ld\n", cycle_counts);
        printf("%ld\n", cycle_counts);
    }

    kill(perf_child_pid, SIGKILL); // SIGTERM cannot be caught by parrent wait()
}

static void warmup_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_warmup_fd, PERF_EVENT_IOC_DISABLE, 0);

    // if (info->si_code != POLL_IN) // Only POLL_IN should happen.
    // {
    //     fprintf(stderr, "wrong signal info %x\n", info->si_code);
    //     exit(-1);
    // }

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

    struct perf_event_attr pe_insts, pe_cycles;
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

    perf_inst_fd = perf_event_open(&pe_insts, perf_child_pid, -1, -1, 0);
    if (perf_inst_fd == -1)
    {
        PRINT_DEBUG_INFO;
        fprintf(stderr, "Error opening leader %llx\n", pe_insts.config);
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }

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

    ioctl(perf_inst_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_RESET, 0);

    ioctl(perf_inst_fd, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(perf_cycle_fd, PERF_EVENT_IOC_ENABLE, 0);
    kill(perf_child_pid, SIGCONT);
}

int main(int argc, char **argv)
{
    cfg = get_cfg_from_json(argv[1]);

    restore_out_file = fopen(nstrjoin(2, cfg->image_dir, "_restore_out.log"), "a+");

    set_image_restore_criu(cfg->image_dir);
    long dump_offset = get_insts_from_dir_name(cfg->image_dir) - (cfg->simpoint.points[cfg->simpoint.current] * cfg->process.ov_insts - cfg->process.ov_insts * (int)cfg->process.warmup_ratio);
    perf_child_pid = image_restore_criu();
    set_sched(perf_child_pid, cfg->process.affinity);
    // printf("restore child pid:%d\n", perf_child_pid);

    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    if (cfg->simpoint.points[cfg->simpoint.current] == 0 || cfg->process.warmup_ratio == 0) // no warmup at all
        pe.sample_period = cfg->process.ov_insts;
    else if (cfg->simpoint.points[cfg->simpoint.current] - (int)cfg->process.warmup_ratio <= 0) // shorten warmup
        pe.sample_period = cfg->simpoint.points[cfg->simpoint.current] * cfg->process.ov_insts - cfg->process.irq_offset;
    else // normal warmup
        pe.sample_period = cfg->process.ov_insts * (int)cfg->process.warmup_ratio - cfg->process.irq_offset - dump_offset;
    pe.wakeup_events = 100;
    perf_warmup_fd = perf_event_open(&pe, perf_child_pid, -1, -1, 0);
    if (perf_warmup_fd == -1)
    {
        PRINT_DEBUG_INFO;
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        kill(perf_child_pid, SIGTERM);
        return -1;
    }

    // signal handler:
    // Configure signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = warmup_event_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    // Setup signal handler
    if (sigaction(SIGIO, &sa, 0) < 0)
    {
        perror("sigaction");
        kill(perf_child_pid, SIGTERM);
        exit(-1);
    }

    // Setup event handler for overflow signals
    fcntl(perf_warmup_fd, F_SETFL, O_NONBLOCK | O_ASYNC);
    fcntl(perf_warmup_fd, __F_SETSIG, SIGIO);
    fcntl(perf_warmup_fd, F_SETOWN, getpid());

    ioctl(perf_warmup_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_warmup_fd, PERF_EVENT_IOC_ENABLE, 0);

    if (cfg->simpoint.points[cfg->simpoint.current] == 0 || cfg->process.warmup_ratio == 0) // no warmup at all
    {
        if (sigaction(SIGUSR1, &sa, 0) < 0)
        {
            perror("sigaction");
            kill(perf_child_pid, SIGTERM);
            exit(-1);
        }
        raise(SIGUSR1);
    }

    kill(perf_child_pid, SIGCONT);

    // return after test finish
    waitpid(perf_child_pid, NULL, 0);
}

long get_insts_from_dir_name(char *path)
{
    int start_idx = strlen(path);
    for (; path[start_idx] != '/'; --start_idx)
        ;
    return atol(path + start_idx + 1);
}