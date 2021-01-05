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
#include <ctype.h>
#include "perf_event_open.h"
#include "util.h"
#include "criu_util.h"

DumpCfg *cfg;
int perf_inst_fd;
int perf_child_pid;
int points_idx = 0; // current dump point index
int dump_fire = 0; // 1 if instructions counter overflow, set to 0 after current point is dumped
int start_from_idx = 0;// setting dump point index
bool continuous_loop = true;

static void perf_event_handler(int signum, siginfo_t *info, void *ucontext)
{
    kill(perf_child_pid, SIGSTOP);
    waitpid(perf_child_pid, NULL, WUNTRACED);
    ioctl(perf_inst_fd, PERF_EVENT_IOC_DISABLE, 0);

    if (info->si_code != POLL_IN) // Only POLL_IN should happen.
    {
        perror("wrong signal\n");
        exit(-1);
    }

    ++points_idx;
    if (dump_fire == 0)
    {
        dump_fire = 1;
    }
    else
    {
        printf("dump fire again!\n");
    }
}

/**
 * @param .json config file name 
 * @param index from which start dump 
 * @param enable loop dump or not
**/
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("need dump config json file\n");
        return -1;
    }
    cfg = get_cfg_from_json(argv[1]);
    if (cfg->simpoint.k == 0)
    {
        fprintf(stderr, "no simpoint points\n");
        return -1;
    }
    if (argc >= 3)
    {
        start_from_idx = atoi(argv[2]);
        points_idx = points_idx < start_from_idx ? start_from_idx : points_idx;
    }
    if (argc >= 4)
    {
        continuous_loop = atoi(argv[3]);
        if (continuous_loop)
            printf("continuous loop dump\n");
        else
            printf("seperate loop dump\n");
    }
#ifdef _DEBUG
    printf("get config finish\n");
    print_dump_cfg(cfg);
#endif

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

        raise(SIGSTOP); // wait for parent process run first
        execv(cfg->process.path_file, cfg->process.argv);
    }
    else //parent
    {
        waitpid(perf_child_pid, NULL, WUNTRACED); // wait for child process stop
#ifdef _DEBUG
        printf("child pid %d\n", perf_child_pid);
#endif

        struct perf_event_attr pe_insts;
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

        // if warmup cause the start point earlier than the beginning of the process,
        // eg. start from 1 million instrutions, but need warmup 2 million instructions
        if (cfg->simpoint.points[points_idx] - (int)cfg->process.warmup_ratio <= 0)
        {
            int warmup_start = 0;
            for (; warmup_start < cfg->simpoint.k; ++warmup_start)
            {
                if (cfg->simpoint.points[warmup_start] - (int)cfg->process.warmup_ratio > 0)
                    break;
            }
            points_idx = warmup_start;
            printf("first %d actual insts:%d\n====================\n", points_idx, 0);
            int dir_fd = set_image_dump_criu(perf_child_pid, nstrjoin(2, cfg->image_dir, "/0"), true);
            image_dump_criu(perf_child_pid, dir_fd);
        }

        pe_insts.sample_period = (cfg->simpoint.points[points_idx] - (int)cfg->process.warmup_ratio) * cfg->process.ov_insts - cfg->process.irq_offset;
        pe_insts.wakeup_events = 100;

        perf_inst_fd = perf_event_open(&pe_insts, perf_child_pid, -1, -1, 0);
        if (perf_inst_fd == -1)
        {
            fprintf(stderr, "Error opening leader %llx\n", pe_insts.config);
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

        // resume child process
        ioctl(perf_inst_fd, PERF_EVENT_IOC_RESET, 0);
        kill(perf_child_pid, SIGCONT);

        while (points_idx < cfg->simpoint.k)
        {
            // wait for instruction counter overflow
            pause();
            if (dump_fire)
            {
                long inst_counts = 0;
                if (read(perf_inst_fd, &inst_counts, sizeof(long)) == -1)
                {
                    fprintf(stderr, "read perf inst empty!\n");
                    kill(perf_child_pid, SIGTERM);
                    exit(-1);
                }

                printf("%d actual insts:%ld\n====================\n", points_idx - 1, inst_counts);
                int dir_fd = set_image_dump_criu(perf_child_pid, nstrjoin(3, cfg->image_dir, "/", long2string(inst_counts)), false);
                image_dump_criu(perf_child_pid, dir_fd);

                if (continuous_loop)
                {
                    // update perf_event period
                    long new_period = (cfg->simpoint.points[points_idx] - (int)cfg->process.warmup_ratio) * cfg->process.ov_insts - cfg->process.irq_offset - inst_counts;
                    ioctl(perf_inst_fd, PERF_EVENT_IOC_PERIOD, &new_period);
                    ioctl(perf_inst_fd, PERF_EVENT_IOC_ENABLE, 0);
                }
                else if (!continuous_loop && points_idx < cfg->simpoint.k)
                {
                    close(perf_inst_fd);

                    long new_period = (cfg->simpoint.points[points_idx] - (int)cfg->process.warmup_ratio) * cfg->process.ov_insts - cfg->process.irq_offset;
                    pe_insts.sample_period = new_period;
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
                    waitpid(perf_child_pid, NULL, WUNTRACED);
                    printf("child pid %d\n", perf_child_pid);
                    perf_inst_fd = perf_event_open(&pe_insts, perf_child_pid, -1, -1, 0);
                    if (perf_inst_fd == -1)
                    {
                        fprintf(stderr, "Error opening leader %llx\n", pe_insts.config);
                        kill(perf_child_pid, SIGTERM);
                        return -1;
                    }

                    fcntl(perf_inst_fd, F_SETFL, O_NONBLOCK | O_ASYNC);
                    fcntl(perf_inst_fd, __F_SETSIG, SIGIO);
                    fcntl(perf_inst_fd, F_SETOWN, getpid());

                    ioctl(perf_inst_fd, PERF_EVENT_IOC_RESET, 0);
                }
                else
                {
                    // last point, continue running to child process finish
                    ioctl(perf_inst_fd, PERF_EVENT_IOC_ENABLE, 0);
                }

                dump_fire = 0;
                kill(perf_child_pid, SIGCONT);
            }
        }

        waitpid(perf_child_pid, NULL, 0);
        printf("finish loop dump\n");
        long inst_counts = 0, cycle_counts = 0;
        if (read(perf_inst_fd, &inst_counts, sizeof(long)) == -1)
        {
            fprintf(stderr, "read perf inst empty!\n");
        }
        else
        {
            printf("total insts:");
            print_long(inst_counts);
        }

        printf("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n\n");
        close(perf_inst_fd);
    }
}
