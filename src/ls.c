#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "colors.h"
// TODO: this needs alot of refactoring //
// TODO: fix dev build segfault //

// the ENTIRE reason for this is jsut cuz i thought itd be unique and wanted to see if i could do it //
// it turned out to be not that bad and I auctually quite liked using it //
uint16_t args = 0b0;
enum arg_options {
	// booleans //
	dot_dirs = 0b1,
	dot_files = 0b10,
	unsorted = 0b100,
	no_nerdfonts = 0b1000,
	fpermissions = 0b1000000,
	dir_conts = 0b10000000,
	// u2 int //
	size_none = 0b000000,
	size_bytes = 0b010000,
	size_bin = 0b100000,
	size_si = 0b110000,
};
// this was a doozie //
#define SIZE_ARG(args) (((args) << 25)) >> 29

struct file {
	char name[256];
	uint32_t size;
	mode_t stat;
};

uint32_t dir_contents(const char* dir) {
	DIR* dfd = opendir(dir);
	if(!dfd) {
		escape_code(stderr, RED);
		fprintf(stderr, "Failed to check the size of \"");
		escape_code(stderr, BLUE); fprintf(stderr ,"%s", dir); escape_code(stderr, RED);
		fprintf(stderr, "\". (Could not open directory!)\n"); escape_code(stderr, RESET);
		return 0;
	} uint32_t result = 0;
	struct stat st; struct dirent* dp;
	char true_fname[strlen(dir)+257] = {};
	while((dp=readdir(dfd))!=NULL) {
		sprintf(true_fname, "%s/%s", dir, dp->d_name);
		#pragma GCC diagnostic ignored "-Wunused-variable"
		#pragma GCC diagnostic push
		int _ = stat(true_fname, &st);
		assert(_!=-1); // in theory stat should not fail here since the file exists: if it does fail there is programmer error involved
		#pragma GCC diagnostic pop
		if(!S_ISDIR(st.st_mode)) result += st.st_size;
	} closedir(dfd); return st.st_size;
}

#define STARTING_LEN 25
struct file* query_files(const char* dir, uint8_t* longest_fname, uint32_t* largest_fsize, uint16_t* fcount) {
	DIR* dfd = opendir(dir);
	if(!dfd) {
		escape_code(stderr, RED);
		fprintf(stderr, "Failed to query files! (memory allocation failure!)\n"); escape_code(stderr, RESET);
		return NULL;
 	} struct file* files = NULL; uint16_t array_len = STARTING_LEN;
	// TODO: dont allocate if files is allocated, instead reuse it for the loop: saving malloc calls to OS //
	if(!(files=calloc(array_len, sizeof(struct file)))) {
		escape_code(stderr, RED);
		fprintf(stderr, "Failed to query files! (memory allocation failure!)\n"); escape_code(stderr, RESET);
		closedir(dfd);
		return NULL;
	} *largest_fsize = 0; *longest_fname = 0;
	
	char true_fname[strlen(dir)+257] = {}; 
 	struct stat st; struct dirent* dp;
 	uint16_t i;
	for(i=0;(dp=readdir(dfd))!=NULL;++i) {
		if(i>=array_len) {
			if(((array_len*2)*sizeof(struct file)) < (array_len*sizeof(struct file))) {
				escape_code(stderr, RED);
				fprintf(stderr, "Failed to query files! (realloaction caused multiplication overflow!)\n"); escape_code(stderr, RESET);
				break;
			} array_len*=2;
			#ifndef NDEBUG
			printf("Rellocating %lu bytes", array_len*sizeof(struct file));
			#endif
			void* new_ptr = reallocarray(files, array_len, sizeof(struct file));
			if(!new_ptr) {
				escape_code(stderr, RED);
				fprintf(stderr, "Failed to query files! (reallocation failure!)\n"); escape_code(stderr, RESET);
				break;
			} files = (struct file*)new_ptr;
		}
		sprintf(true_fname, "%s/%s", dir, dp->d_name);

		#pragma GCC diagnostic ignored "-Wunused-variable"
		#pragma GCC diagnostic push
		int _ = stat(true_fname, &st);
		assert(_!=-1); // in theory stat should not fail here since the file exists: if it does fail there is programmer error involved
		#pragma GCC diagnostic pop
		
		files[i].stat = st.st_mode;
		// TODO: change to blocks //
		if(!S_ISDIR(st.st_mode)) {
			files[i].size = st.st_size;
		} else {
			files[i].size = dir_contents(true_fname);
		}
		if(st.st_size > *largest_fsize) *largest_fsize = st.st_size;

		strcpy(files[i].name, dp->d_name);
		uint8_t fname = (uint8_t)strlen(dp->d_name);
		if(fname > *longest_fname) *longest_fname = fname;
	}
	
	closedir(dfd);
	*fcount = i;
	return files;
}

// inline void print_help(void) {
// 	
// }

