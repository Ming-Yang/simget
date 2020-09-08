#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <util.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include "cJSON.h"

void print_long(long val)
{
    int val_table[20];
    int table_eles = 0;
    while (val > 0)
    {
        val_table[table_eles] = val % 1000;
        val /= 1000;
        table_eles++;
    }
    for (int i = table_eles - 1; i >= 0; --i)
    {
        if (i == table_eles - 1)
            printf("%d", val_table[i]);
        else
            printf("%03d", val_table[i]);

        if (i != 0)
            printf(",");
    }
    printf("\n");
}

void print_dump_cfg(DumpCfg *const cfg)
{
    printf("=================== Dump Config Info ===================\n");
    printf("image dir:%s\n", cfg->image_dir);
    printf("process path:%s\n", cfg->process.path);
    printf("process file:%s\n", cfg->process.filename);

    printf("process argv:");
    for (int i = 0;; ++i)
    {
        if (cfg->process.argv[i] == NULL)
            break;
        printf("%s ", cfg->process.argv[i]);
    }
    if (cfg->process.input_from_file)
    {
        if (cfg->process.file_in != NULL)
            printf("\ninput from file:%s\n", cfg->process.file_in);
    }
    else
    {
        printf("\nno input file\n");
    }

    printf("overflow trigger insts:");
    print_long(cfg->process.ov_insts);
    printf("offset insts caused by irq:");
    print_long(cfg->process.irq_offset);
    printf("affinity:%d\n", cfg->process.affinity);

    printf("loop output file:%s\n", cfg->loop.out_file);

    printf("simpoints:%d\n", cfg->simpoint.k);
    for (int i = 0; i < cfg->simpoint.k; ++i)
    {
        printf("%d,%f\n", cfg->simpoint.points[i], cfg->simpoint.weights[i]);
    }

    printf("=================== Dump Config Info ===================\n");
}

void parse_cfg_path(DumpCfg *cfg)
{
    size_t m = strlen(cfg->process.path);
    size_t n = strlen(cfg->process.filename);
    size_t len = m + n;
    char *path_file = calloc(len + 2, sizeof(char));
    memset(path_file, '\0', len);
    path_file[m] = '/';
    strncpy(path_file, cfg->process.path, m);
    strncpy(path_file + m + 1, cfg->process.filename, n);
    cfg->process.path_file = path_file;

    char **argv = (char **)calloc(MAX_ARGS + 1, sizeof(char *));
    *argv = cfg->process.filename;
    for (int i = 0;; ++i)
    {
        argv[i + 1] = cfg->process.args[i];
        if (cfg->process.args[i] == NULL)
            break;
    }
    cfg->process.argv = argv;
}

