#include "criu_util.h"
#include "util.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <wait.h>

int main(int argc, char** argv)
{
    DumpCfg *cfg = get_cfg_from_json(argv[1]);
    set_image_restore_criu(cfg->image_dir);
    pid_t pid = image_restore_criu();
    printf("restore child pid:%d\n", pid);
    kill(pid, SIGCONT);
    waitpid(pid, NULL, 0);
}