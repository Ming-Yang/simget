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
#include <unistd.h>
#include "criu_util.h"

int main()
{
    int criu_err=0;
    criu_err= criu_init_opts();
    if(criu_err < 0)
    {
        criu_perror(criu_err);
        exit(-1);
    }
        
    int pid = fork();
    if(pid < 0)
    {
        perror("fork failed");
        exit(-1);
    }
    else if(pid ==0) // child
    {
        raise(SIGSTOP);
        execl("./test", "test", NULL);
    }
    else
    {
        waitpid(pid, NULL, WUNTRACED);
        
        criu_set_pid(pid);
        criu_set_log_file("dump.log");
        criu_set_log_level(4);

        int fd=open("./criu_logs", O_DIRECTORY|O_CREAT);
        if(fd<0)
        {
            perror("open dir failed");
            exit(-1);
        }
        criu_set_images_dir_fd(fd);
        
        criu_set_shell_job(true);

        kill(pid, SIGCONT);
        sleep(1);

        criu_err = criu_dump();
        if(criu_err<0)
        {
            criu_perror(criu_err);
            kill(pid, SIGTERM);
            exit(-1);
        }
        printf("criu dump pid %d finish\n", pid);
    }
}