#ifndef __UTIL_H
#define __UTIL_H

#define MAX_ARGS 128
#define MAX_PATH_LEN 512
#define MAX_JSON_BUFFER 100000

typedef struct
{
    char *path;
    char *filename;
    char **args;
    int input_from_file;
    char *file_in;
    char *path_file; //used for execv
    char **argv;     //used for execv
    long ov_insts;
    int irq_offset;
    double warmup_ratio;
    int affinity;
    int perf_fd;
    int child_pid;
} PerfCfg;

typedef struct
{
    char *out_file;
} LoopCfg;

typedef struct
{
    int k;
    int *points;
    double *weights;
} SimpointCfg;

typedef struct
{
    char *image_dir;
    PerfCfg process;
    LoopCfg loop;
    SimpointCfg simpoint;
} DumpCfg;

void print_long(long);
void print_dump_cfg(DumpCfg *const);
DumpCfg *get_cfg_from_json(const char *);
void set_sched(pid_t, int);
void redirect_io(DumpCfg *);

char *nstrjoin(int, ...);
char *long2string(long);

#endif // __UTIL_H