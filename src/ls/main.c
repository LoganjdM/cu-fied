#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// if we dont have embed we *probably* aren't c23 (though some versions of clang are c23 without #embed so this isn't foolproof) //
#ifndef __has_embed
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "../app_info.h"

#define REALLOCARRAY_IMPLEMENTATION
#define MEMPCPY_IMPLEMENTATION
#include "../polyfill.h"

#include "../ctypes/strbuild.h"
#include "file_t_map.h"

// the ENTIRE reason for this is just cuz i thought itd be unique and wanted to see if i could do it //
// it turned out to be not that bad and I auctually quite liked using it //
uint16_t args = 0b0;

// booleans //
#define ARG_DOT_DIRS 0b1
#define ARG_DOT_FILES 0b10
#define ARG_UNSORTED 0b100
#define ARG_NO_NERDFONTS 0b1000
#define ARG_FPERMS 0b1000000
#define ARG_DIR_CONTS 0b10000000
// u2 int //
#define SIZE_ARG(args) (args >> 4 & 0b11)
#define ARG_SIZE_NONE	0b000000

struct file {
	char name[256];
	uint32_t size;
	mode_t stat;
};

// FIXME: if bytes == true, this may overflow! //
uint32_t dir_contents(const char* dir, const bool bytes) {
	DIR* dfd = opendir(dir);
	if(!dfd) {
		printf_escape_code(stderr, RED, "Failed to check the size of \"");
		printf_escape_code(stderr, BLUE, "%s", dir);
		printf_escape_code(stderr, RED, "\". (Could not open directory!)\n"); print_escape_code(stderr, RESET);
		return 0;
	} uint32_t result = 0;
	struct stat st; struct dirent* dp;
	char true_fname[strlen(dir)+257];
	while((dp=readdir(dfd))!=NULL) {
		sprintf(true_fname, "%s/%s", dir, dp->d_name);

		stat(true_fname, &st);
		if(!S_ISDIR(st.st_mode)) {
			result += bytes ? st.st_size : st.st_blocks;
		}
		result += 0;
	} closedir(dfd); return result;
}

#define STARTING_LEN 25
struct file* query_files(const char* dir, uint8_t* longest_fname, uint32_t* largest_fsize, uint16_t* fcount, struct file* files) {
	DIR* dfd = opendir(dir);
	if(!dfd) {
		printf_escape_code(stderr, RED, "Failed to query files! (memory allocation failure!)\n"); print_escape_code(stderr, RESET);
		return NULL;
 	} uint16_t array_len = STARTING_LEN;
	if(!files) {
		if(!(files=calloc(array_len, sizeof(struct file)))) {
			printf_escape_code(stderr, RED, "Failed to query files! (memory allocation failure!)\n"); print_escape_code(stderr, RESET);
			closedir(dfd);
			return NULL;
		} *largest_fsize = 0; *longest_fname = 0;
	}

	char true_fname[strlen(dir)+257];
 	struct stat st; struct dirent* dp;
 	uint16_t i;
	for(i=0;(dp=readdir(dfd))!=NULL;++i) {
		if(i>=array_len) {
			if(((array_len*2)*sizeof(struct file)) < (array_len*sizeof(struct file))) {
				printf_escape_code(stderr, RED, "Failed to query files! (realloaction caused multiplication overflow!)\n"); print_escape_code(stderr, RESET);
				break;
			} array_len*=2;
			#ifndef NDEBUG
			printf("Reallocating %lu bytes", array_len*sizeof(struct file));
			#endif
			void* new_ptr = reallocarray(files, array_len, sizeof(struct file));
			if(!new_ptr) {
				printf_escape_code(stderr, RED, "Failed to query files! (reallocation failure!)\n"); print_escape_code(stderr, RESET);
				break;
			} files = (struct file*)new_ptr;
		}

		sprintf(true_fname, "%s/%s", dir, dp->d_name);
		stat(true_fname, &st);

		files[i].stat = st.st_mode;
		if(!S_ISDIR(st.st_mode)) {
			files[i].size = (SIZE_ARG(args)==1) ? st.st_size : st.st_blocks;
		} else {
			files[i].size = dir_contents(true_fname, SIZE_ARG(args)==1);
		}
		if(files[i].size > *largest_fsize) *largest_fsize = st.st_size;

		strcpy(files[i].name, dp->d_name);
		uint8_t fname = (uint8_t)strlen(dp->d_name);
		if(fname > *longest_fname) *longest_fname = fname;
	}

	closedir(dfd);
	*fcount = i;
	return files;
}

