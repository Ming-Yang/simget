#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "criu_util.h"

void criu_perror(int ret)
{
	/* NOTE: errno is set by libcriu */
	switch (ret)
	{
	case -EBADE:
		perror("RPC has returned fail");
		break;
	case -ECONNREFUSED:
		perror("Unable to connect to CRIU");
		break;
	case -ECOMM:
		perror("Unable to send/recv msg to/from CRIU");
		break;
	case -EINVAL:
		perror("CRIU doesn't support this type of request."
			   "You should probably update CRIU");
		break;
	case -EBADMSG:
		perror("Unexpected response from CRIU."
			   "You should probably update CRIU");
		break;
	default:
		perror("Unknown error type code."
			   "You should probably update CRIU");
	}
}

int set_image_dump_criu(pid_t pid, const char *image_dir, bool cont)
{
	int criu_err = 0;
    mkdir(image_dir, 0777);
	int fd = open(image_dir, O_DIRECTORY);
	if (fd < 0)
	{
		perror("image dir open failed");
		kill(pid, SIGTERM);
		exit(-1);
	}

	criu_err = criu_init_opts();
	if (criu_err < 0)
	{
		criu_perror(criu_err);
		kill(pid, SIGTERM);
		exit(criu_err);
	}

	criu_set_pid(pid);
	criu_set_log_file("dump.log");
	criu_set_log_level(4);
	criu_set_images_dir_fd(fd);
	criu_set_leave_running(cont);

	return fd;
}

int set_image_restore_criu(const char *image_dir)
{
	int criu_err = 0;
	int fd = open(image_dir, O_DIRECTORY);
	if (fd < 0)
	{
		perror("image dir open failed");
		exit(-1);
	}

	criu_err = criu_init_opts();
	if (criu_err < 0)
	{
		criu_perror(criu_err);
		exit(criu_err);
	}

	criu_set_log_file("restore.log");
	criu_set_log_level(4);
	criu_set_images_dir_fd(fd);

	return fd;
}

int image_dump_criu(pid_t pid, int dir_fd)
{
	int criu_err = criu_dump();
	if (criu_err < 0)
	{
		criu_perror(criu_err);
		kill(pid, SIGTERM);
		exit(-1);
	}
	close(dir_fd);

	return 0;
}

pid_t image_restore_criu(int dir_fd)
{
	pid_t pid = criu_restore_child();
    if (pid < 0)
    {
        fprintf(stderr, "restore failed");
		criu_perror(pid);
		exit(-1);
    }
	close(dir_fd);

	return pid;
}
