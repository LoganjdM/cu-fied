#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../colors.h"
#include "../app_info.h"

#ifndef C23
#	include <stdbool.h>
#endif

#include <assert.h>

// note my original comment here about this binary math being a big of expertimentation on trying a new way of storing args. this is more of me fucking around and figuring out a new way of doing things I like //
// it personally is quite nice imo, though because of that and knowing me: i would not be suprised if I just reinvented something that's been done a million times before without me knowing //

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

// reading too much gnu stuff and writing in zig got me in a "lets try these things I havn't done much" phase //
bool parse_argv(const int argc, const char** argv, struct args arg_buf[static 1]) {
	assert(arg_buf);
	bool ret = true;

	if (argc == 1) {
		arg_buf->operandc = 1;
		arg_buf->operandv[0] = strdup(".");
		if (!arg_buf->operandv[0]) return false;
	} for (int i=1; i<argc; ++i) {
		#define ARG argv[i]

		if (ARG[0] != '-') {
			arg_buf->operandv[arg_buf->operandc] = strdup(ARG);
			if(!arg_buf->operandv[arg_buf->operandc]) return false;
			++arg_buf->operandc;
			continue;
		}

		// simple //
		if (!strcmp(ARG, "--all")) {
			arg_buf->args |= dot_dirs | dot_files; continue;
		} else if (!strcmp(ARG, "--almost-all")) {
			arg_buf->args |= dot_files; continue;
		} else if (!strcmp(ARG, "--dot-dirs")) {
			arg_buf->args |= dot_dirs; continue;
		} else if (!strcmp(ARG, "--no-nerdfonts")) {
			arg_buf->args |= no_nerdfont; continue;
		} else if (!strcmp(ARG, "--dir_contents")) {
			arg_buf->args |= dir_contents; continue;
		} else if (!strcmp(ARG, "--force-color")) {
			force_color = true; continue;
		} else if (!strcmp(ARG, "--unsorted")) {
			arg_buf->args |= (0b1 << 2); continue;
		} else if (!strcmp(ARG, "--directories-first")) {
			arg_buf->args |= (0b10 << 2); continue;
		} else if (!strcmp(ARG, "--stat")) {
			arg_buf->args |= include_stat; continue;
		} 
		// more logic needed //
		#define IS_ARG(arg, l, s) (!strcmp(arg, l) || !strcmp(arg, s))
		else if (IS_ARG(ARG, "--recurse", "-R")) {
			if (i == (argc - 1)) {
				arg_buf->args |= (0xFF << 8);
				continue;
			} ++i;
			unsigned int recurse_parse = (unsigned)atoi(ARG);
			// don't overflow and assume max recursion if parse failed or input was 0 //
			if (recurse_parse == 0 || recurse_parse > 0xFFFF)
				arg_buf->args |= (0xFFFF << 8);
			else arg_buf->args |= (recurse_parse << 8);
			
			continue;
		} else if (IS_ARG(ARG, "--human-readable", "-hr")) {
			if (i == (argc -1)) {
				fprintf_color(stderr, YELLOW, "\"%s\" is missing its specification! (assumed none, check --help?).\n", ARG);
				continue;
			} ++i;

			if (IS_ARG(ARG, "SI", "si"))
				arg_buf->args |= (0b11 << 6);
			else if (IS_ARG(ARG, "bin", "BIN") || IS_ARG(ARG, "binary", "BINARY"))
				arg_buf->args |= (0b10 << 6);
			else if (IS_ARG(ARG, "blocks", "BLOCKS"))
				arg_buf->args |= (0b01 << 6);
			else {
				unsigned int hr_val = (unsigned)atoi(ARG);
				if (hr_val <= 3) arg_buf->args |= (hr_val << 6);
			}
		} else if (IS_ARG(ARG, "--help", "-h")) {
			#ifdef __has_embed
			const char synopsis[] = {
			#	embed "help.txt"
			, '\0' };
			#else
			const char synopsis[] = "LSF was not compiled with the C23 standard; therefore, could not use \"#embed\"!";
			#endif
			puts(synopsis);
			ret = false;
		} else if (IS_ARG(ARG, "--version", "-v")) {
			puts(__CU_FIED_VERSION__);
			ret = false;
		} else {
			char* arg_tok = strdup(ARG);
			if (!arg_tok) return false;
			// gotta deal with numbers und sheisse, da ist verruckt, ich werde mich TOTEN //
			if (strtok(arg_tok, "=")) {
				arg_tok = strtok(NULL, "=");
				if (IS_ARG(arg_tok, "--recurse", "-R")) {
					unsigned int recurse_parse = (unsigned)atoi(arg_tok);
					// don't overflow and assume max recursion if parse failed or input was 0 //
					if (recurse_parse == 0 || recurse_parse > 0xFFFF)
						arg_buf->args |= (0xFFFF << 8);
					else arg_buf->args |= (recurse_parse << 8);	
				} else if (IS_ARG(arg_tok, "--human-readable", "-hr")) {
					unsigned int hr_val = (unsigned)atoi(arg_tok);
					if (hr_val <= 3) arg_buf->args |= (hr_val << 6);
				} continue;
			} free(arg_tok);

			// lsf -fac -U etc... btw pronounce those args out loud :) //
			size_t arg_len = strlen(ARG);
			for (size_t j=1; j<arg_len; ++j) {
				switch (ARG[j]) {
					case 'a':
						arg_buf->args |= dot_dirs | dot_files; continue;
					case 'A':
						arg_buf->args |= dot_files; continue;
					case 'd':
						arg_buf->args |= dot_dirs; continue;
					case 'f':
						arg_buf->args |= no_nerdfont; continue;
					case 'c':
						arg_buf->args |= dir_contents; continue;
					case 'U':
						arg_buf->args |= (0b1 << 2); continue;
					case 's':
						arg_buf->args |= include_stat; continue;
					case 'R':
						arg_buf->args |= (0xFFFF << 8); continue;
					default: break;
				} fprintf_color(stderr, YELLOW, "\"%s\" is not a valid argument!\n", ARG);
				break;
			}
		}
		
		#undef ARG
	}
	
	return ret;
}

void free_operands(struct args args) {
	for (uint16_t i=0; i<args.operandc; ++i)
		free(args.operandv[i]);
}

int main(int argc, char** argv) {
	struct args args = {0};
	if (!parse_argv(argc, (const char**)argv, &args)) {
		if (errno == ENOMEM) {
			fprintf_color(stderr, RED, "Didn't have enough memory to parse over arguments and operands\n");
			return 1;
		} else {
			fprintf_color(stderr, RED, "Encountered error parsing over arguments and operands\n");
			return 255;
		}
	}
	
	free_operands(args);
	return 0;
}