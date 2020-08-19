#ifndef __UTIL_H
#define __UTIL_H

#define MAX_ARGS 128

typedef struct
{
    char *path;
    char *filename;
    char **args;
    int input_from_file;
    char *file_in;
    char *path_file; //used for execv
    char **argv; //used for execv
    long long ov_insts;
    int affinity;
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
void set_sched(pid_t, int);
void redirect_io(DumpCfg *);

#endif // __UTIL_H