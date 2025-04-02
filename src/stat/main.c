#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include "../f_ext_map.c"
#include "../app_info.h"
#include "../libcolor/colors.h"
#include "do_stat.h"

#ifndef C23
#	define u(n) int
#else
#	define u(n) _BitInt(n)
#endif

typedef u(2) args_t;
#define ARG_NO_NERDFONT 0b1

args_t parse_argv(int argc, char** argv) {
	args_t result = 0b0;

	bool print_help = false, print_vers = false, print_bin = false;
	for (int i=1; i<argc; ++i) {
		#define ARG argv[i]
		if (ARG[0] != '-') continue;
		
		if (!strcmp(ARG, "--help")) {
			print_help = true;
			continue;
		} if (!strcmp(ARG, "--version")) {
			print_vers = true;
			continue;
		} if (!strcmp(ARG, "--bin")) {
			print_bin = true;
			continue;
		} if (!strcmp(ARG, "--no-nerdfonts")) {
			result |= ARG_NO_NERDFONT;
			continue;
		}

		for (size_t j=1; j<strlen(ARG); ++j) {
			switch (ARG[j]) {
				case 'h':
					print_help = true;
					break;
				case 'v':
					print_vers = true;
					break;
				case 'b':
					print_bin = true;
					break;
				case 'f':
					result |= ARG_NO_NERDFONT;
					break;
				default:
					fprintf_color(stderr, YELLOW, "Unknown argument: %s\n", ARG);
					return result;
			}
		}
	}

	if (print_help) {
		#ifdef __has_embed
		const char help[] = {
		#	embed "help.txt"
		};
		#else
		const char help[] = "Touchf was not compiled with the C23 standard and could not embed the help message!\n";
		#endif
		puts(help);
	}
	if (print_vers) puts("Cu-Fied version: "__CU_FIED_VERSION__);
	if (print_bin) puts("This software is in honor of my trash bin, tim, who was destroyed in a tornado during the writing of this software. :<");

	if (print_help || print_vers || print_bin) exit(0);
	return result;
}

void print_gid_uid(const struct stat st) {
	struct passwd* pws = getpwuid(st.st_uid);
	if (!pws) {
		assert(errno != ERANGE);
		printf("\t");
		switch (errno) {
			// see signal(7) //
			case EINTR:
				fprintf_color(stderr, YELLOW, "Caught an OS signal! ");
				break;
			case EIO:
				fprintf_color(stderr, YELLOW, "General I/O error! ");
				break;
			case EMFILE:
				fprintf_color(stderr, YELLOW, "Hit the open file descriptor limit! ");
				break;
			case ENFILE:
				fprintf_color(stderr, YELLOW, "Hit the system-wide open file descriptor limit! ");
				break;
			case ENOMEM:
				fprintf_color(stderr, YELLOW, "Didn't have enough memory to get Uid info! ");
				break;
			default: // 0 or ENOENT or ESRCH or EBADF or EPERM ... //
				fprintf_color(stderr, YELLOW, "Couldn't find info on the file's Uid! "); // should not happen //
		} printf(" Uid: (%d)\t", st.st_uid);
	} else printf("\tUid: (%d/%s)\t", st.st_uid, pws->pw_name);
	
	struct group* grp = getgrgid(st.st_gid);
	if (!grp) {
		assert(errno != ERANGE);
		switch (errno) {
			// see signal(7) //
			case EINTR:
				fprintf_color(stderr, YELLOW, "Caught an OS signal! ");
				break;
			case EIO:
				fprintf_color(stderr, YELLOW, "General I/O error! ");
				break;
			case EMFILE:
				fprintf_color(stderr, YELLOW, "Hit the open file descriptor limit! ");
				break;
			case ENFILE:
				fprintf_color(stderr, YELLOW, "Hit the system-wide open file descriptor limit! ");
				break;
			case ENOMEM:
				fprintf_color(stderr, YELLOW, "Didn't have enough memory to get Gid info! ");
				break;
			default: // 0 or ENOENT or ESRCH or EBADF or EPERM ... //
				fprintf_color(stderr, YELLOW, "Couldn't find info on the file's Gid! "); // should not happen //
		} printf(" Gid: (%d)\n", st.st_gid);
	} else printf("Gid: (%d/%s)\n", st.st_gid, grp->gr_name);

}

