#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#ifndef C23
#	include <stdbool.h>
#endif
#include <math.h>
#include <string.h>
#include <strbuild.h>

#include "../stat/do_stat.h"
#include "../libcolor/colors.h"
#include "common.h"

#define STRDUPA_IMPLEMENTATION
#	define STRDUPA_IMPLEMENTED
#	include "../polyfill.h"

float simplify_file_size(const size_t f_size, char* unit, const struct args args) {
	*unit = 0;
	// this shows ULONG_MAX on my system is 19 digits long, perfectly representable by 8 bits //
	// int main(void) {
	//  printf("%f", floor(log10((double)ULONG_MAX))+1);
	//  return 0;
	// }
	uint8_t exp = f_size != 0 ? floor(log10(f_size)) : 0;
	if (exp+1 < 3) return (float)f_size;

	switch (exp) {
		case 4:
			*unit = 'K';
			break;
		case 5:
			*unit = 'M';
			break;
		case 6:
			*unit = 'G';
			break;
		case 7:
			*unit = 'T';
			break;
		case 8:
			*unit = 'P';
			break;
		case 9:
			*unit = 'E';
			break;
		case 10:
			*unit = 'Y'; // what are you, google 150 years from now?
			break;
	}

	// si(x) = x/10^(floor∘log₁₀)(x) //
	if (ARG_HR(args.args) == 3)
		return (float)f_size / pow(10, exp);
	// TODO: i suck at math //
	else return 0;
	
}

size_t get_longest_f_string(const file_t* files, const size_t file_len, const struct args args) {
	assert(files);
	size_t longest_f_name = 0, longest_f_size = 0;
	for (size_t i=0; i<file_len-1; ++i) {
		size_t f_name_len = strlen(files[i].name);
		if (f_name_len > longest_f_name) longest_f_name = f_name_len;
		if ((size_t)files[i].stat.st_size > longest_f_size) longest_f_size = files[i].stat.st_size;
	}

	size_t result = longest_f_name + 3;
	uint8_t arg_hr = ARG_HR(args.arg);
	switch (arg_hr) {
		case 0: break;
		case 1:
			result += floor(log10(longest_f_size)) + 1;
			break;
		default:
			float size_hr = simplify_file_size(longest_f_size, (char*)alloca(1), args);
			char longest_hr_f_size[100] = {0};
			if (arg_hr == 2) result += snprintf(longest_hr_f_size, 100, "%.1f XiB", size_hr);
			else result += snprintf(longest_hr_f_size, 100, "%.1f XB", size_hr);

			result += 5;//( ) //
			if (args.args & dir_contents) result += 9;//Contains //
	}

	if (args.args & include_stat) result += 13;//<drwxr-xr-x> //
	if (!(args.args & no_nerdfont)) result += 2;// //
	return result;
}

