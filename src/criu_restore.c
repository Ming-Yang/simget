#include <criu/criu.h>
#include <signal.h>
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
#include <fcntl.h>
#include "criu_util.h"

int main()
{
    int criu_err=0;
    criu_err = criu_init_opts();
    if (criu_err < 0)
    {
        criu_perror(criu_err);
        exit(-1);
    }
    criu_set_log_level(4);
    criu_set_log_file("restore.log");

    int fd = open("./criu_logs", O_DIRECTORY|O_CREAT);
    if (fd < 0)
    {
        perror("open dir failed");
        exit(-1);
    }
    criu_set_images_dir_fd(fd);

    criu_set_shell_job(true);

    int pid = criu_restore_child();
    if (pid < 0)
    {
        perror("restore failed!");
        exit(-1);
    }

    printf("restore child pid:%d\n", pid);
    waitpid(pid, NULL, 0);
}