bool print_nerdfont(const char* operand, const struct stat st, table_t* f_ext_map) {
	char* operand_copy = malloc(strlen(operand) + 1);
	if (!operand_copy) return false;
	strcpy(operand_copy, operand);
	
	char* tok = strtok(operand_copy, ".");
	char* ext = NULL;
	while (tok) {
		ext = tok;
		tok = strtok(NULL, ".");
	} 

	char* nerdfont = f_ext_map->get(f_ext_map, ext).s;
	if (!nerdfont) {
		if (S_ISDIR(st.st_mode)) printf_color(BLUE, "%s", " ");
		else printf(" ");
		
		free(operand_copy);
		return true;
	}
	
	if (S_ISDIR(st.st_mode)) printf_color(BLUE, "%s", nerdfont);
	else printf("%s", nerdfont);
	free(operand_copy);
	return true;
}

#define PRINTLN_READABLE_TIME(tm_struct) \
	if(!(args & ARG_NO_NERDFONT)) printf(" 󰃭"); \
	printf(": "); \
	printf("%s", get_readable_time(tm_struct))


int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf_color(stderr, YELLOW, "You need to provide at least one file argument! (check -h?)\n");
		return 1;
	}

	args_t args = parse_argv(argc, argv);
	table_t* f_ext_map = NULL;
	if (!(args & ARG_NO_NERDFONT) && !(f_ext_map = init_filetype_dict())) {
		fprintf_color(stderr, RED, "Failed to allocate memory for nerdfonts based on file extensions!\n");
		return 2;
	}
	
	struct stat st = {0};
	for (int i=1; i<argc; ++i) {
		#define OPERAND argv[i]
		if (ARG[0] == '-') continue;

		// ERRORS in stat(2) //
		if (stat(OPERAND, &st) == -1) {
			switch (errno) {
				case EACCES:
					fprintf_color(stderr, YELLOW, "Failed to access file! (do you have permission?)\n"); break;
				case ELOOP:
					fprintf_color(stderr, RED, "Encountered too many symbolic links!\n"); break;
				default: // ENOTDIR, ENOENT, & ENAMETOOLONG //
					fprintf_color(stderr, YELLOW, "Failed to access file! (Is the name valid?)\n");
			} continue;
		}

		if (!(args & ARG_NO_NERDFONT)) {
			if(!print_nerdfont(OPERAND, st, f_ext_map))
				fprintf_color(stderr, YELLOW, "Failed to allocate memory for getting the nerdfont!\n");
		}
		
		if (!S_ISDIR(st.st_mode)) printf("%s\n", OPERAND);
		else printf_color(BLUE, "%s\n", OPERAND);

		#ifdef __APPLE__
		printf("\tSize: %lli\tBlocks: %lli\tI/O Block: %i\n", st.st_size, st.st_blocks, st.st_blksize);
		#else
		printf("\tSize: %zu\tBlocks: %zu\tI/O Block: %zu\n", st.st_size, st.st_blocks, st.st_blksize);
		#endif
		
		char* hr_mode = get_readable_mode(st.st_mode);
		if (hr_mode) {
			printf("\tMode: (%d/%s)", st.st_mode, hr_mode);
			free(hr_mode);
		} else {
			fprintf_color(stderr, YELLOW, "\t Failed to allocate memory for human readable mode!\n");
			printf("\tMode: (%d)", st.st_mode);
		}

		#ifdef __APPLE__
		printf("\tDevice ID: %i\tInode: %llu\tLinks: %u\n", st.st_dev, st.st_ino, st.st_nlink);
		#else
		printf("\tDevice ID: %zu\tInode: %zu\tLinks: %zu\n", st.st_dev, st.st_ino, st.st_nlink);
		#endif
		
		print_gid_uid(st);			

		// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html //
		#ifdef __APPLE__
		printf("\tAccess");
		PRINTLN_READABLE_TIME(st.st_atimespec);
		printf("\tModify");
		PRINTLN_READABLE_TIME(st.st_mtimespec);
		printf("\tCreation");
		PRINTLN_READABLE_TIME(st.st_ctimespec);
		#else
		printf("\tAccess");
		PRINTLN_READABLE_TIME(st.st_atim);
		printf("\tModify");
		PRINTLN_READABLE_TIME(st.st_mtim);
		printf("\tCreation");
		PRINTLN_READABLE_TIME(st.st_ctim);
		#endif
		
		#undef OPERAND		
	}
	return 0;
}