#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#ifndef __has_embed
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "../app_info.h"
#include "file_types.h"

short args = 0b0;
#define ARG_FORCE 0b1

table_t* file_types = NULL;
// TODO: automate this and allocate these buffers on the heap //
bool init_table(void) {
	file_types = ht_create(25);
	if(!file_types) return false;

	file_types->put(file_types, "c", c_example);
	file_types->put(file_types, "cpp", cpp_example);
	file_types->put(file_types, "lua", lua_example);
	file_types->put(file_types, "tl", teal_example);
	file_types->put(file_types, "py", python_example);
	file_types->put(file_types, "cs", csharp_example);
	file_types->put(file_types, "zig", zig_example);
	file_types->put(file_types, "txt", txt_example);
	file_types->put(file_types, "md", markdown_example);

	return true;
}

#define ISARG(x, s, l) !strcmp(x, s) | !strcmp(x, l)
bool parse_args(const char* arg) {
	if(arg[0]=='-') {
		if(ISARG(arg, "-h", "--help")) {
			#ifndef __has_embed
			const char help_message[] = "\0";
			#else
			const char help_message [] = {
				#embed "help.txt"
				, '\0'
			};
			#endif
			puts(help_message);
			exit(0);
		} else if(ISARG(arg, "-v", "--version")) {
			puts(__CU_FIED_VERSION__);
			exit(0);
		} else if(ISARG(arg, "-f", "--force")) {
			args |= ARG_FORCE;
		}
	} return false;
}

int main(int argc, char** argv) {
	if(argc<=1) {
		fprintf_color(stderr, RED, "You need at least 1 file to create!");
		return -1;
	}
	if(!init_table()) 
		fprintf_color(stderr, RED, "Failed to allocate memory for file type table!");

	for(int i=1; i<argc; ++i) {
		if(!parse_args(argv[i])) continue;
	} for(int i=1; i<argc; ++i) {
		if(parse_args(argv[i])) continue;

		if(!(args & ARG_FORCE)) {
			struct stat st = {0};
			if(stat(argv[i], &st)==0) continue; // stat succeeded, file exists
		}

		FILE* fp = fopen(argv[i], "w");
		if(!fp) {
			fprintf_color(stderr, RED, "Failed to fill file %s! (%s)", argv[i], strerror(errno));
			continue;
		}

		char* tok = strtok(argv[i], "."); char* file_extension = NULL;
		while(tok) {
			file_extension = tok;
			tok = strtok(NULL, ".");
		}

		char* fileconts = NULL;
		if((fileconts = file_types->get(file_types, file_extension).s)) {
			fputs(fileconts, fp);
		}

		fclose(fp);
	} ht_free(file_types);
	return 0;
}