DumpCfg *get_cfg_from_json(const char *json_path)
{
    char buff[MAX_JSON_BUFFER];
    int fd = open(json_path, O_RDONLY);
    if (fd < 0)
    {
        perror("json cfg file read failed!");
    }
    int read_bytes = read(fd, buff, MAX_JSON_BUFFER);
    if (read_bytes <= 0)
    {
        perror("empty json config file");
    }

    cJSON *cfg = cJSON_Parse(buff);

    cJSON *image_dir = cJSON_GetObjectItem(cfg, "image_dir");
    cJSON *process = cJSON_GetObjectItem(cfg, "process");
    cJSON *loop = cJSON_GetObjectItem(cfg, "loop");
    cJSON *simpoint = cJSON_GetObjectItem(cfg, "simpoint");

    cJSON *process_path = cJSON_GetObjectItem(process, "path");
    cJSON *process_filename = cJSON_GetObjectItem(process, "filename");
    cJSON *process_args = cJSON_GetObjectItem(process, "args");
    cJSON *input_from_file = cJSON_GetObjectItem(process, "input_from_file");
    cJSON *file_in = cJSON_GetObjectItem(process, "file_in");
    cJSON *process_affinity = cJSON_GetObjectItem(process, "affinity");
    cJSON *process_ov_insts = cJSON_GetObjectItem(process, "ov_insts");
    cJSON *process_irq_offset = cJSON_GetObjectItem(process, "irq_offset");
    cJSON *process_warmup_ratio = cJSON_GetObjectItem(process, "warmup_ratio");
    cJSON *process_pid = cJSON_GetObjectItem(process, "pid");

    cJSON *loop_out_file = cJSON_GetObjectItem(loop, "out_file");

    cJSON *simpoint_k = cJSON_GetObjectItem(simpoint, "k");
    cJSON *simpoint_current = cJSON_GetObjectItem(simpoint, "current");
    cJSON *simpoint_point = cJSON_GetObjectItem(simpoint, "points");
    cJSON *simpoint_weight = cJSON_GetObjectItem(simpoint, "weights");

    if (image_dir == NULL)
    {
        fprintf(stderr, "no image dir settings");
        return NULL;
    }
    if (process_path == NULL || process_filename == NULL || process_args == NULL || process_ov_insts == NULL)
        printf("dump-configuration is not satisfied");

    DumpCfg *dump_cfg = (DumpCfg *)malloc(sizeof(DumpCfg));
    dump_cfg->process.args = (char **)calloc(MAX_ARGS, sizeof(char *));

    dump_cfg->image_dir = image_dir->valuestring;

    if (process_path != NULL)
        dump_cfg->process.path = process_path->valuestring;

    if (process_filename != NULL)
        dump_cfg->process.filename = process_filename->valuestring;

    if (process_args != NULL)
    {
        cJSON *process_arg = process_args->child;
        int i = 0;
        for (; process_arg != NULL && i < MAX_ARGS; ++i)
        {
            dump_cfg->process.args[i] = process_arg->valuestring;
            process_arg = process_arg->next;
        }
        dump_cfg->process.args[i] = NULL;
    }

    if (input_from_file != NULL)
    {
        dump_cfg->process.input_from_file = input_from_file->valueint;
        if (dump_cfg->process.input_from_file)
        {
            if (file_in == NULL)
            {
                fprintf(stderr, "no suitable input file!\n");
            }
            else
            {
                dump_cfg->process.file_in = file_in->valuestring;
            }
        }
    }

    if (process_affinity != NULL)
        dump_cfg->process.affinity = process_affinity->valueint;
    else
        dump_cfg->process.affinity = 1;

    if (process_ov_insts != NULL)
        dump_cfg->process.ov_insts = atoll(process_ov_insts->valuestring);

    if (process_irq_offset != NULL)
        dump_cfg->process.irq_offset = process_irq_offset->valueint;

    if (process_warmup_ratio != NULL)
        dump_cfg->process.warmup_ratio = process_warmup_ratio->valuedouble;

    if (process_path != NULL && process_filename != NULL && process_args != NULL)
        parse_cfg_path(dump_cfg);

    if (loop_out_file != NULL)
        dump_cfg->loop.out_file = loop_out_file->valuestring;

    if (simpoint_k != NULL && simpoint_point != NULL && simpoint_weight != NULL)
    {
        cJSON *simpoint_point_item = simpoint_point->child;
        cJSON *simpoint_weight_item = simpoint_weight->child;

        dump_cfg->simpoint.k = simpoint_k->valueint;
        if (dump_cfg->simpoint.k != 0)
        {
            int *points = (int *)calloc(dump_cfg->simpoint.k, sizeof(int));
            double *weights = (double *)calloc(dump_cfg->simpoint.k, sizeof(double));
            for (int i = 0; i < dump_cfg->simpoint.k; ++i)
            {
                if (simpoint_point_item == NULL || simpoint_weight_item == NULL)
                    break;

                points[i] = simpoint_point_item->valueint;
                weights[i] = simpoint_weight_item->valuedouble;

                simpoint_point_item = simpoint_point_item->next;
                simpoint_weight_item = simpoint_weight_item->next;
            }
            dump_cfg->simpoint.points = points;
            dump_cfg->simpoint.weights = weights;
        }
    }
    else
    {
        dump_cfg->simpoint.k = 0;
    }
    if (simpoint_current != NULL)
        dump_cfg->simpoint.current = simpoint_current->valueint;

    return dump_cfg;
}

