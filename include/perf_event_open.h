#ifndef __PERF_EVENT_OPEN
#define __PERF_EVENT_OPEN

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                  group_fd, flags);
    return ret;
}

#endif // __PERF_EVENT_OPEN