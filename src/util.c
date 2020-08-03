#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cJSON.h>

void print_long(long long val)
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

void print_dmp_cfg(DumpCfg *const cfg)
{
    printf("image dir:%s\n", cfg->image_dir);
    printf("process path:%s\n", cfg->process.path);

    printf("process argv:");
    for (int i = 0;; ++i)
    {
        printf("%s ", cfg->process.args[i]);
        if (!strncmp(cfg->process.args[i], "NULL", 4)) // == "NULL" is incorrect!!!
        {
            printf("\n");
            break;
        }
    }
    printf("overflow trigger insts:");
    print_long(cfg->process.ov_insts);
    printf("process pid:%d\n", cfg->process.child_pid);
    printf("process fd:%d\n", cfg->process.perf_fd);
}

DumpCfg *get_cfg_from_json(const char *json_path)
{
    char buff[1000];
    int fd = open(json_path, O_RDONLY);
    if (fd < 0)
    {
        perror("json cfg file read failed!");
    }
    int read_bytes = read(fd, buff, 1000);
    if (read_bytes <= 0)
    {
        perror("empty json config file");
    }

    cJSON *cfg = cJSON_Parse(buff);

    cJSON *image_dir = cJSON_GetObjectItem(cfg, "image_dir");
    cJSON *process = cJSON_GetObjectItem(cfg, "process");
    cJSON *process_path = cJSON_GetObjectItem(process, "path");
    cJSON *process_args = cJSON_GetObjectItem(process, "args");
    cJSON *process_arg = process_args->child;
    cJSON *process_ov_insts = cJSON_GetObjectItem(process, "ov_insts");
    cJSON *process_pid = cJSON_GetObjectItem(process, "pid");

    if (image_dir == NULL)
    {
        fprintf(stderr, "no image dir settings");
        return NULL;
    }
    if (process_path == NULL || process_args == NULL || process_ov_insts == NULL)
        printf("dump-configuration is not satisfied");

    DumpCfg *dmp_cfg = (DumpCfg *)malloc(sizeof(DumpCfg));
    dmp_cfg->process.args = (char **)calloc(MAX_ARGS, sizeof(char **));

    dmp_cfg->image_dir = image_dir->valuestring;

    if (process_path != NULL)
        dmp_cfg->process.path = process_path->valuestring;

    for (int i = 0; process_arg != NULL && i < MAX_ARGS; ++i)
    {
        dmp_cfg->process.args[i] = process_arg->valuestring;
        process_arg = process_arg->next;
    }

    if (process_ov_insts != NULL)
        dmp_cfg->process.ov_insts = atoll(process_ov_insts->valuestring);

    if (process_pid != NULL)
        dmp_cfg->process.child_pid = process_pid->valueint;

    return dmp_cfg;
}
