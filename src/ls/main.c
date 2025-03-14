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
#include <math.h> // floor() and log10()
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
					// atoi(3) can fail, but "0 on error", so we assume none //
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

file_t* query_files(const char* path, uint8_t* longest_fname, size_t* largest_fsize, size_t* f_count, const args_t args, const uint16_t recurse) {
	if (!(args & ARG_RECURSIVE)) {
		if (recurse >= 1) {
			errno = 22;
			return NULL;
		}
	}
	DIR* dfd = opendir(path);
	if (!dfd) return NULL;

	#define DA_STARTING_LEN 32
	size_t da_len = DA_STARTING_LEN;
	file_t* result = (file_t*)calloc(da_len, sizeof(file_t));
	// I'd normally use a goto here but since we allocate a bunch of variable sized var's on the stack, we can't guarantee our scope to be constant size... but now that I think about it whenever I use goto, i use it like a defer... god damn i wish this old ass language had some of zig's features(yes i'm aware defer was proposed to the C standard) //
	if (!result) {
		closedir(dfd);
		return NULL;
	}

	char true_path[strlen(path) + 257] = {};
	struct stat st = {}; struct dirent* dp = NULL;
	for (*f_count=0; (dp=readdir(dfd))!=NULL; ++*f_count) {
		if (*f_count >= da_len) {
			// I feel like (2da_len)/2 may get sneakily optimized out by -O... //
			if (*f_count != 0 && (2*da_len)/2 != *f_count) {
				errno = EOVERFLOW;
				break;
			} da_len*=2;

			void* da_new_sheisse = reallocarray(result, da_len, sizeof(file_t));
			if (!da_new_sheisse) break;
			else result = (file_t*)da_new_sheisse;
		}

		sprintf(true_path, "%s/%s", path, dp->d_name);
		stat(true_path, &st);

		result[*f_count].stat = st.st_mode;
		if (!S_ISDIR(st.st_mode))
			result[*f_count].size = (HR_ARG(args) == 1) ? st.st_size : st.st_blocks;
		else {
			// TODO: this is how we will do recursion flag, but for now just ignore it and free this pointer //
			size_t dir_conts_size = 0, junk1 = 0;
			uint8_t junk0 = 0;
			// prevent endless recursion caused by forever checking "./.", '\000' <repeats 254 times> //
			// FIXME: this is very fucking slow, infact, this ENTIRE function should be rewritten to use fts(3), as that would be quicker than stat'ing and using dirent. //
			// FIXME: infact, I have not checked ls src code in a bit, but I wonder if they even do recursion on --recursive or just us fts. //
			if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
				free(query_files(true_path, &junk0, &dir_conts_size, &junk1, args, recurse + 1));
				errno = 0; // < this sucks, essentially throws awaay whatever error we had //
			}
		
			result[*f_count].size = dir_conts_size;
		}

		if (result[*f_count].size > *largest_fsize) *largest_fsize = st.st_size;

		strcpy(result[*f_count].name, dp->d_name);
		uint8_t fname = (uint8_t)strlen(dp->d_name);
		if (fname > *longest_fname) *longest_fname = fname;
	}

	closedir(dfd);
	return result;
}

float get_simplified_file_size(const size_t f_size, char* unit, args_t args) {
	assert(*unit == 0);

 	// his shows ULONG_MAX on my system is 19 digits long, perfectly representable by 8 bits //
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
	else (float)f_size * ( pow(2 * exponent, 10) ); // TODO: base2 math would make this faster and be more accurate //
}

size_t get_longest_fdescriptor(const uint8_t longest_fname, const size_t largest_fsize, const args_t args) {
	// assert(largest_fsize != 0);
	
	size_t result = (size_t)longest_fname + 3;

	u(2) hr_arg = HR_ARG(args);
	switch (hr_arg) {
		case 0: break;
		case 1:
			result += floor(log10(largest_fsize)) + 1;
			break;
		default:
			char junk = 0;
			float hr_size = get_simplified_file_size(largest_fsize, &junk, args);
			// "somewhere around this ... shouldn't overflow" am I stupid? if i'm not certain i should use snprintf not sprintf to truly prevent overflow //
			char largest_hr_fsize[25] = {0};
			if (hr_arg == 2) result += snprintf(largest_hr_fsize, 25, "%.1f XiB", hr_size);
			else result += snprintf(largest_hr_fsize, 25, "%.1f XB", hr_size);

			result += /*( ) */4;
			if (args & ARG_DIR_CONTS) result += /*Contains */9;
	}

	if (args & ARG_STAT) result += /*<drwxr-xr-x> */13;
	if (!(args & ARG_NO_NERDFONTS)) result += /* */2;
	return result;
}

