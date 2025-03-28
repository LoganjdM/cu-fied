#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <math.h> // floor() and log10()
#include <fcntl.h>
#include <alloca.h>
// this is not how to properly see if this is C23 but oh well //
#ifndef C23
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "../app_info.h"
#include "../stat/do_stat.h"
#include "../f_ext_map.h"
#include <strbuild.h>

#define REALLOCARRAY_IMPLEMENTATION
#include "../polyfill.h"

// I wish C had zig packed structs //
#ifndef C23
#	define u(n) int
#else
#	define u(n) unsigned _BitInt(n)
#endif
typedef u(9) args_t;

// bool //
#define ARG_DOT_DIRS     0b1
#define ARG_DOT_FILES    0b10
#define ARG_UNSORTED     0b100
#define ARG_NO_NERDFONTS 0b1000
#define ARG_STAT         0b10000
#define ARG_DIR_CONTS    0b100000
// u2 //
#define HR_ARG(args)  (args >> 6 & 0b11)
// bool //
#define ARG_RECURSIVE    0b100000000

// TODO: I want to abstract this argument parsing into a function that every program can use //
#define IS_ARG(arg, str_short, str_long) (!strcmp(arg, str_short) || !strcmp(arg, str_long))
args_t parse_argv(const int argc, char** argv, uint32_t* operant_count) {
	assert(operant_count);
	args_t result = 0b0;
	for(int i=1; i<argc; ++i) {
		#define ARG argv[i]
		if (ARG[0] != '-') {
			++*operant_count;
			continue;
		}

		if (!strcmp(ARG, "--all"))
			result |= ARG_DOT_FILES | ARG_DOT_DIRS;
		else if (!strcmp(ARG, "--almost-all"))
			result |= ARG_DOT_FILES;
		else if (!strcmp(ARG, "--dot-dirs"))
			result |= ARG_DOT_DIRS;
		else if (!strcmp(ARG, "--no-nerdfonts"))
			result |= ARG_NO_NERDFONTS;
		else if (!strcmp(ARG, "--dir-contents"))
			result |= ARG_DIR_CONTS;
		else if (!strcmp(ARG, "--unsorted"))
			result |= ARG_UNSORTED;
		else if (!strcmp(ARG, "--stat"))
			result |= ARG_STAT;
		else if (!strcmp(ARG, "--force-color"))
			force_color = true;
		else if (!strcmp(ARG, "--recursive")) {
			result |= ARG_RECURSIVE;
		}
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
					// atoi(3) can fail, but "0 on error", so we assume none //
					uint8_t hr_val= (uint8_t)atoi(ARG);
					if (hr_val <= 3) result |= hr_val << 6;
			}
		} else if (IS_ARG(ARG, "-h", "--help")) {
			#ifdef __has_embed
			result |= ARG_DIR_CONTS;
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

			size_t arg_len = strlen(ARG);
			for (size_t j=1; j<arg_len; ++j) {
				 switch (ARG[j]) {
				 	case 'a':
						result |= ARG_DOT_FILES | ARG_DOT_DIRS; continue;
					case 'A':
						result |= ARG_DOT_FILES; continue;
					case 'd':
						result |= ARG_DOT_DIRS; continue;
					case 'f':
						result |= ARG_NO_NERDFONTS; continue;
					case 'c':
						result |= ARG_DIR_CONTS; continue;
					case 'U':
						result |= ARG_UNSORTED; continue;
					case 's':
						result |= ARG_STAT; continue;
					case 'R':
						result |= ARG_RECURSIVE; continue;
					default: break;
				 }
				printf_color(stderr, YELLOW, "\"%s\" is not a valid argument!\n", ARG);
				break;
			}

		}
		#undef ARG
	} return result;
}


typedef struct {
	char** dir; 
	char* name/*[256] <- this long, but to avoid sigsev we use alloca(3)*/;
	struct stat stat;

	bool ok_st;
	char* link;
} file_t;

typedef struct { 
	void* elems;
	size_t len;
	size_t cap;
} da_t;

