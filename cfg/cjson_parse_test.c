
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cjson/cJSON.h>
#include <util.h>

int main(int argc, char **argv)
{
    char buff[1000];
    int fd = open(argv[1], O_RDONLY);
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
    
    print_dump_cfg(dmp_cfg);
}
