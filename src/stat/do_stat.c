#include <sys/stat.h>
#include <strbuild.h>
#include <errno.h>

char* get_readable_mode(mode_t mode) {
	strbuild_t sb = sb_new();
	if(!sb.str) {
		errno = 12; // ENOMEM //	
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