#define S_ISEXE(mode) (mode & S_IXUSR || mode & S_IXOTH || mode & S_IXGRP)
const char* get_descriptor_color(const file_t file, table_t* f_ext_map, const struct args args) {
	if (S_ISDIR(file.stat.st_mode)) return get_escape_code(STDOUT_FILENO, BLUE);
	else if (S_ISEXE(file.stat.st_mode))
		return get_escape_code(STDOUT_FILENO, GREEN);

	if (!(args.args & no_nerdfont)) {
		char* media = NULL; char* ext= NULL;

		char* f_name_copy = strdupa(file.name);
		
		for (char* tok = strtok(f_name_copy, "."); tok; tok = strtok(NULL, ".")) ext = tok;
		if (!(media=f_ext_map->get(f_ext_map, ext).p)) return "\0";

		if(!strcmp(media, "󰈟 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		if(!strcmp(media, "󰵸 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		if(!strcmp(media, "󰜡 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
		if(!strcmp(media, "󰈫 ")) return get_escape_code(STDOUT_FILENO, YELLOW);
	}

	if (S_ISREG(file.stat.st_mode)) return "\0";
	else return get_escape_code(STDOUT_FILENO, CYAN); // prob standard symlink //
}

char* get_nerdicon(file_t file, table_t* f_ext_map, const struct args args) {
	if (args.args & no_nerdfont) return "\0";

	char* result = "\0";
	if ((result = f_ext_map->get(f_ext_map, file.name).s)) return result;

	char* f_name_copy = strdupa(file.name);
	char* ext = NULL;
	for (char* tok = strtok(f_name_copy, "."); tok; tok = strtok(NULL, "."))
		ext = tok;
	if (ext && (ext = f_ext_map->get(f_ext_map, ext).s)) return ext;
	
	if (S_ISDIR(file.stat.st_mode)) return " ";
	if (S_ISEXE(file.stat.st_mode)) return " ";
	else return " ";
	
}


bool list_files(file_t* files, const size_t file_len,
				const size_t longest_string,
				table_t* f_ext_map,
				bool (*condition)(mode_t),
				const uint32_t f_per_row,
				const struct args args) {
	assert(f_ext_map || (args.args & no_nerdfont));
	assert(files);

	bool fucky_wucky = false;
	static size_t printed = 0;
	for (size_t i=0; i<file_len-1; ++i) {
		#define FILE files[i]
		if (condition(FILE.stat.st_mode)) continue;
		else if (FILE.name[0] == '.') {
			if (!(args.args & dot_dirs) && !(strcmp(FILE.name, ".") && strcmp(FILE.name, ".."))) continue;
			else if (!(args.args & dot_files)) continue;
		}

		strbuild_t sb = sb_new();
		size_t string_len = 0;
		sb_append(&sb, get_descriptor_color(FILE, f_ext_map, args));
		string_len += sb_append(&sb, get_nerdicon(FILE, f_ext_map, args));
		sb_append(&sb, get_escape_code(STDOUT_FILENO, BOLD));

		string_len += sb_append(&sb, FILE.name);
		string_len += sb_append(&sb, " ");
		sb_append(&sb, get_escape_code(STDOUT_FILENO, RESET));
		if (args.args & include_stat) {
			string_len += sb_append(&sb, "<");
			string_len += sb_append(&sb, get_readable_mode(FILE.stat.st_mode));
			string_len += sb_append(&sb, ">");
		}

		uint8_t arg_hr = ARG_HR(args.args);

		if (!arg_hr) goto dont_list_size;
		else if (S_ISDIR(FILE.stat.st_mode)) {
			if(!(args.args & dir_contents)) goto dont_list_size;
			else string_len += sb_append(&sb, "(Contains ");
		} else string_len += sb_append(&sb, "(");

		char* file_size = NULL;
		if (arg_hr == 1) {
			if (!(file_size = malloc(floor(log10(FILE.stat.st_blocks)) + 5))) {
				fucky_wucky = true;
				continue;
			}
			#ifdef __APPLE__
			// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html //
			sprintf(file_size, "%lli B", FILE.stat.st_blocks);
			#else
			sprintf(file_size, "%lu B", FILE.stat.st_blocks);
			#endif
		} else {
			char unit = 0;
			const float size_hr = simplify_file_size(FILE.stat.st_blocks, &unit, args);
			#define MAX_FILE_SIZE_HR_LEN 100
			if (!(file_size = malloc(MAX_FILE_SIZE_HR_LEN))) {
				fucky_wucky = true;
				continue;
			}

			if (unit == 0) snprintf(file_size, MAX_FILE_SIZE_HR_LEN, "%.0f Blocks) ", size_hr);
			else if (arg_hr == 2) snprintf(file_size, MAX_FILE_SIZE_HR_LEN, "%.1f %ciB) ", size_hr, unit);
			else snprintf(file_size, MAX_FILE_SIZE_HR_LEN, "%.1f %cB) ", size_hr, unit);
			#undef MAX_FILE_SIZE_HR_LEN
			string_len += sb_append(&sb, file_size);
		} free(file_size);
		
		dont_list_size:

		for (size_t	 i=longest_string-string_len; i>0; --i) sb_append(&sb, " ");
		++printed;
		
		printf("%s", sb.str);
		if (printed >= f_per_row) {
			printf("\n");
			printed = 0;
		}
		fflush(stdout);
		free(sb.str);
	} return fucky_wucky;
}
