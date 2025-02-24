#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>

uint16_t move_args = 0b0;
#define MOVE_ARG_RECURSIVE 0b1

uint16_t copy(const char* src, const char* dest) {
	uint16_t files_copied = 0;
	struct stat st;
	// stat sets errno, `man 'stat(2)'` //
	if(stat(src, &st)==-1) {
		return files_copied;
	} else {
		struct stat exists;
		if(stat(dest, &exists)) {
			errno = EEXIST;
			return files_copied;
		}
	}
	
	if(S_ISDIR(st.st_mode) && !(move_args & MOVE_ARG_RECURSIVE)) {
		if(!(move_args & MOVE_ARG_RECURSIVE)) {
			errno = EINVAL;
			return files_copied;
		}
		errno = ENOSYS;
		return files_copied;
	}

	// if this fails errno will be set //
	if(rename(src, dest)!=-1) {
		files_copied++;
	}
	return files_copied;
}