// returns the amount of auctual non-argument entires there are so we can just skip past em //
int parse_arguments(const int argc, char** argv) {
	int dir_argc = 0;
	enum arg_options arg;
	for(int i=0;i<argc;++i) {
		if(argv[i][0]!='-') {
			dir_argc++;
			continue;
		}
		// TODO: hashmap //
		if(!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all") || !strcmp(argv[i], "-f") || !strcmp(argv[i], "--full")) {
			args |= dot_dirs | dot_files;
		} else if(!strcmp(argv[i], "-A") || !strcmp(argv[i], "--almost-all")) {
			args |= dot_files;
		} else if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dot-dirs")) {
			args |= dot_dirs;
		} else if(!strcmp(argv[i], "-nf") || !strcmp(argv[i], "--no-nerdfonts")) {
			args |= no_nerdfonts;
		} else if(!strcmp(argv[i], "-c") || !strcmp(argv[i], "--dir-contents")) {
			args |= dir_conts;
		} else if(!strcmp(argv[i], "-hr") || !strcmp(argv[i], "--human-readable")) {
			if(argv[i+1]==nullptr) {
				escape_code(stderr, YELLOW);
				printf("\"%s\" is missing an argument on what type! (assumed none2).\n", argv[i]); escape_code(stderr, RESET);
				return dir_argc;
			} ++i;
			switch(argv[i][0]) {
				case 's':
					args |= size_si;
					break;
				case 'n':
					args |= size_none;
					break;
				case 'b':
					if(!strcmp(argv[i], "bin")) {
						args |= size_bin;
					} else {
						args |= size_bytes;
					} break;
				default:
					uint8_t val = (uint8_t)atoi(argv[i]);
					if(val <= 3) {
						args |= (val << 4);
					}
			}
		} else if(!strcmp(argv[i], "-U") || !strcmp(argv[i], "--unsorted")) {
			args |= unsorted;
		} else {
			// FIXME: this causes segfault on dev build and only dev build //
			// for some reason we need 2 seperate buffers, and I have no clue why //
			char humanreadable_arg[strlen(argv[i])] = {}; char hr_arg[strlen(argv[i])] = {};
			if(!strcmp(strncpy(humanreadable_arg, argv[i], 17), "--human-readable=")) {
				// atoi can fail here but it returns with 0 so if your wrong we assume default //
				uint8_t val =(uint8_t)atoi(strncpy(humanreadable_arg, argv[i]+17, 1));
				if(val <= 3) {
					args |= (val << 4);
				}
				continue;
			} else if(!strcmp(strncpy(hr_arg, argv[i], 4), "-hr=")) {
				uint8_t val = (uint8_t)atoi(strncpy(hr_arg, argv[i]+4, 1));
				if(val <= 3) {
					args |= (val << 4);
				}
				continue;
			}
			escape_code(stderr, YELLOW);
			fprintf(stderr, "\"%s\" is not a valid argument!\n", argv[i]); escape_code(stderr, RESET);
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
char simplified_fsize(uint32_t fsize, float* readable_fsize) {
	enum arg_options opt;
	*readable_fsize = 0;
	char unit = 0;
	uint8_t size_arg = SIZE_ARG(args);
	if(fsize<(size_arg==3 ? 1000 : 1024)) {
		*readable_fsize = (float)fsize;
		return unit;
	}
	NEW_UNIT(unit);
	if(size_arg == 2) {
		while(fsize>1048576) {
			fsize >>= 10;
			NEW_UNIT(unit);
		} *readable_fsize = (float)fsize / 1024;
	} else {
		while(fsize>10000) {
			fsize /= 10;
			NEW_UNIT(unit);
		} /* hack, should be `/ 10` but shit sucks */*readable_fsize = (float)fsize / 100;
	}
	return unit;
}

void list_files(const struct file* files, const uint16_t longest_fdescriptor, const uint16_t fcount, const struct winsize termsize, bool (*condition)(mode_t)) {
	enum arg_options opt;
	static uint8_t files_printed = 0;
	uint8_t files_per_row =  termsize.ws_col / longest_fdescriptor;
	for(uint16_t i=0;i<fcount;++i) {
		if(condition(files[i].stat)) {
			continue;
		} else if(files[i].name[0] == '.') {
			if(!(args & dot_dirs) && (strcmp(files[i].name, ".") && strcmp(files[i].name, ".."))) {
				continue;
			} else if(!(args & dot_files)) {
				continue;
			}
		} 
		
		uint16_t fdescriptor = 0;
		bool is_dir = S_ISDIR(files[i].stat);
		if(is_dir) escape_code(stdout, BLUE);
		if(!(args & no_nerdfonts)) {
			if(is_dir) {
				fdescriptor += printf(" ");
			} else {
				fdescriptor += printf(" ");
			}
		} escape_code(stdout, BOLD);
		fdescriptor += printf("%s ", files[i].name); escape_code(stdout, RESET);
		uint8_t size_arg = SIZE_ARG(args); 
		if(size_arg) {
			if(is_dir) {
				if(!(args & dir_conts)) {
					goto dont_list_directories;
				} fdescriptor += printf("(Contains ");
			} else {
				fdescriptor += printf("(");				
			}
			if(size_arg==1) {
				fdescriptor += printf("%u B)", files[i].size);
			} else {
				float hr_size = 0;
				char unit = simplified_fsize(files[i].size, &hr_size);
				if(unit==0) {
					fdescriptor += printf("%.1f B) ", hr_size);
				} else if(size_arg==2) {
					fdescriptor += printf("%.1f %ciB) ", hr_size, unit);
				} else {
					fdescriptor += printf("%.1f %cB) ", hr_size, unit);
				}
			}	
		}
		dont_list_directories:

		// printf("%u-%u=%u", longest_fdescriptor, fdescriptor, longest_fdescriptor-fdescriptor);
		for(uint16_t i=longest_fdescriptor-fdescriptor;i>0;--i) {
			putchar(' ');
		}
		files_printed++;
		// i tried doing this based off bytes printed, gave issues. also same with using `i % files_per_row` as it gave some segfault i dont undestand to be real with you //
		if(files_printed >= files_per_row) {
			putchar('\n'); files_printed = 0;
		}
	}
}

uint16_t get_longest_fdescriptor(const uint8_t longest_fname, const uint32_t largest_fsize) {
	enum arg_options opt;
	uint16_t result = ((uint16_t)longest_fname)+2;

	uint8_t size_arg = SIZE_ARG(args);
	switch(size_arg) {
		case 0:
			break;
		case 1:
			result += floor(log10(largest_fsize))+1; // if I could get this equation to auctually be right then I'd like to use it to speed up `simplified_fsize(size_t, float*)`
	 		#ifndef NDEBUG
	 		printf("The largest file %u bytes. the equation from pulled from our as- brain says thats %u base10 numbers.\n", largest_fsize, result-((uint16_t)longest_fname)+1);
	 		#endif
	 		break;
	 	default:
			float hr_size = 0;
	 		simplified_fsize(largest_fsize, &hr_size);
	 		char largest_size[25]; // somewhere around this i didn't do the math, but shouldn't overflow //
	 		if(args & size_bin) {
	 			result += sprintf(largest_size, "%.1f XiB", hr_size);
	 		} else {
	 			result += sprintf(largest_size, "%.1f XB", hr_size);
	 		}		 
	} if(size_arg) {
		result += /*( ) */4;
		if(args & dir_conts) result += /*Contains */9;
		result += size_arg;
	}
	if(args & fpermissions) result += /*<drwxr-xr-x>*/12;
	if(!(args & no_nerdfonts)) result += /* */2;
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
	struct winsize termsize; ioctl(STDOUT_FILENO, TIOCGWINSZ, &termsize);

	int dir_argc = argc<=1 ? 1 : parse_arguments(argc, argv);
	// #ifndef NDEBUG
	printf("args: %b\nsize arg: %b\n", args, SIZE_ARG(args));
	// #endif

	if(dir_argc==1) {
		files = query_files(".", &longest_fname, &largest_fsize, &fcount);
		LIST_FILES_MAIN(args & unsorted, files, longest_fname, largest_fsize, fcount, termsize);
		free(files);
		putchar('\n');
		return 0;
	}

	for(int i=1;i<argc;++i) {
		if(!dir_argc) {
			break;
		} if(argv[i][0]=='-') {
			continue;
		}
		if(argv[i][strlen(argv[i])-1]=='/' && strlen(argv[i])!=1) argv[i][strlen(argv[i])-1] = '\0';

		struct stat st;
		if(stat(argv[i], &st)==-1) {
			escape_code(stderr, YELLOW);
			fprintf(stderr, "Could not access "); escape_code(stderr, BLUE); fprintf(stderr, "%s!", argv[i]);
			escape_code(stderr, YELLOW); fprintf(stderr, " (does it exist?)\n"); escape_code(stderr, RESET);
		} else if(!S_ISDIR(st.st_mode)) {
			escape_code(stderr, YELLOW);
			fprintf(stderr, "Could not access "); escape_code(stderr, BLUE); fprintf(stderr, "%s!", argv[i]);
			escape_code(stderr, YELLOW); fprintf(stderr, " (is a file!)\n"); escape_code(stderr, RESET);
			continue;
		}

		fcount = 0; longest_fname = 0;
		// TODO: change so files is not reallocated for each iteration of the loop, this is a waste of resources //
		files = query_files(argv[i], &longest_fname, &largest_fsize, &fcount);
		LIST_FILES_MAIN(args & unsorted, files, longest_fname, largest_fsize, fcount, termsize);		
		free(files);
		dir_argc--;
	} putchar('\n');
	return 0;
}