#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
// #include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <math.h> // floor() and log10()
#include <fcntl.h>
// ftw was abit hard to find info online, the best you'll get is /usr/include/ftw.h header and ftw(3) manpage //
#include <ftw.h>
// this is not how to properly see if this is C23 but oh well //
#ifndef C23
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "../app_info.h"
#include "f_ext_map.h"
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
	char* parent_dir;
	char* name;
	struct stat st;
} file_t;

// mb eli, tried to to keep 0 global vars besides errno here since ik you don't like them
// but because ftw(3) works on a callback function, kinda cant for this one var //
struct files_query {
	file_t* files;
	size_t capacity;
	size_t len;
	uint32_t max_depth;
};
struct files_query queried_files = {NULL, 32, 0, 1};

// I really don't like this fix, infact I personally think GNU's FTW_ACTIONRETVAL flag is just how libc should handle nftw(3), but I am forced to do this. this also measn I believe lsf is inherntly faster on macos //
#ifndef __GLIBC__
#	define FTW_CONTINUE 0
#	define FTW_SKIP_SUBTREE 0 // this doesn't auctually skip the subdir on non-gnu
#	define FTW_STOP 1
#endif
// FIXME: why the fuck does ntfw just exits? i don't tell it to.. it prevents listing things like `/`, my brain is jumbled rn, ima go play ultakill and i'll get back at it //
int query_files(const char* path, const struct stat* st, int typeflag, struct FTW* file_desc) {
	// sanity checks //
	bool couldnt_get_stat = false;
	if (typeflag == FTW_NS) {
		// I dont think nftw() sets errno, but open() will //
		int fd = open(path, 0);
		if (fd != -1) close(fd);
		assert(errno != EBADF); // EBADF shouldn't happen //
		switch (errno) {
			case EPERM: // most likely failure //
				couldnt_get_stat = true;
				break;
			case EACCES: 
				couldnt_get_stat = true;
				break;
			case ELOOP:
				couldnt_get_stat = true;
				break;
			default: // it is probably fine //
 				couldnt_get_stat = true;
				break; 
		}
	} else if (typeflag == FTW_SLN) {
		errno = ELOOP;
		couldnt_get_stat = true;
	}


	char* path_copy = (char*)malloc(strlen(path) + 1);
	if (!path_copy) return FTW_STOP;
	strcpy(path_copy, path);
	
	if (file_desc->level > queried_files.max_depth) {
		// TODO: (Contains X) //
		if (file_desc->level > (queried_files.max_depth + 1)) {
			free(path_copy);
			return FTW_SKIP_SUBTREE;
		}
		if (queried_files.len > queried_files.capacity || !queried_files.files) {
			if ((queried_files.capacity << 1) < queried_files.capacity) {
				// overflow! (I really hope zig compiler doesn't fuck with me by killing the program here) //
				errno = EOVERFLOW;
				return FTW_STOP;
			} queried_files.capacity <<= 1;
			void* new_ptr = reallocarray(queried_files.files, queried_files.capacity, sizeof(file_t));
			if (!new_ptr) return FTW_STOP;
			else queried_files.files = (file_t*)new_ptr;
		}
		if (!couldnt_get_stat) queried_files.files[queried_files.len].st = *st; // < TEMP //
		free(path_copy);
		return FTW_SKIP_SUBTREE;
	} else {
		if (queried_files.len > queried_files.capacity || !queried_files.files) {
			if ((queried_files.capacity << 1) < queried_files.capacity) {
				// overflow! (I really hope zig compiler doesn't fuck with me by kill the program here) //
				errno = EOVERFLOW;
				return FTW_STOP;
			} queried_files.capacity <<= 1;
			void* new_ptr = reallocarray(queried_files.files, queried_files.capacity, sizeof(file_t));
			if (!new_ptr) return FTW_STOP;
			else queried_files.files = (file_t*)new_ptr;
		}
		if (!couldnt_get_stat) queried_files.files[queried_files.len].st = *st;
	}

	// i fucking hate this eyesore so i'll explain it so no one has to decipher these ancient egyptian hyroglyphics //
	// 1. tokenize our path copy based on / once so we get the base dir //
	// 2. allocate the memory we need for that (and error check) //
	// 3. copy it and free the path copy //
	char* tok = strtok(path_copy, "/");
	queried_files.files[queried_files.len].parent_dir = malloc(strlen(tok) + 1);
	if (!queried_files.files[queried_files.len].parent_dir) return FTW_STOP;
	strcpy(queried_files.files[queried_files.len].parent_dir, path_copy);
	free(path_copy);

	queried_files.files[queried_files.len].name = malloc(strlen(path + file_desc->base) + 1);
	strcpy(queried_files.files[queried_files.len].name, path + file_desc->base);
	
	// else queried_files.files[queried_files.len].st = {};
	
	++queried_files.len;
	return FTW_CONTINUE;
}

