#include <sys/stat.h>
#include <strbuild.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

char* get_readable_mode(mode_t mode) {
	strbuild_t sb = sb_new();
	if(!sb.str) {
		errno = ENOMEM;	
		return NULL;
	}
	if (S_ISDIR(mode)) sb_append(&sb, "d");
	else sb_append(&sb, "-");
	
	#define APPEND_IXXXX(flag, hr_char) \
		if (mode & flag) sb_append(&sb, #hr_char); \
		else sb_append(&sb, "-")

	APPEND_IXXXX(S_IRUSR, r);
	APPEND_IXXXX(S_IWUSR, w);
	APPEND_IXXXX(S_IXUSR, x);

	APPEND_IXXXX(S_IRGRP, r);
	APPEND_IXXXX(S_IWGRP, w);
	APPEND_IXXXX(S_IXGRP, x);
	
	APPEND_IXXXX(S_IROTH, r);
	APPEND_IXXXX(S_IWOTH, w);
	APPEND_IXXXX(S_IXOTH, x);

	return sb.str;
}

// honestly could be a macro, ¯\_(ツ)_/¯ //
char* get_readable_time(const struct timespec st_tim) {
	struct tm* local_tm = localtime(&st_tim.tv_sec);
	// man page doesn't describe possible errors //
	assert(local_tm);
	
	// char buf[restrict 26] //
	char* t_buf = ctime(&local_tm->tm_sec);
	assert(errno != EOVERFLOW);
	return t_buf;
}