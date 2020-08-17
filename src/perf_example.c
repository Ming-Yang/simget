#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sched.h>


#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                            } while (0)

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                    int cpu, int group_fd, unsigned long flags)
{
        int ret;
        ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                                group_fd, flags);
        return ret;
}


int
main(int argc, char **argv)
{
    struct perf_event_attr pe;
    long long count;
    int fd;
    long long cc[10] = {0};
#if 1
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(7, &set);
    if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
        errExit("sched_setaffinity");
    
    const struct sched_param sched = {
        .sched_priority = sched_get_priority_max(SCHED_FIFO)
    };
    sched_setscheduler(getpid(), SCHED_FIFO, &sched);
#endif

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
       fprintf(stderr, "Error opening leader %llx\n", pe.config);
       exit(EXIT_FAILURE);
    }
    // syscall(5333);
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    
    for(long i= 10000000000;i>0;--i) 
    {
    	__asm__ volatile("nop\n");
    }

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    // syscall(5334);
    read(fd, &count, sizeof(long long));

    printf("Used %lld instructions\n", count);

    close(fd);
}