// returns the amount of auctual non-argument entries there are so we can just skip past em //
#define ISARG(arg, shortstr, longstr) !strcmp(arg, shortstr) || !strcmp(arg, longstr)
uint16_t parse_arguments(const int argc, char** argv) {
	int dir_argc = 0;
	for(int i=0;i<argc;++i) {
		if(argv[i][0]!='-') {
			dir_argc++;
			continue;
		}
		// TODO: hashmap //
		if(ISARG(argv[i], "-a", "--all") || ISARG(argv[i], "-f", "--full")) {
			args |= ARG_DOT_DIRS | ARG_DOT_FILES;
		} else if(ISARG(argv[i], "-A", "--almost-all")) {
			args |= ARG_DOT_FILES;
		} else if(ISARG(argv[i], "-d", "--dot-dirs")) {
			args |= ARG_DOT_DIRS;
		} else if(ISARG(argv[i], "-nf", "--no-nerdfonts")) {
			args |= ARG_NO_NERDFONTS;
		} else if(ISARG(argv[i], "-c", "--dir-contents")) {
			args |= ARG_DIR_CONTS;
		} else if(ISARG(argv[i], "-hr", "--human-readable")) {
			if(argv[i+1]==NULL) {
				printf_escape_code(stderr, YELLOW,  "\"%s\" is missing an argument on what type! (assumed none).\n", argv[i]); print_escape_code(stderr, RESET);
				return dir_argc;
			} ++i;
			switch(argv[i][0]) {
				case 's':
					args |= SIZE_ARG(3);
					break;
				case 'n':
					args |= ARG_SIZE_NONE;
					break;
				case 'b':
					if(!strcmp(argv[i], "bin")) {
						args |= SIZE_ARG(2);
					} else {
						args |= SIZE_ARG(1);
					} break;
				default:
					uint8_t val = (uint8_t)atoi(argv[i]);
					if(val <= 3) {
						args |= (val << 4);
					}
			}
		} else if(ISARG(argv[i], "-U", "--unsorted")) {
			args |= ARG_UNSORTED;
		} else if(ISARG(argv[i], "-h", "--help")) {
			#ifdef __has_embed
			const char help[] = {
			#embed "help.txt"
				, '\0'};
			#else
			const char help[] = "lsf was not compiled with a modern C compiler like Clang 9 or GCC15, and could not make use of `#embed` which is required for the help message!\n";
			#endif

			puts(help);
			exit(0);
		} else if(ISARG(argv[i], "-v", "--version")) {
			puts(vers);
			exit(0);
		} else if(ISARG(argv[i], "-g", "--uid-gid")) {
			args |= ARG_FPERMS;
		} else if(ISARG(argv[i], "-fc", "--force-color")) {
			force_color = true;
		} else {
			// what the fuck was I smoking when I was using strncpy for this? just tokenize it //
			char* tok = strtok(argv[i], "=");
			if(ISARG(tok, "--human-readable", "-hr")) {
				tok = strtok(NULL, "=");
				// atoi can fail here but it returns with 0 so if your wrong we assume default //
				uint8_t val =(uint8_t)atoi(tok);
				if(val <= 3) {
					args |= (val << 4);
				} continue;
			}

			printf_escape_code(stderr, YELLOW,  "\"%s\" is not a valid argument!\n", argv[i]); print_escape_code(stderr, RESET);
			exit(1);
		}
	}
	return dir_argc;
}

#define NEW_UNIT(old_unit) switch(old_unit) { \
	case 'K': \
		old_unit = 'M'; \
		break; \
	case 'M': \
		old_unit = 'G'; \
		break; \
	case 'G': \
		old_unit = 'T'; \
		break; \
	case 'T': \
		old_unit = 'P'; \
		break; \
	default: \
		old_unit = 'K'; \
}

// i hate even simple conversion math //
// TODO: use floor(log10(n))+1 to make this O1 //
char simplified_fsize(uint32_t fsize, float* readable_fsize) {
	*readable_fsize = 0;
	char unit = 0;
	uint8_t size_arg = SIZE_ARG(args);
	uint32_t real_fsize = fsize*512;
	if(real_fsize<(size_arg==3 ? 1000 : 1024)) {
		*readable_fsize = (float)real_fsize;
		return unit;
	}
	NEW_UNIT(unit);
	if(size_arg == 2) {
		while(real_fsize>1048576) {
			real_fsize >>= 10;
			NEW_UNIT(unit);
		} *readable_fsize = (float)real_fsize / 1024;
	} else {
		while(real_fsize>10000) {
			real_fsize /= 10;
			NEW_UNIT(unit);
		} /* hack, should be `/ 10` but shit sucks */*readable_fsize = (float)real_fsize / 1000;
	}
	return unit;
}

