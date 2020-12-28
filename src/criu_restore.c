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
    int dir_fd = set_image_restore_criu("criu_logs");
    pid_t pid = image_restore_criu();

    printf("restore child pid:%d\n", pid);
    waitpid(pid, NULL, 0);
}