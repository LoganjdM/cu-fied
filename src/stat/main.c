#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "../app_info.h"
#include "../colors.h"
#include "do_stat.h"

#ifndef C23
#	define u(n) int
#else
#	define u(n) _BitInt(n)
#endif

typedef u(2) args_t;
#define ARG_NO_NERDFONT 0b1

bool parse_argument(const char* arg, args_t* args) {
	(void)args;
	if (arg[0] != '-') return false;

	bool should_exit = 0;
	size_t arg_len = strlen(arg);
	for (size_t i=0; i<arg_len; ++i) {
		switch (arg[i]) {
			// EXITING ARGS //
			case 'h':
				#ifdef __has_embed
				const char help[] = {
				#	embed "help.txt"
				};
				#else
				const char help[] = "Touchf was not compiled with the C23 standard and could not embed the help message!\n";
				#endif
				puts(help);
				should_exit = 1; break;
			case 'v':
				puts(__CU_FIED_VERSION__);
				should_exit = 1; break;
			case 'b':
				puts("This software is in honor of my trash bin, tim, who was destroyed in a tornado during the writing of this software. :<\n");
				should_exit = 1; break;
			// NON-EXITING ARGS //
			case 'f':
				*args |= ARG_NO_NERDFONT; break;
		}
	}

	if (should_exit) exit(0);
	return true;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf_color(stderr, YELLOW, "You need to provide at least one file argument! (check -h?)\n");
		return 1;
	}

	struct stat st = {0};
	args_t args = 0;
	for (int i=1; i<argc; ++i) {
		#define ARG argv[i]
		if (parse_argument(ARG, &args)) continue;

		// ERRORS in stat(2) //
		if (stat(ARG, &st) == -1) {
			switch (errno) {
				case 13: // EACCESS //
					printf_color(stderr, YELLOW, "Failed to access file! (do you have permission?)\n"); break;
				case 40: // ELOOP //
					printf_color(stderr, RED, "Encountered too many symbolic links!\n"); break;
				default: // ENOTDIR, ENOENT, & ENAMETOOLONG //
					printf_color(stderr, YELLOW, "Failed to access file! (Is the name valid?)\n");
			} continue;
		}

		if (!(args & ARG_NO_NERDFONT)) {
			if (!S_ISDIR(st.st_mode)) printf(" ");
			else printf_color(stdout, BLUE, " ");
		}
		
		printf("%s\n", ARG);
		printf("\tSize: %zu\tBlocks: %zu\n", st.st_size, st.st_blocks);
		
		char* hr_mode = get_readable_mode(st.st_mode);
		if (hr_mode) {
			printf("\tMode: ( %d / %s )\n", st.st_mode, hr_mode);
			free(hr_mode);
		} else {
			printf_color(stderr, YELLOW, "\t Failed to allocate memory for human readable mode!");
			printf(" mode: (%d)\n", st.st_mode);
		}

		#undef ARG		
	}
	return 0;
}