const char* fdescriptor_color(struct file finfo) {

	if(finfo.stat & S_IFDIR) return escape_code(stdout, BLUE);
	if(finfo.stat & S_IXUSR) return escape_code(stdout, GREEN);

	char* is_media = NULL;
	char* tok = strtok(finfo.name, "."); char* extension = NULL;
	while(tok) {
		extension = tok;
		tok = strtok(NULL, ".");
	} if(!(is_media=file_t_map->get(file_t_map, extension).p)) return "\0";

	// TODO: prob could do some boolean math here instead of 78352957 237y89523 if statements //
	if(!strcmp(is_media, "󰈟 ")) return escape_code(stdout, YELLOW);
	if(!strcmp(is_media, "󰵸 ")) return escape_code(stdout, YELLOW);
	if(!strcmp(is_media, "󰜡 ")) return escape_code(stdout, YELLOW);
	if(!strcmp(is_media, "󰈫 ")) return escape_code(stdout, YELLOW);
	if(!strcmp(is_media, "󰈫 ")) return escape_code(stdout, YELLOW);

	if(finfo.stat & S_IFREG) return "\0";
	else return escape_code(stdout, CYAN); // must be symlink //
}

// TODO: how tf does stat(1) format its readable access perms? //
// uint8_t print_file_perms(const mode_t fstat) {
// 	if(!(args & ARG_FPERMS)) {
// 		return 0;
// 	}
// 	return 0;
// }

const char* nerdfont_icon(struct file finfo) {
	if(args & ARG_NO_NERDFONTS) {
		return "\0";
	}

	char* result = NULL;
	if((result = file_t_map->get(file_t_map, finfo.name).s)) return result;
	char* tok = strtok(finfo.name, ".");
	while(tok) {
		result = tok;
		tok = strtok(NULL, ".");
	} if((result = file_t_map->get(file_t_map, result).s) && !(!strcmp(result, " "/*HACK TO GET RID OF /lib AND /usr/lib + /usr/local/lib */) && finfo.stat & S_IFDIR)) return result;

	if(finfo.stat & S_IFDIR) return " ";
	else if(finfo.stat & S_IXUSR) return " ";
	else { return " "; }
}

// zamn i was right on the string builder, 3 times faster, 1.7 ms mean //
void list_files(const struct file* files, const uint16_t longest_fdescriptor, const uint16_t fcount, const struct winsize termsize, bool (*condition)(mode_t)) {
	static uint8_t files_printed = 0;
	uint8_t files_per_row = termsize.ws_col / longest_fdescriptor;

	if(!file_t_map) init_filetype_dict();
	if(!file_t_map) {
		printf_escape_code(stderr, RED, "Failed to allocate memory for file icon and color hashmapping\n"); print_escape_code(stderr, RESET);
		return;
	}

	for(uint16_t i=0;i<fcount;++i) {
		if(condition(files[i].stat)) {
			continue;
		} else if(files[i].name[0] == '.') {
			if(!(args & ARG_DOT_DIRS) && !(strcmp(files[i].name, ".") && strcmp(files[i].name, ".."))) {
				continue;
			} else if(!(args & ARG_DOT_FILES)) {
				continue;
			}
		}

		strbuild_t sb = sb_new();
		sb_append(&sb, fdescriptor_color(files[i]));
		size_t fdescriptor_len = sb_append(&sb, nerdfont_icon(files[i]));
		sb_append(&sb, escape_code(stdout, BOLD));

		fdescriptor_len += sb_append(&sb, files[i].name);
		fdescriptor_len += sb_append(&sb, " ");

		sb_append(&sb, escape_code(stdout, RESET));

		uint8_t size_arg = SIZE_ARG(args);
		if(size_arg) {
			if(S_ISDIR(files[i].stat)) {
				if(!(args & ARG_DIR_CONTS)) {
					goto dont_list_directories;
				}
				fdescriptor_len += sb_append(&sb, "(Contains ");
			} else {
				fdescriptor_len += sb_append(&sb, "(");
			}
			if(size_arg==1) {
				char* bytes = malloc((size_t)floor(log10(files[i].size))+2);
				sprintf(bytes, "%u B)", files[i].size);
				fdescriptor_len += sb_append(&sb, bytes);
				free(bytes);
			} else {
				float hr_size = 0;
				char unit = simplified_fsize(files[i].size, &hr_size);
				char bytes[100] = {0};
				if(unit==0) {
					sprintf(bytes, "%.1f B) ", hr_size);
					fdescriptor_len += sb_append(&sb, bytes);
				} else if(size_arg==2) {
					sprintf(bytes, "%.1f %ciB) ", hr_size, unit);
					fdescriptor_len += sb_append(&sb, bytes);
				} else {
					sprintf(bytes, "%.1f %cB) ", hr_size, unit);
					fdescriptor_len += sb_append(&sb, bytes);
				}
			}

		}
		dont_list_directories:

		for(uint16_t i=longest_fdescriptor-fdescriptor_len;i>0;--i) {
			sb_append(&sb, " ");
		}
		files_printed++;
		// i tried doing this based off bytes printed, gave issues. also same with using `i % files_per_row` as it gave some segfault i dont understand to be real with you //
		if(files_printed >= files_per_row) {
			printf("%s\n", sb.str); files_printed = 0;
		} else printf("%s", sb.str);
		free(sb.str);
	}
	ht_free(file_t_map); file_t_map = NULL;
}