const char* get_descriptor_color(file_t f_info, table_t* f_ext_map) {
	if(f_info.stat & S_IFDIR) return get_escape_code(STDOUT_FILENO, BLUE);
	else if(f_info.stat & S_IXUSR) return get_escape_code(STDOUT_FILENO, GREEN);

	char* is_media = NULL; char* extension = NULL;
	char* tok = strtok(f_info.name, ".");
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

	if(f_info.stat & S_IFREG) return "\0";
	else return get_escape_code(STDOUT_FILENO, CYAN); // must be symlink //
}

const char* get_nerdfont_icon(file_t f_info, table_t* f_ext_map, const args_t args) {
	if (args & ARG_NO_NERDFONTS) return "\0";

	char* result = NULL;
	if ((result = f_ext_map->get(f_ext_map, f_info.name).s)) return result;

	char* tok = strtok(f_info.name, ".");
	while (tok) {
		result = tok;
		tok = strtok (NULL, ".");
	} if((result = f_ext_map->get(f_ext_map, result).s) &&
		 !(!strcmp(result, " ") && S_ISDIR(f_info.stat)) /*HACK TO GET RID OF `lib` DIRS */ ) return result;

	if(S_ISDIR(f_info.stat)) return " ";
	else if(f_info.stat & S_IXUSR) return " ";
	else return " ";
}