bool query_files(char* path, const int fd, 
				da_t* da_files, da_t* da_fd,
				const size_t recurse) {
	// sanity checks //
	assert(fd != -1);
	assert(da_files);
	assert(da_fd);
	assert(path);

	if (recurse > 0) return true;

	DIR* dfp = fdopendir(fd);
	if (!dfp) return false;
	
	struct dirent* d_stream = NULL;
	for (; (d_stream = readdir(dfp)); ++da_files->len) {
		#define FILE ((file_t*)da_files->elems)[da_files->len - 1]

		if (da_files->len > da_files->cap) {
			if (da_files->cap > da_files->cap << 1) {
				errno = EOVERFLOW;
				return false;
			} da_files->cap <<= 1;

			void* np = reallocarray(da_files->elems, da_files->cap, sizeof(file_t));
			if (!np) return false;
			da_files->elems = (file_t*)np;
		} if (da_fd->len > da_fd->cap) {
			// closing is an expensive operation. so instead we make use of close_range() syscall // 
			close_range(((unsigned int*)da_fd->elems)[0], ((unsigned int*)da_fd->elems)[da_fd->cap - 1], 0);
			da_fd->len = 0;
		}

		FILE.dir = &path;
		FILE.name = alloca(256);
		strcpy(FILE.name, d_stream->d_name);

		char* fullpath = malloc(strlen(FILE.dir)+strlen(FILE.name)+2);
		if (!fullpath) return false;

		int fd = open(fullpath, 0);
		if (fd == -1) continue;
		
		((int*)da_fd->elems)[da_fd->len] = fd;
		++da_fd->len;

		struct stat st = {0};
		FILE.ok_st = fstat(fd, &st) != -1;
		FILE.stat = st;

		if(FILE.ok_st && S_ISDIR(FILE.stat.st_mode)) {
			if (!query_files(fullpath, fd, da_files, da_fd, recurse - 1))
				return false;
		} else free(fullpath);

		#undef FILE
	} closedir(dfp);

	return true;
}

float get_simplified_file_size(const size_t f_size, char* unit, args_t args) {
	assert(*unit == 0);

 	// this shows ULONG_MAX on my system is 19 digits long, perfectly representable by 8 bits //
	// int main(void) {
	// 	printf("%f", floor(log10((double)ULONG_MAX))+1);
	// 	return 0;
	// }
	uint8_t exponent = 0;
	if (f_size != 0) exponent = floor(log10(f_size));
	if ((exponent + 1) < 3) return (float)f_size;

	// exponent + 1 == strlen(f_size) //
	switch (exponent) {
		case 4: *unit = 'K'; break;
		case 5: *unit = 'M'; break;
		case 6: *unit = 'G'; break;
		case 7: *unit = 'T'; break;
		case 8: *unit = 'P'; break;
		case 9: *unit = 'E'; break;
		case 10: *unit = 'Z'; break;
		// I better never see this shit be incorrect because your file happens to be bigger than a yottabyte fucking galactic machine //
		case 11: *unit = 'Y'; break;
	}

	// si(x) = x/10^(floor∘log₁₀)(x)
	// i spent like 30 minutes figuring that equation out
	u(2) hr_arg = HR_ARG(args);
	if (hr_arg == 3) return (float)f_size / pow(10, exponent);
	else return (float)f_size / ( pow(2 * exponent, 10) ); // FIXME: this is wrong //
}

size_t get_longest_fdescriptor(const da_t* da_files, uint8_t* longest_fname, size_t* largest_fsize, const args_t args) {
	assert(da_files);

	assert(*longest_fname == 0);
	assert(*largest_fsize == 0);
	// first we have to get longest_fname and largest_fsize //
	for (size_t i=0; i<da_files->len; ++i) {
		#define FILE ((file_t*)da_files->elems)[i]
		size_t fname_len = strlen(FILE.name);
		if (fname_len > *longest_fname) *longest_fname = fname_len;
		if ((size_t)FILE.stat.st_size > *largest_fsize) *largest_fsize = FILE.stat.st_size;
		#undef FILE
	}

	size_t result = (size_t)*longest_fname + 3;

	u(2) hr_arg = HR_ARG(args);
	switch (hr_arg) {
		case 0: break;
		case 1:
			result += floor(log10(*largest_fsize)) + 1;
			break;
		default:
			char junk = 0;
			float hr_size = get_simplified_file_size(*largest_fsize, &junk, args);
			// "somewhere around this ... shouldn't overflow" am I stupid? if i'm not certain i should use snprintf not sprintf to truly prevent overflow //
			char largest_hr_fsize[25] = {0};
			if (hr_arg == 2) result += snprintf(largest_hr_fsize, 25, "%.1f XiB", hr_size);
			else result += snprintf(largest_hr_fsize, 25, "%.1f XB", hr_size);

			result += /*( ) */5;
			if (args & ARG_DIR_CONTS) result += /*Contains */9;
	}

	if (args & ARG_STAT) result += /*<drwxr-xr-x> */13;
	if (!(args & ARG_NO_NERDFONTS)) result += /* */2;
	return result;
}

