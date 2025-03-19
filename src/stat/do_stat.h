#ifndef _SYS_STAT_H
#	include <sys/stat.h>
#endif

char* get_readable_mode(mode_t mode);
char* get_readable_time(struct timespec st_tim);