void free_dump_cfg(DumpCfg *cfg)
{
    free(cfg->process.argv);
    free(cfg->process.args);
    free(cfg->process.filename);
    free(cfg->process.path);

    free(cfg);
}

void set_sched(pid_t pid, int affinity)
{
    cpu_set_t cpu_set_cfg;
    CPU_ZERO(&cpu_set_cfg);
    CPU_SET(affinity, &cpu_set_cfg);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set_cfg) == -1)
    {
        perror("set sched_setaffinity");
        exit(-1);
    }
    // const struct sched_param sched = {
    //     .sched_priority = sched_get_priority_max(SCHED_FIFO)};

    // if (sched_setscheduler(pid, SCHED_FIFO, &sched) == -1)
    // {
    //     perror("set sched_setscheduler");
    //     exit(-1);
    // }
}

void detach_from_shell(DumpCfg *cfg)
{
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0)
        perror("open /dev/null failed!");

    if (cfg == NULL)
    {
        dup2(null_fd, STDIN_FILENO);
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
    }
    else
    {
        if (chdir(cfg->process.path) < 0)
        {
            perror("change dir failed");
        }

        if (cfg->process.input_from_file)
        {
            int in_fd = open(cfg->process.file_in, O_RDONLY);
            if (in_fd < 0)
                perror("open child process input file failed!");

            dup2(in_fd, STDIN_FILENO);
        }
        else
        {
            dup2(null_fd, STDIN_FILENO);
        }

        size_t l = strlen(cfg->process.filename);
        char *out_file = calloc(l + 5, sizeof(char));
        strncpy(out_file, cfg->process.filename, l);
        strncpy(out_file + l, ".out", 4);

        char *err_file = calloc(l + 5, sizeof(char));
        strncpy(err_file, cfg->process.filename, l);
        strncpy(err_file + l, ".err", 4);

        int out_fd = open(out_file, O_RDWR | O_CREAT, 0666);
        int err_fd = open(err_file, O_RDWR | O_CREAT, 0666);
        if (out_fd < 0)
        {
            perror("open child process out file failed, redirect to /dev/null");
            dup2(null_fd, STDOUT_FILENO);
        }
        else
        {
            dup2(out_fd, STDOUT_FILENO);
        }

        if (err_fd < 0)
        {
            perror("open child process err file failed,redirect to /dev/null");
            dup2(null_fd, STDERR_FILENO);
        }
        else
        {
            dup2(err_fd, STDERR_FILENO);
        }

        free(out_file);
        free(err_file);
    }

    if(setsid() < 0)
    {
        perror("fail to set session ID");
    }
}

char *long2string(long num)
{
    int n = 1;
    long val = num;
    for (; val > 0; ++n)
        val /= n;
    char *str = (char *)calloc(n, sizeof(n));
    sprintf(str, "%ld", num);
    return str;
}

char *nstrjoin(int num, ...)
{
    va_list args;
    size_t len = 0;
    const char *arg_ele = NULL;

    va_start(args, num);
    for (int i = 0; i < num; ++i)
    {
        arg_ele = va_arg(args, const char *);
        len += strlen(arg_ele);
    }
    va_end(args);

    char *s = (char *)calloc(len + 1, sizeof(char));
    char *p = s;

    va_start(args, num);
    for (int i = 0; i < num; ++i)
    {
        arg_ele = va_arg(args, const char *);
        int l = strlen(arg_ele);
        strncpy(p, arg_ele, l);
        p += l;
    }
    va_end(args);

    *p = '\0';
    return s;
}