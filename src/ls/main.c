#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
// this is not how to properly see if this is C23 but oh well //
#ifndef C23
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "../app_info.h"

#define REALLOCARRAY_IMPLEMENTATION
#include "../polyfill.h"

// I wish C had zig packed structs //
#ifndef C23
typedef int args_t;
#else
typedef _BitInt(8) args_t;
#endif

// bool //
#define ARG_DOT_DIRS     0b1
#define ARG_DOT_FILES    0b10
#define ARG_UNSORTED     0b100
#define ARG_NO_NERDFONTS 0b1000
#define ARG_STAT         0b10000
#define ARG_DIR_CONTS    0b100000
// u2 //
#define HR_ARG(args)  (args >> 6 & 0b11)

#define IS_ARG(arg, str_short, str_long) !(strcmp(arg, str_short) || strcmp(arg, str_long))
args_t parse_argv(const int argc, char** argv, uint32_t* operant_count) {
	assert(operant_count);
	args_t result = 0b0;
	for(int i=1; i<argc; ++i) {
		#define ARG argv[i]
		if (ARG[0] != '-') {
			++*operant_count;
			continue;
		}
		
		if (IS_ARG(ARG, "-a", "--all"))
			result |= ARG_DOT_FILES | ARG_DOT_DIRS;
		else if (IS_ARG(ARG, "-A", "--almost-all"))
			result |= ARG_DOT_FILES;
		else if (IS_ARG(ARG, "-d", "--dot-dirs"))
			result |= ARG_DOT_DIRS;
		else if (IS_ARG(ARG, "-f", "--no-nerdfonts"))
			result |= ARG_NO_NERDFONTS;
		else if (IS_ARG(ARG, "-c", "--dir-contents"))
			result |= ARG_DIR_CONTS;
		else if (IS_ARG(ARG, "-U", "--unsorted"))
			result |= ARG_UNSORTED;
		else if (IS_ARG(ARG, "-s", "--stat"))
			result |= ARG_STAT;
		else if (!strcmp(ARG, "--force-color"))
			force_color = true;
		else if (IS_ARG(ARG, "-hr", "--human-readable")) {
			if (argv[i+1] == NULL) {
				printf_color(stderr, YELLOW, "\"%s\" is missing it's specification! (assumed none, check --help next time).\n", ARG);
				return result;
			} ++i;
			
			switch(ARG[0]) {
				case 's':
					result |= 0b11 << 6; break;
				case 'n':
					break; // OR'ing 0 with 0... //
				case 'b':
					if(!strcmp(ARG, "bin")) result |= 0b10 << 6;
					else result |= 0b1 << 6;
					break;
				default:
					// atoi(3) can fail, but "0 on error", so we asssume none //
					uint8_t hr_val= (uint8_t)atoi(ARG);
					if (hr_val <= 3) result |= hr_val << 6;
			}
		} else if (IS_ARG(ARG, "-h", "--help")) {
			#ifdef __has_embed
			const char help[] = {
			#	embed "help.txt"
				, '\0'};
			#else
			const char help[] = "lsf was not compiled with the C23 standard; therefore, could not use `#embed`!\n";
			#endif

			puts(help);
			exit(0);
		} else if (IS_ARG(ARG, "-v", "--help")) {
			puts(__CU_FIED_VERSION__);
			exit(0);
		} else {
			char* tok = strtok(ARG, "=");
			if (IS_ARG(tok, "--human-readable", "-hr")) {
				tok = strtok(NULL, "=");
				uint8_t hr_val = (uint8_t)atoi(tok);
				if (hr_val <= 3) result |= hr_val << 6;
				continue;
			}

			printf_color(stderr, YELLOW, "\"%s\" is not a valid argument!\n", ARG);
		}
		#undef ARG
	} return result;
}


typedef struct file file_t;
struct file {
	char name[256];
	size_t size;
	mode_t stat;
	// TODO: //
	file_t* recursive_files;
};

