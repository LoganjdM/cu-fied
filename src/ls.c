#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "colors.h"

unsigned short args = 0b0;
enum arg_options {
	dot_dirs = 0b1,
	dot_files = 0b10,
	unsorted = 0b100,
};

struct file {
	char name[256];
	size_t size;
	mode_t stat;
};

#define STARTING_LEN 25
void* query_files(const char* dir, unsigned char* longest_fname,size_t* fcount) {
	DIR* dfd = opendir(dir);
	if(!dfd) {
		escape_code(stderr, RED);
		fprintf(stderr, "Failed to query files! (memory allocation failure!)\n" RESET);
		return NULL;
 	} struct file* files = NULL; size_t array_len = STARTING_LEN;
	if(!(files=calloc(array_len, sizeof(struct file)))) {
		escape_code(stderr, RED);
		fprintf(stderr, "Failed to query files! (memory allocation failure!)\n" RESET);
		closedir(dfd);
		return NULL;
	}
	
	char true_fname[strlen(dir)+256]; 
 	struct stat st; struct dirent* dp;
 	size_t i;
	for(i=0;(dp=readdir(dfd))!=NULL;++i) {
		if(i>=array_len) {
			if(((array_len*2)*sizeof(struct file)) < (array_len*sizeof(struct file))) {
				escape_code(stderr, RED);
				fprintf(stderr, "Failed to query files! (realloaction caused multiplication overflow!)\n" RESET);
				break;
			} array_len*=2;
			#ifndef NDEBUG
			printf("Rellocating %lu bytes", array_len*sizeof(struct file));
			#endif
			void* new_ptr = reallocarray(files, array_len, sizeof(struct file));
			if(!new_ptr) {
				escape_code(stderr, RED);
				fprintf(stderr, "Failed to query files! (reallocation failure!)\n" RESET);
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
		files[i].size = st.st_size;
		strcpy(files[i].name, dp->d_name);
		unsigned char fname = (unsigned char)strlen(dp->d_name);
		if(fname > *longest_fname) *longest_fname = fname;
	}
	
	closedir(dfd);
	*fcount = i;
	return files;
}

// returns the amount of auctual non-argument entires there are so we can just skip past em //
int parse_arguments(int argc, char** argv) {
	int dir_argc = 0;
	enum arg_options arg;
	for(int i=0;i<argc;++i) {
		if(argv[i][0]!='-') {
			dir_argc++;
			continue;
		}
		// TODO: hashmap? //
		if(!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all")) {
			args = args | dot_dirs | dot_files;
		} else if(!strcmp(argv[i], "-A") || !strcmp(argv[i], "--almost-all")) {
			args = args | dot_files;
		} else if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dot-dirs")) {
			args = args | dot_dirs;
		} else {
			escape_code(stderr, YELLOW);
			fprintf(stderr, "\"%s\" is not a valid argument!\n" RESET, argv[i]);
			exit(1);
		}
	}
	return dir_argc;
}

void list_files(const struct file* files, const unsigned int longest_fdescriptor, const size_t fcount, const struct winsize termsize) {
	#ifndef NDEBUG
	printf("args: %b\n", args & dot_dirs);
	#endif
	enum arg_options opt;
	const size_t start = (args & dot_dirs) ? 0 : 2;
	unsigned char files_printed = 0;
	unsigned int files_per_row =  termsize.ws_col / longest_fdescriptor;
	for(size_t i=start;i<fcount;++i) {
		if((files[i].name[0] == '.' && !(args & dot_files)) && (strcmp(files[i].name, ".") && strcmp(files[i].name, ".."))) {
			continue;
		} unsigned int fdescriptor = printf("%s ", files[i].name);
		for(int i=longest_fdescriptor-fdescriptor;i>0;--i) {
			putchar(' ');
		} files_printed++;
		// i tried doing this based off bytes printed, gave issues. also same with using `i % files_per_row` as it gave some segfault i dont undestand to be real with you //
		if(files_printed >= files_per_row) {
			putchar('\n'); files_printed = 0;
		}
	} putchar('\n');
}

int main(int argc, char** argv) {
	size_t fcount = 0; unsigned char longest_fname = 0;
	struct file* files = NULL;
	struct winsize termsize; ioctl(STDOUT_FILENO, TIOCGWINSZ, &termsize);

	const size_t starting_pos = (args | dot_dirs) ? 0 : 2;
	int dir_argc = argc<=1 ? 1 : parse_arguments(argc, argv);
	
	if(dir_argc==1) {
		files = query_files(".", &longest_fname, &fcount);
		list_files(files, longest_fname+1, fcount, termsize);
		free(files);
		return 0;
	}

	for(int i=1;i<argc;++i) {
		if(!dir_argc) {
			break;
		} if(argv[i][0]=='-') {
			continue;
		}
		if(argv[i][strlen(argv[i])-1]=='/') argv[i][strlen(argv[i])-1] = '\0';

		struct stat st;
		if(stat(argv[i], &st)==-1) {
			escape_code(stderr, YELLOW);
			fprintf(stderr, "Could not access " BLUE "%s!" YELLOW " (does it exist?)\n" RESET, argv[i]);
		} else if(!S_ISDIR(st.st_mode)) {
			escape_code(stderr, YELLOW);
			fprintf(stderr, "Could not access " BLUE "%s!" YELLOW " (is a file!)\n" RESET, argv[i]);
			continue;
		}

		fcount = 0; longest_fname = 0;
		files = query_files(argv[i], &longest_fname, &fcount);
		list_files(files, longest_fname+1, fcount, termsize);
		free(files);
		dir_argc--;
	} return 0;
}