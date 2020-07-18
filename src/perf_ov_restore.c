#include "criu_util.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <wait.h>

const char *image_dir = "./dump";

int main()
{
    set_image_restore_criu(image_dir);
    pid_t pid = image_restore_criu();
    printf("restore child pid:%d\n", pid);
    kill(pid, SIGCONT);
    waitpid(pid, NULL, 0);
}