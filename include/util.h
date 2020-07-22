#ifndef __UTIL_H
#define __UTIL_H

#define MAX_ARGS 128

typedef struct
{
    char *path;
    char **args;
    long long ov_insts;
    int perf_fd;
    int child_pid;
}PerfCfg;

typedef struct
{
    char *image_dir;
    PerfCfg process;
}DumpCfg;

void print_long(long long);
void print_dmp_cfg(DumpCfg* const);
DumpCfg *get_cfg_from_json(const char *);

#endif // __UTIL_H