file_t* query_files(const char* path, size_t* longest_fname, size_t* largest_fsize, size_t* fcount, const args_t args) {
	DIR* dfd = opendir(path);
	if (!dfd) return NULL;

	#define DA_STARTING_LEN 32
	size_t da_len = DA_STARTING_LEN;
	file_t* result = (file_t*)calloc(da_len, sizeof(file_t));
	// I'd normally use a goto here but since we allocate a bunch of variable sized var's on the stack, we can't gurantee our scope to be constant size... but now that I think about it whenever I use goto, i use it like a defer... god damn i wish this old ass language had some of zig's features(yes i'm aware defer was proposed to the C standard) //
	if (!result) {
		closedir(dfd);
		return NULL;
	}

	char true_path[strlen(path) + 257] = {};
	struct stat st = {}; struct dirent* dp = NULL;
	for (*fcount=0; (dp=readdir(dfd))!=NULL; ++*fcount) {
		if (*fcount >= da_len) {
			// I feel like (2da_len)/2 may get sneakily optimized out by -O... //
			if (*fcount != 0 && (2*da_len)/2 != *fcount) {
				errno = EOVERFLOW;
				break;
			} da_len*=2;

			void* da_new_sheisse = reallocarray(result, da_len, sizeof(file_t));
			if (!da_new_sheisse) break;
			else result = (file_t*)da_new_sheisse;
		}

		sprintf(true_path, "%s/%s", path, dp->d_name);
		stat(true_path, &st);

		result[*fcount].stat = st.st_mode;
		if (!S_ISDIR(st.st_mode))
			result[*fcount].size = (HR_ARG(args) == 1) ? st.st_size : st.st_blocks;
		else {
			size_t dir_conts_size = 0, junk0, junk1;
			// TODO: this is how we will do recursion flag, but for now just ignore it and free this pointer //
			free(query_files(path, &junk0, &dir_conts_size, &junk1, args));
			result[*fcount].size = dir_conts_size;
		}

		if (result[*fcount].size > *largest_fsize) *largest_fsize = st.st_size;

		strcpy(result[*fcount].name, dp->d_name);
		size_t fname = strlen(dp->d_name);
		if (fname > *longest_fname) *longest_fname = fname;
	}

	closedir(dfd);
	return result;
}

int main(int argc, char** argv) {
	struct winsize tty_dimensions = {0};
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dimensions);

	uint32_t operand_count = 0;
	const args_t args = parse_argv(argc, argv, &operand_count);
	#ifndef NDEBUG
	// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3047.pdf, _BitInt is converted to corresponding bit percision, so int is fully representable here //
	printf("args: %b\nsize arg: %b\n", (int)args, (int)HR_ARG(args));
	#endif

	file_t* files = NULL;
	size_t longest_fname = 0, largest_fsize = 0, fcount = 0;
	if (operand_count == 1) {
		files = query_files(".", &longest_fname, &largest_fsize, &fcount, args);
		if (errno || !files) {
			// TODO: error checking //
			return 1;
		}
		free(files);
		putchar('\n');
		return 0;
	}

	uint8_t ret_code = 0;
	for(int i=1; i<argc; ++i) {
		#define OPERAND argv[i]
		if (!operand_count) break;
		else if (OPERAND[0] == '-') continue;
		else if (OPERAND[strlen(OPERAND)-1] == '/' && strlen(OPERAND) != 1) OPERAND[strlen(OPERAND) - 1] = '\0';

		struct stat st;
		if (stat(OPERAND, &st) == -1) {
			// TODO: error check //
			ret_code += 1;
			continue;
		} else if (!S_ISDIR(st.st_mode)) {
			// TODO: call to cat, bat, or catf.... auctually... //
			// side rant: I don't know if Cu-Fied will implement cat or cd //
			// for cat, bat is already REALLY good, and for cd zoxide works wonders.. //
			// and I don't know what to do for either of those that hasn't already been done well //
			continue;
		}
		
		if (!(args & ARG_NO_NERDFONTS)) printf_color(stdout, BLUE, " ");
		printf_color(stdout, BLUE, "%s:\n", OPERAND);

		errno = 0;
		longest_fname = 0, largest_fsize = 0, fcount = 0;
		files = query_files(".", &longest_fname, &largest_fsize, &fcount, args);
		if (errno || !files) {
			// TODO: error checking //
			return 1;
		}

		--operand_count;
		putchar('\n');

		if (operand_count > 1) putchar('\n');
		free(files);
	} return ret_code;
}