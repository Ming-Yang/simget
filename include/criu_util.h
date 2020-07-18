#ifndef __CRIU_UTIL
#define __CRIU_UTIL
#include <criu/criu.h>
#include <unistd.h>

void criu_perror(int);

int set_image_dump_criu(pid_t pid, const char *);
int set_image_restore_criu(const char *);
int image_dump_criu(pid_t);
pid_t image_restore_criu();


#endif //__CRIU_UTIL