uint16_t get_longest_fdescriptor(const uint8_t longest_fname, const uint32_t largest_fsize) {
	uint16_t result = ((uint16_t)longest_fname)+3;

	uint8_t size_arg = SIZE_ARG(args);
	switch(size_arg) {
		case 0:
			break;
		case 1:
			result += floor(log10(largest_fsize))+1;
	 		#ifndef NDEBUG
	 		printf("The largest file %u bytes. the equation from pulled from our as- brain says thats %u base10 numbers.\n", largest_fsize, (uint32_t)floor(log10(largest_fsize))+1);
	 		#endif
	 		break;
	 	default:
			float hr_size = 0;
	 		simplified_fsize(largest_fsize, &hr_size);
	 		char largest_size[25]; // somewhere around this i didn't do the math, but shouldn't overflow //
	 		if(SIZE_ARG(args) == 2) {
	 			result += sprintf(largest_size, "%.1f XiB", hr_size);
	 		} else {
	 			result += sprintf(largest_size, "%.1f XB", hr_size);
	 		}
	} if(size_arg) {
		result += /*( ) */4;
		if(args & ARG_DIR_CONTS) result += /*Contains */9;
		result += size_arg-1;
	}
	// TODO: //
	if(args & ARG_FPERMS) result += /*<drwxr-xr-x> */13;
	if(!(args & ARG_NO_NERDFONTS)) result += /* */2;
	return result;
}

bool condition_isdir(mode_t stat) { return S_ISDIR(stat); }
bool condition_isfile(mode_t stat) { return !S_ISDIR(stat); }
bool condition_unsorted(mode_t stat) { (void)stat; return false; }

#define LIST_FILES_MAIN(unsorted_arg, files, longest_fname, largest_fsize, fcount, winsize) if(unsorted_arg) { \
			list_files(files, get_longest_fdescriptor(longest_fname, largest_fsize), fcount, winsize, condition_unsorted); \
} else { \
			list_files(files, get_longest_fdescriptor(longest_fname, largest_fsize), fcount, winsize, condition_isfile); \
			list_files(files, get_longest_fdescriptor(longest_fname, largest_fsize), fcount, winsize, condition_isdir); \
}

int main(int argc, char** argv) {
	uint16_t fcount = 0; uint8_t longest_fname = 0; uint32_t largest_fsize = 0;
	struct file* files = NULL;
	struct winsize termsize; ioctl(fileno(stdout), TIOCGWINSZ, &termsize);

	uint16_t dir_argc = argc<=1 ? 1 : parse_arguments(argc, argv);
	#ifndef NDEBUG
	printf("args: %b\nsize arg: %b\n", args, SIZE_ARG(args));
	#endif

	if(dir_argc==1) {
		files = query_files(".", &longest_fname, &largest_fsize, &fcount, files);
		LIST_FILES_MAIN(args & ARG_UNSORTED, files, longest_fname, largest_fsize, fcount, termsize);
		free(files);
		putchar('\n');
		return 0;
	}

	for(uint16_t i=1;i<argc;++i) {
		if(!dir_argc) {
			break;
		} if(argv[i][0]=='-') {
			continue;
		}
		if(argv[i][strlen(argv[i])-1]=='/' && strlen(argv[i])!=1) argv[i][strlen(argv[i])-1] = '\0';

		struct stat st;
		if(stat(argv[i], &st)==-1) {
			printf_escape_code(stderr, YELLOW,  "Could not access ");
			printf_escape_code(stderr, BLUE, "%s!", argv[i]);
			printf_escape_code(stderr, YELLOW, " (does it exist?)\n"); print_escape_code(stderr, RESET);
			continue;
		} else if(!S_ISDIR(st.st_mode)) {
			printf_escape_code(stderr, YELLOW, "Could not access ");
			printf_escape_code(stderr, BLUE, "%s!", argv[i]);
			printf_escape_code(stderr, YELLOW, " (is a file!)\n"); print_escape_code(stderr, RESET);
			continue;
		}
		print_escape_code(stdout, BLUE);
		if(!(args & ARG_NO_NERDFONTS)) printf(" ");
		printf("%s:\n", argv[i]); print_escape_code(stdout, RESET);

		fcount = 0; longest_fname = 0;
		files = query_files(argv[i], &longest_fname, &largest_fsize, &fcount, files);
		LIST_FILES_MAIN(args & ARG_UNSORTED, files, longest_fname, largest_fsize, fcount, termsize);
		dir_argc--;
		putchar('\n');
		if(dir_argc>1) putchar('\n');
	}
	free(files);
	return 0;
}
