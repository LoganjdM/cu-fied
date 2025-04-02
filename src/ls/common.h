#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <table.h>
#include <string.h>
#include <unistd.h>

#ifndef C23
#	include <stdbool.h>
#endif

#include <stdint.h>

// note my original comment here about this binary math being a big of expertimentation on trying a new way of storing args. this is more of me fucking around and figuring out a new way of doing things I like //
// it personally is quite nice imo, though because of that and knowing me: i would not be surprised if I just reinvented something that's been done a million times before without me knowing //
struct args {
	uint16_t args;

	enum {
		dot_dirs =     0b1,
		dot_files =    0b10,
		#define ARG_SORT(self)    (self >> 2 & 0b11)
		no_nerdfont =  0b10000,
		include_stat = 0b100000,
		#define ARG_HR(self)      (self >> 6 & 0b11)
		dir_contents = 0b10000000,
		#define ARG_RECURSE(self) (self >> 8 & 0xFF)
	} arg;
	
	char* operandv[0xFFFF];
	uint16_t operandc;
};

typedef struct {
	char* name;

	bool ok_st;
	struct stat stat;
} file_t;

float simplify_file_size(const size_t f_size, char* unit, const struct args args);

size_t get_longest_f_string(const file_t* files, const size_t file_len, const struct args args);

char* get_nerdicon(file_t file, table_t* f_ext_map, const struct args args);

bool list_files(file_t* files, const size_t file_len,
				const size_t longest_string,
				table_t* f_ext_map,
				bool (*condition)(mode_t),
				const uint32_t f_per_row,
				const struct args args);