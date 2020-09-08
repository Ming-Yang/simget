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
#include "util.h"

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
        detach_from_shell(NULL);
        raise(SIGSTOP);
        execl("./test", "test", NULL);
    }
    else
    {
        waitpid(pid, NULL, WUNTRACED);
        
        set_image_dump_criu(pid, "criu_logs", false);

        kill(pid, SIGCONT);
        sleep(1);

        image_dump_criu(pid);

        printf("criu dump pid %d finish\n", pid);
    }
}