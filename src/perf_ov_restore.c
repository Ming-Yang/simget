#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <wait.h>
#include "criu_util.h"
#include "util.h"
#ifdef _DEBUG
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "perf_event_open.h"
#endif

int main(int argc, char **argv)
{
    DumpCfg *cfg = get_cfg_from_json(argv[1]);

    set_image_restore_criu(cfg->image_dir);
#ifdef _DEBUG
    struct timeval start,end;
    gettimeofday(&start, NULL);
#endif
    pid_t pid = image_restore_criu();
#ifdef _DEBUG
    gettimeofday(&end, NULL);
    printf("restore time:%f s\n", ((double)(end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec))/1000000);
#endif
    set_sched(pid, cfg->process.affinity);
    printf("restore child pid:%d\n", pid);
#ifdef _DEBUG
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    cfg->process.child_pid = pid;
    int perf_open_fd = perf_event_open(&pe, pid, -1, -1, 0);
    if (perf_open_fd == -1)
    {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        kill(pid, SIGTERM);
        return -1;
    }
    ioctl(perf_open_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_open_fd, PERF_EVENT_IOC_ENABLE, 0);
#endif

    kill(pid, SIGCONT);
    waitpid(pid, NULL, 0);
#ifdef _DEBUG
    long inst_counts = 0;
    if (read(perf_open_fd, &inst_counts, sizeof(long)) == -1)
    {
        fprintf(stderr, "read perf event empty!\n");
    }
    else
    {
        printf("total inst counts:");
        print_long(inst_counts);
    }

    close(perf_open_fd);
#endif
}