float get_simplified_file_size(const size_t f_size, char* unit, args_t args) {
	assert(*unit == 0);

 	// this shows ULONG_MAX on my system is 19 digits long, perfectly representable by 8 bits //
	// int main(void) {
	// 	printf("%f", floor(log10((double)ULONG_MAX)));
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

size_t get_longest_fdescriptor(const file_t* files, const size_t f_count, uint8_t* longest_fname, size_t* largest_fsize, const args_t args) {
	assert(*longest_fname == 0);
	assert(*largest_fsize == 0);
	// first we have to get longest_fname and largest_fsize //
	for (size_t i=0; i<f_count; ++i) {
		size_t fname_len = strlen(files[i].name);
		if (fname_len > *longest_fname) *longest_fname = fname_len;
		if (files[i].st.st_size > *largest_fsize) *largest_fsize = files[i].st.st_size;
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
	if(f_info.st.st_mode & S_IFDIR) return get_escape_code(STDOUT_FILENO, BLUE);
	else if(f_info.st.st_mode & S_IXUSR) return get_escape_code(STDOUT_FILENO, GREEN);

	if (!(args & ARG_NO_NERDFONTS)) {
		char* is_media = NULL; char* extension = NULL;
		
		// strtok(3) modifies the string itself, we must create a copy or our name becomes all fucked // 
		char file_name_copy[strlen(f_info.name) + 1] = {};
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

	if(f_info.st.st_mode & S_IFREG) return "\0";
	else return get_escape_code(STDOUT_FILENO, CYAN); // must be symlink //
}

const char* get_nerdfont_icon(file_t f_info, table_t* f_ext_map, const args_t args) {
	if (args & ARG_NO_NERDFONTS) return "\0";

	char* result = NULL;
	if ((result = f_ext_map->get(f_ext_map, f_info.name).s)) return result;

	// strtok(3) modifies the string itself, we must create a copy or our name becomes all fucked // 
	char file_name_copy[strlen(f_info.name) + 1] = {};
	strcpy(file_name_copy, f_info.name);			

	char* tok = strtok(file_name_copy, ".");
	while (tok) {
		result = tok;
		tok = strtok (NULL, ".");
	} if((result = f_ext_map->get(f_ext_map, result).s) &&
		 !(!strcmp(result, " ") && S_ISDIR(f_info.st.st_mode)) /*HACK TO GET RID OF `lib` DIRS */ ) return result;

	if(S_ISDIR(f_info.st.st_mode)) return " ";
	else if(f_info.st.st_mode & S_IXUSR) return " ";
	else return " ";
}

// oh god i am reading too much GNU source code //
bool list_files(const file_t* files,
				const size_t longest_descriptor, const size_t f_count,
				const uint32_t files_per_row,
				table_t* f_ext_map, 
				bool (*condition)(mode_t),
				args_t args) {
	assert(f_ext_map || (args & ARG_NO_NERDFONTS ));

	static size_t files_printed = 0;
	for (size_t i=0; i<f_count; ++i) {
		#define FILE files[i]
		if (condition(FILE.st.st_mode)) continue;
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

		// ehh _BitInt(2) is nice and explicit but I'm aware compile will make this an i8 //
		u(2) hr_arg = HR_ARG(args);
		if (hr_arg) {
			if (S_ISDIR(FILE.st.st_mode)) {
				if (!(args & ARG_DIR_CONTS)) goto skip_dirs;
				else descriptor_len += sb_append(&sb, "(Contains ");
			} else descriptor_len += sb_append(&sb, "(");

			char* file_size = NULL;
			if (hr_arg == 1) {
				if (!(file_size = malloc(floor(log10(FILE.st.st_blocks + 1)) + 4))) return false;
			// 	https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html //
			// apple uses their own type for blocks that derives from long long //
				#ifdef __APPLE__
				sprintf(file_size, "%lli B)", FILE.st.st_blocks);
				#else
				sprintf(file_size, "%lu B)", FILE.st.st_blocks);
				#endif
			} else {
				char unit = 0;
				const float hr_size = get_simplified_file_size(FILE.st.st_blocks, &unit, args);
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

// evil bool... aka this is for return code, so 0 on success, 1 on fail.. just inverse is all //
bool query_and_list(const char* operand, table_t* f_ext_map, const struct winsize tty_dimensions, const args_t args) {
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

	(void)args; // < TODO
	if (args & ARG_RECURSIVE) {
		queried_files.max_depth = UINT_MAX;
		puts("Recursive file listing is not implemented yet!\n");
		goto TODO;
	}
	
	// if I understand the description of FTW_DEPTH flag correctly, we go through all of ./ first and then the subdirectories //
	#ifndef __APPLE__
	int nsfw = nftw(operand, &query_files, fd, FTW_ACTIONRETVAL|FTW_DEPTH);
	#else
	int nsfw = nftw(operand, &query_files, fd, FTW_DEPTH);
	#endif
	if (nsfw == -1 || errno) {
		switch (errno) {
			case EOVERFLOW:
				printf_color(stderr, RED, "Failed to allocate memory for files! ");
				printf_color(stderr, RED, "(File capacity overflowed!)\n"); 
				return 1;
			case ENOMEM:
				printf_color(stderr, RED, "Failed to allocate memory for files! ");
				printf_color(stderr, RED, "(Ran out of memory!)\n");
				return 1;
			case ELOOP:
				printf_color(stderr, YELLOW, "Encountered a symbolic link loop in a file!\n");
				break;
			case EACCES:
				printf_color(stderr, YELLOW, "Didn't have permission to get stat on a file!\n");
				break;
			case EPERM:
				printf_color(stderr, YELLOW, "Didn't have permission to get stat on a file!\n");
				break;
			default:
				printf_color(stderr, YELLOW, "Encountered an error! (%s : %d)\n", strerror(errno), errno);
				break;
		}
	}

	uint8_t longest_fname = 0; size_t largest_fsize = 0;
	const size_t longest_fdescriptor = get_longest_fdescriptor(queried_files.files, queried_files.len, &longest_fname, &largest_fsize, args);
	bool succ = 1;
	if(!list_files(queried_files.files, longest_fdescriptor, queried_files.len, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_dontcare, args)) {
		printf_color(stderr, RED, "Failed to allocate memory for showing file size!\n");
		succ = 0;
	}
	// free everything //
	TODO:
	for(size_t i=0; i<queried_files.len; ++i) {
		free(queried_files.files[i].name);
		free(queried_files.files[i].parent_dir);	
	} free(queried_files.files);
	
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
	
	uint8_t longest_fname = 0, retcode = 0;
	size_t largest_fsize = 0, f_count = 0;
	if (operand_count == 1) {
		retcode = (uint8_t)query_and_list(".", f_ext_map, tty_dimensions, args);
	} else{
		for (int i=1; i<argc; ++i) {
			if (operand_count == 0) break; // we can ignore the rest of the args //

			#define ARG argv[i]
			if (ARG[0] == '-') continue;
			else --operand_count;

			const char* folder_icon = args & ARG_NO_NERDFONTS ? "" : "";
			printf_color(stdout, BLUE, "%s %s:\n", folder_icon, ARG);

			retcode += query_and_list(ARG, f_ext_map, tty_dimensions, args);
		}
	}
	ht_free(f_ext_map);
	return retcode;
}