const char* get_descriptor_color(file_t f_info, table_t* f_ext_map, args_t args) {
	if(f_info.stat.st_mode & S_IFDIR) return get_escape_code(STDOUT_FILENO, BLUE);
	else if(f_info.stat.st_mode & S_IXUSR) return get_escape_code(STDOUT_FILENO, GREEN);

	if (!(args & ARG_NO_NERDFONTS)) {
		char* is_media = NULL; char* extension = NULL;

		// strtok(3) modifies the string itself, we must create a copy or our name becomes all fucked //
		char file_name_copy[strlen(f_info.name) + 1];
		strcpy(file_name_copy, f_info.name);

		char* tok = strtok(file_name_copy, ".");
		while(tok) {
			extension = tok;
			tok = strtok(NULL, ".");
		} if(!(is_media=f_ext_map->get(f_ext_map, extension).p)) return "\0";

		// TODO: prob could do some boolean math here instead of 78352957 237y89523 if statements //
		if(!strcmp(is_media, "󰈟 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		else if(!strcmp(is_media, "󰵸 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		else if(!strcmp(is_media, "󰜡 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		else if(!strcmp(is_media, "󰈫 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		else if(!strcmp(is_media, "󰈫 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
	}

	if(f_info.stat.st_mode & S_IFREG) return "\0";
	else return get_escape_code(STDOUT_FILENO, CYAN); // must be symlink //
}

const char* get_nerdfont_icon(file_t f_info, table_t* f_ext_map, const args_t args) {
	if (args & ARG_NO_NERDFONTS) return "\0";

	char* result = "\0";
	if ((result = f_ext_map->get(f_ext_map, f_info.name).s)) return result;

	// strtok(3) modifies the string itself, we must create a copy or our name becomes all fucked //
	char file_name_copy[strlen(f_info.name) + 1];
	strcpy(file_name_copy, f_info.name);

	char* tok = strtok(file_name_copy, ".");
	while (tok) {
		result = tok;
		tok = strtok (NULL, ".");
	} if (result && (result = f_ext_map->get(f_ext_map, result).s)) return result;

	if(S_ISDIR(f_info.stat.st_mode)) return " ";
	else if(f_info.stat.st_mode & S_IXUSR) return " ";
	else return " ";
}

// oh god i am reading too much GNU source code //
bool list_files(const da_t* files,
				const size_t longest_descriptor,
				const uint32_t files_per_row,
				table_t* f_ext_map,
				bool (*condition)(mode_t),
				args_t args) {
	assert(f_ext_map || (args & ARG_NO_NERDFONTS ));

	static size_t files_printed = 0;
	for (size_t i=0; i<files->len; ++i) {
		#define FILE ((file_t*)files->elems)[i]
		if (condition(FILE.stat.st_mode)) continue;
		else if (FILE.name[0] == '.') {
			if (!(args & ARG_DOT_DIRS) && !(strcmp(FILE.name, ".") && strcmp(FILE.name, ".."))) continue;
			else if (!(args & ARG_DOT_FILES)) continue;
		}

		strbuild_t sb = sb_new();
		size_t descriptor_len = 0;
		sb_append(&sb, get_descriptor_color(FILE, f_ext_map, args));
		descriptor_len += sb_append(&sb, get_nerdfont_icon(FILE, f_ext_map, args));
		sb_append(&sb, get_escape_code(STDOUT_FILENO, BOLD));

		descriptor_len += sb_append(&sb, FILE.name);
		descriptor_len += sb_append(&sb, " ");
		sb_append(&sb, get_escape_code(STDOUT_FILENO, RESET));
		if (args & ARG_STAT) {
			descriptor_len += sb_append(&sb, "<");
			descriptor_len += sb_append(&sb, get_readable_mode(FILE.stat.st_mode));
			descriptor_len += sb_append(&sb, ">");
		}

		// ehh _BitInt(2) is nice and explicit but I'm aware compile will make this an i8 //
		u(2) hr_arg = HR_ARG(args);
		if (hr_arg) {
			if (S_ISDIR(FILE.stat.st_mode)) {
				if (!(args & ARG_DIR_CONTS)) goto skip_dirs;
				else descriptor_len += sb_append(&sb, "(Contains ");
			} else descriptor_len += sb_append(&sb, "(");

			char* file_size = NULL;
			if (hr_arg == 1) {
				if (!(file_size = malloc(floor(log10(FILE.stat.st_blocks + 1)) + 4))) return false;
			// 	https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html //
			// apple uses their own type for blocks that derives from long long //
				#ifdef __APPLE__
				sprintf(file_size, "%lli B)", FILE.stat.st_blocks);
				#else
				sprintf(file_size, "%lu B)", FILE.stat.st_blocks);
				#endif
			} else {
				char unit = 0;
				const float hr_size = get_simplified_file_size(FILE.stat.st_blocks, &unit, args);
				#define ARBITRARY_SIZE 100
				if (!(file_size = malloc(ARBITRARY_SIZE))) return false;

				// file system block of 8 KiB bytes on linux //
				if (unit == 0) snprintf(file_size, ARBITRARY_SIZE, "%.0f Blocks) ", hr_size);
				else if (hr_arg == 2) snprintf(file_size, ARBITRARY_SIZE, "%.1f %ciB) ", hr_size, unit);
				else snprintf(file_size, ARBITRARY_SIZE, "%.1f %cB) ", hr_size, unit);
				#undef ARBITRARY_SIZE
				descriptor_len += sb_append(&sb, file_size);
			}
			free(file_size);
		} skip_dirs:

		for (size_t i=longest_descriptor-descriptor_len; i>0; --i) sb_append(&sb, " ");
		++files_printed;
		// i tried doing this based off bytes printed, gave issues. also same with using `i % files_per_row` as it gave some segfault i dont understand to be real with you //
		if(files_printed >= files_per_row) {
			printf("%s\n", sb.str); files_printed = 0;
		} else printf("%s", sb.str);
		free(sb.str);
	}
	fflush(stdout);
	return true;
}

bool condition_isdir(mode_t stat) { return S_ISDIR(stat); }
bool condition_isndir(mode_t stat) { return !S_ISDIR(stat); }
bool condition_dontcare(mode_t stat) { (void)stat; return false; }

bool query_and_list(const char* operand, table_t* f_ext_map, const struct winsize tty_dimensions, const args_t args, const bool list_name) {
	int fd = open(operand, 0);
	if (fd == -1) {
		printf_color(stderr, YELLOW, "Could not list ");
		printf_color(stderr, BLUE, "%s ", operand);
		switch (errno) {
			case EACCES:
				printf_color(stderr, YELLOW, "! (do you have access to it?)\n", operand);
				break;
			case EBADF:
				printf_color(stderr, YELLOW, "! (is it a valid file/directory?)\n", operand);
				break;
			case EINVAL:
				printf_color(stderr, YELLOW, "! (is it a valid path?)\n", operand);
				break;
			default:
				printf_color(stderr, YELLOW, "!\n", operand);
				break;
		} return 1;
	}

	struct stat st = {0};
	if (fstat(fd, &st) == -1) {
		assert(errno != EBADF);

		printf_color(stderr, YELLOW, "Could get info on ");
		printf_color(stderr, BLUE, "%s ", operand);
		switch (errno) {
			case EACCES:
				printf_color(stderr, YELLOW, "! (do you have access to it?)\n", operand);
				break;
			case ENOMEM:
				printf_color(stderr, RED, "! (Couldn't get memory!)\n", operand);
				// > Out of memory (i.e., kernel memory) //
				// You are fucked for other reasons if this happens //
				exit(2);
			case EOVERFLOW:
				printf_color(stderr, YELLOW, "! (File's size, inode, or block count, can't be represented!)\n", operand);
				break;
			default:
				printf_color(stderr, YELLOW, "! (%s)", strerror(errno));
		} return 1;
	}

	if (list_name) {
		char operand_copy[strlen(operand) + 1];
		strcpy(operand_copy, operand);

		char* f_ext = "\0";
		char* tok = strtok(operand_copy, ".");
		while(tok) {
			f_ext = tok;
			tok = strtok(NULL, ".");
		}
		const char* nerdicon = f_ext_map->get(f_ext_map, f_ext).s;
		const char* not_found_nerdicon = S_ISDIR(st.st_mode) ? "" : "";
		printf_color(stdout, S_ISDIR(st.st_mode) ? BLUE : RESET, "%s %s:\n", nerdicon ? nerdicon : not_found_nerdicon, operand);
	}

	if (!S_ISDIR(st.st_mode)) {
		// careful.. dont want an RCE //
		for (size_t i=0; i<strlen(operand); ++i) {
			if (operand[i] == ';' || operand[i] == '|' || operand[i] == '&') {
				printf_color(stderr, YELLOW, "Not catting file for security! Could result in remote code execution!\n");
				fflush(stderr); // fuck sake //
				goto avoid_rce;
			}
		}

		// TODO: account for environment variables... non-standard distros exist, ie Nix //
		if(execl("/usr/bin/bat", "bat", "-p", operand, NULL)==-1) {
			if (execl("/usr/bin/more", "more", "-f", operand, NULL)==-1) {
				printf_color(stderr, YELLOW, "Failed to list ");
				printf_color(stderr, BLUE, "%s", operand);
				switch (errno) {
					default:
						printf_color(stderr, YELLOW, "! (%s : %d)", strerror(errno), errno);
				}
			}
		} return 0;
	}

	avoid_rce:
	size_t max_recurse = 0;
	if (args & ARG_RECURSIVE) {
		max_recurse = UINT_MAX;
		puts("Recursive file listing is not implemented yet!\n");
		goto TODO;
	}

	char* path = strdup(operand);
	// i tried putting this on the stack but it segfaults //
	da_t* da_files = malloc(sizeof(da_t));
	if (!da_files) return false;
	da_files->cap = 8;
	da_files->elems = calloc(da_files->cap, sizeof(file_t));
	
	da_t* da_fd = alloca(sizeof(da_t));
	da_fd->cap = 50;
	da_fd->elems = calloc(da_fd->cap, sizeof(file_t));
	
	if (!query_files(path, fd, da_files, da_fd, max_recurse)) {
		switch (errno) {
			case EOVERFLOW:
				printf_color(stderr, RED, "Failed to allocate memory for files! (File capacity overflowed!)\n");
				return 1;
			case ENOMEM:
				printf_color(stderr, RED, "Failed to allocate memory for files! (Ran out of memory!)\n");
				return 1;
			case ELOOP:
				printf_color(stderr, YELLOW, "Encountered a symbolic link loop in a file!\n");
				break;
			case EACCES:
				printf_color(stderr, YELLOW, "Wasn't allowed to get stat on a file!\n");
				break;
			case EPERM:
				printf_color(stderr, YELLOW, "Didn't have permission to get stat on a file!\n");
				break;
			default:
				printf_color(stderr, YELLOW, "Encountered an error! (%s : %d)\n", strerror(errno), errno);
				break;
		}
	}

	uint8_t longest_fname = 0;
	size_t largest_fsize = 0;
	const size_t longest_fdescriptor = get_longest_fdescriptor(da_files, &longest_fname, &largest_fsize, args);
	bool succ = 1;
	if (args & ARG_UNSORTED)
		succ = list_files(da_files, longest_fdescriptor, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_dontcare, args);
	else {
		succ = list_files(da_files, longest_fdescriptor, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isndir, args);
		succ |= list_files(da_files, longest_fdescriptor, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isdir, args);
	}

	// only possible failure //
	if(!succ)
		printf_color(stderr, RED, "Failed to allocate memory for showing file size!\n");

	// free everything //
	TODO:
	// this car is too bumpy for me to see what the fuck this double free is (im currently omw to alabama rn) //
	// for(size_t i=0; i<queried_files.len; ++i) {
		// #define QUERIED_FILE queried_files.files[i]
		// if(QUERIED_FILE.name) {
			// printf("name : %p\n", QUERIED_FILE.name);
		    // free(QUERIED_FILE.name);
		    // QUERIED_FILE.name = NULL;
		// }
		// if(QUERIED_FILE.parent_dir) {
			// printf("parent dir : %p\n", QUERIED_FILE.parent_dir);
			// free(QUERIED_FILE.parent_dir);
			// QUERIED_FILE.parent_dir = NULL;
		// }
		// #undef QUERIED_FILE
	// } free(queried_files.files);

	return !succ;
}

int main(int argc, char** argv) {
	struct winsize tty_dimensions = {0};
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dimensions);

	uint32_t operand_count = 1;
	const args_t args = parse_argv(argc, argv, &operand_count);
	#ifndef NDEBUG
	// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3047.pdf, _BitInt is converted to corresponding bit precision, so int is fully representable here //
	printf("args: %b\noperands: %i\n", (int)args, operand_count);
	#endif

	table_t* f_ext_map = NULL;
	if (!(args & ARG_NO_NERDFONTS)) {
		f_ext_map = init_filetype_dict();
		if (!f_ext_map) {
			printf_color(stderr, RED, "Failed to allocate memory for file extension table, for nerdfonts!\n");
			return 1;
		}
	}

	uint8_t retcode = 0;
	if (operand_count == 1) {
		retcode = (uint8_t)query_and_list(".", f_ext_map, tty_dimensions, args, false);
	} else {
		for (int i=1; i<argc; ++i) {
			if (operand_count == 0) break; // we can ignore the rest of the args //

			#define ARG argv[i]
			if (ARG[0] == '-') continue;
			else --operand_count;

			retcode += query_and_list(ARG, f_ext_map, tty_dimensions, args, true);
		}
	}
	if(f_ext_map) ht_free(f_ext_map);
	return retcode;
}