// oh god i am reading too much GNU source code //
bool list_files(const file_t* files,
				const size_t longest_descriptor, const size_t f_count,
				const uint32_t files_per_row,
				table_t* f_ext_map, 
				bool (*condition)(mode_t),
				args_t args) {
	assert(f_ext_map);

	static size_t files_printed = 0;
	for (size_t i=0; i<f_count; ++i) {
		#define FILE files[i]
		if (condition(FILE.stat)) continue;
		else if (FILE.name[0] == '.') {
			if (!(args & ARG_DOT_DIRS) && !(strcmp(FILE.name, ".") && strcmp(FILE.name, ".."))) continue;
			else if (!(args & ARG_DOT_FILES)) continue;
		}

		strbuild_t sb = sb_new();
		size_t descriptor_len = 0;
		
		sb_append(&sb, get_descriptor_color(FILE, f_ext_map));
		descriptor_len += sb_append(&sb, get_nerdfont_icon(FILE, f_ext_map, args));
		sb_append(&sb, get_escape_code(STDOUT_FILENO, BOLD));

		descriptor_len += sb_append(&sb, FILE.name);
		descriptor_len += sb_append(&sb, " ");
		sb_append(&sb, get_escape_code(STDOUT_FILENO, RESET));

		// ehh _BitInt(2) is nice and explicit but I'm aware compile will make this an i8 //
		u(2) hr_arg = HR_ARG(args);
		if (hr_arg) {
			if (S_ISDIR(FILE.stat)) {
				if (!(args & ARG_DIR_CONTS)) goto skip_dirs;
				else descriptor_len += sb_append(&sb, "(Contains ");
			} else descriptor_len += sb_append(&sb, "(");

			char* file_size = NULL;
			if (hr_arg == 1) {
				if (!(file_size = malloc(floor(log10(FILE.size + 1)) + 4))) return false;
				sprintf(file_size, "%lu B)", FILE.size);
			} else {
				char unit = 0;
				const float hr_size = get_simplified_file_size(FILE.size, &unit, args);
				#define ARBITRARY_SIZE 100
				if (!(file_size = malloc(ARBITRARY_SIZE))) return false;

				// file system block of 8 KiB bytes on linux //
				if (unit == 0) snprintf(file_size, ARBITRARY_SIZE, "%.0f Blocks)", hr_size);
				else if (hr_arg == 2) snprintf(file_size, ARBITRARY_SIZE, "%.1f %ciB", hr_size, unit);
				else snprintf(file_size, ARBITRARY_SIZE, "%.1f %cB", hr_size, unit);
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
	return true;
}

bool condition_isdir(mode_t stat) { return S_ISDIR(stat); }
bool condition_isndir(mode_t stat) { return !S_ISDIR(stat); }
bool condition_dontcare(mode_t stat) { (void)stat; return false; }		

int main(int argc, char** argv) {
	struct winsize tty_dimensions = {0};
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dimensions);


	uint32_t operand_count = 0;
	const args_t args = parse_argv(argc, argv, &operand_count);
	#ifndef NDEBUG
	// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3047.pdf, _BitInt is converted to corresponding bit precision, so int is fully representable here //
	printf("args: %b\noperands: %i\n", (int)args, operand_count);
	#endif

	table_t* f_ext_map = NULL;
	if (!(args & ARG_NO_NERDFONTS)) {
		f_ext_map = init_filetype_dict();
		if (!f_ext_map) {
			printf_color(stdout, RED, "Failed to allocate memory for file extension table, for nerdfonts!\n");
			return 1;
		}
	}
	file_t* files = NULL;
	uint8_t longest_fname = 0;
	size_t largest_fsize = 0, f_count = 0;
	if (operand_count == 0) {
		files = query_files(".", &longest_fname, &largest_fsize, &f_count, args, 0);
		if (errno || !files) {
			// TODO: error checking //
			return 1;
		}

		// TODO: this should be abstracted out to afunction but I don't know what to call the function //
		int8_t list_files_retcode = 0;
		size_t longest_fdescriptor = get_longest_fdescriptor(longest_fname, largest_fsize, args);
		if (args & ARG_UNSORTED)
			list_files_retcode = list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_dontcare, args);
		else {
			list_files_retcode =  list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isndir, args);
			list_files_retcode =  list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isdir, args);
		}
		
		free(files);
		putchar('\n');
		return 0;
	}

	uint8_t ret_code = 0;
	for(int i=0; i<argc; ++i) {
		#define OPERAND argv[i]
		if (!operand_count) break;
		else if (OPERAND[0] == '-') continue;
		else if (OPERAND[strlen(OPERAND)-1] == '/' && strlen(OPERAND) != 1) OPERAND[strlen(OPERAND) - 1] = '\0';

		struct stat st;
		if (stat(OPERAND, &st) == -1) {
			printf_color(stderr, YELLOW, "Failed to access %s! (Do you have permission?)\n", OPERAND);
			++ret_code;
			continue;
		} else if (!S_ISDIR(st.st_mode)) {
			// FIXME: why tf does this print? //
			puts("Currently using lsf on files does nothing. It is planned in the future to call to `cat`");
			// TODO: call to cat, bat, or catf.... auctually... //
			// side rant: I don't know if Cu-Fied will implement cat or cd //
			// for cat, bat is already REALLY good, and for cd zoxide works wonders.. //
			// and I don't know what to do for either of those that hasn't already been done well //
			continue;
		}

		if (!(args & ARG_NO_NERDFONTS)) printf_color(stdout, BLUE, " ");
		printf_color(stdout, BLUE, "%s:\n", OPERAND);

		errno = 0;
		longest_fname = 0, largest_fsize = 0, f_count = 0;
		files = query_files(OPERAND, &longest_fname, &largest_fsize, &f_count, args, 0);
		if (errno || !files) {
			switch (errno) {
				case EOVERFLOW:
					printf_color(stderr, RED, "Failed to allocate memory for listing %s! (Multiplication overflow!)\n", OPERAND);
				case ENOMEM:
					printf_color(stderr, RED, "Failed to allocate memory for listing %s! (Out of memory!)\n", OPERAND);
				default:
					printf_color(stderr, RED, "Failed to list %s!\n", OPERAND);
			}
			++ret_code;
			goto failed_to_query_files;
		}

		int8_t list_files_retcode = 0;
		size_t longest_fdescriptor = get_longest_fdescriptor(longest_fname, largest_fsize, args);
		if (args & ARG_UNSORTED)
			list_files_retcode = list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_dontcare, args);
		else {
			list_files_retcode =  list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isndir, args);
			list_files_retcode =  list_files(files, longest_fdescriptor, f_count, tty_dimensions.ws_col / longest_fdescriptor, f_ext_map, condition_isdir, args);
		}

		failed_to_query_files:
	
		--operand_count;
		putchar('\n');

		if (operand_count > 1) putchar('\n');
		free(files);
	} return ret_code;
}
