#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifndef __has_embed
#	include <stdbool.h>
#endif

#include "../colors.h"
#include "file_types.h"

table_t* file_types = NULL;
// TODO: automate this and allocate these buffers on the heap //
bool init_table(void) {
	file_types = ht_create(25);
	if(!file_types) return false;

	file_types->put(file_types, "c", c_hello);	
	file_types->put(file_types, "cpp", cpp_hello);	
	file_types->put(file_types, "lua", lua_hello);
	file_types->put(file_types, "tl", teal_hello);
	file_types->put(file_types, "py", python_hello);
	file_types->put(file_types, "cs", csharp_hello);
	file_types->put(file_types, "zig", zig_hello);
	
	return true;
}

int main(int argc, char** argv) {
	if(argc<=1) {
		print_escape_code(stderr, RED);
		puts("You need at least 1 file to create!");
		print_escape_code(stderr, RESET);
		return -1;
	}
	if(!init_table()) {
		print_escape_code(stderr, RED);
		puts("Failed to allocate memory for file type table!");
		print_escape_code(stderr, RESET);
	}

	for(int i=1; i<argc; ++i) {
		FILE* fp = fopen(argv[i], "w");
		if(!fp) {
			print_escape_code(stderr, RED);
			printf("Failed to fill file %s! (%s)\n", argv[i], strerror(errno));
			print_escape_code(stderr, RESET);
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
	}
	return 0;
}