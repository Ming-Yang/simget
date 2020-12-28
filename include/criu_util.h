#ifndef __CRIU_UTIL
#define __CRIU_UTIL
#include <criu/criu.h>
#include <unistd.h>

void criu_perror(int);

int set_image_dump_criu(pid_t pid, const char *, bool);
int set_image_restore_criu(const char *);
int image_dump_criu(pid_t, int);
pid_t image_restore_criu(int);


#endif //__CRIU_UTIL