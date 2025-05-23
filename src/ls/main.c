#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include "../libcolor/colors.h"
#include "../app_info.h"
#include "../f_ext_map.c"
#define REALLOCARRAY_IMPLEMENTATION
#define STRDUPA_IMPLEMENTATION
#	include "../polyfill.h"

#include "common.h"

bool parse_argv(const int argc, const char** argv, struct args* arg_buf) {
	assert(arg_buf);
	bool ret = true;

	arg_buf->operandc = 0;
	for (int i=1; i<argc; ++i) {
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
			char* arg_tok = strdupa(ARG);
			// lol spell checker doesn't like german //
			// gotta deal with numbers und sheisse, da is verruckt, ich werde mich TOTEN //
			if ((arg_tok = strtok(arg_tok, "="))) {
				if (IS_ARG(arg_tok, "--recurse", "-R")) {
					arg_tok = strtok(NULL, "=");
					unsigned int recurse_parse = (unsigned)atoi(arg_tok);
					// don't overflow and assume max recursion if parse failed or input was 0 //
					if (recurse_parse == 0 || recurse_parse > 0xFFFF)
						arg_buf->args |= (0xFFFF << 8);
					else arg_buf->args |= (recurse_parse << 8);
				} else if (IS_ARG(arg_tok, "--human-readable", "-hr")) {
					arg_tok = strtok(NULL, "=");
					unsigned int hr_val = (unsigned)atoi(arg_tok);
					if (hr_val <= 3) arg_buf->args |= (hr_val << 6);
				} continue;
			}

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
	} if (arg_buf->operandc == 0) {
		arg_buf->operandc = 1;
		arg_buf->operandv[0] = strdup(".");
		if (!arg_buf->operandv[0]) return false;
	}

	return ret;
}

static inline void free_operands(struct args args) {
	for (uint16_t i=0; i<args.operandc; ++i)
		free(args.operandv[i]);
}

// this should be a macro :/ //
void iterate_over_open_err() {
	switch (errno) {
		case ELOOP:
			fprintf_color(stderr, YELLOW, "(Encountered too many symbolic links!)!");
			break;
		case ENOENT:
			fprintf_color(stderr, YELLOW, "(does it exist?)!");
			break;
		// we don't have access //
		case EACCES:
			fprintf_color(stderr, YELLOW, "(do you have access to it?)!");
			break;
		case EPERM:
			fprintf_color(stderr, YELLOW, "(do you have access to it?)!");
			break;
		case EROFS:
			fprintf_color(stderr, YELLOW, "(do you have access to it?)!");
			break;
		// its not valid //
		case ENAMETOOLONG:
			fprintf_color(stderr, YELLOW, "(is it a valid file?)!");
			break;
		case EINVAL:
			fprintf_color(stderr, YELLOW, "(is it a valid file?)!");
			break;
		case ENXIO:
			fprintf_color(stderr, YELLOW, "(is it a valid file?)!");
			break;
		default:
			fprintf_color(stderr, YELLOW, "(uh oh, errno: %d)!", errno);
	}
}

void close_range_binding(int start, int end, int flags) {
	#ifdef __APPLE__
	for (uint8_t i=start; i<end;  ++i)
		close(i);
	errno = 0; // close_range ignores EBADF's
	// #elif defined(__FreeBSD__) // we thinkin about it
	// close_range(start, end, flags);
	#else // Linux //
	// what the fuck zig CC? outdated glibc forcing me to make my own binding //
	#	ifdef __x86_64__
	__asm__ volatile(
		"movl $436, %%eax\n" // close_range //
		"movl %[start], %%edi\n"
		"movl %[end], %%esi\n"
		"movl $0, %%edx\n"
		"syscall\n"
		: // no out //
		: [start] "r"(start),
		  [end] "r"(end)
		: "%eax", "%edi", "%esi", "%edx"
	);
	#	else // ARM //
	__asm__ volatile(
		"mov x8, #436\n" // close_range //
		"mov x0, %[start]\n"
		"mov x1, %[end]\n"
		"mov x2, #0\n"
		"svc #0\n"
		:
		: [start] "r"(start),
	      [end]   "r"(end)
		: "x8", "x0", "x1", "x2"
	);
	#	endif
	#endif
}

bool query_files(char* path, const int fd,
				file_t** da_files, size_t* file_len, size_t file_cap,
				unsigned int da_fd[static 100], size_t* fd_len,
				struct args* args) {
	assert(fd != -1);
	assert(da_files);

	DIR* dfp = fdopendir(fd);
	if (!dfp) return false;

	bool had_dirs = false;
	struct dirent* d_stream = NULL;
	for (; (d_stream = readdir(dfp)); ++*file_len) {
		#define FILE (*da_files)[*file_len - 1]
		if (*file_len > file_cap) {
			if (file_cap > file_cap << 1) {
				errno = EOVERFLOW;
				return false;
			} file_cap <<= 1;

			void* np = reallocarray(*da_files, file_cap, sizeof(file_t));
			if (!np) return false;
			*da_files = (file_t*)np;
		} if (*fd_len == 100) {
			close_range_binding(da_fd[0], da_fd[99], 0);
			*fd_len = 0;
		}

		FILE.name = strdup(d_stream->d_name);
		if (!FILE.name) return false;

		char* fullpath = malloc(strlen(path)+strlen(FILE.name)+2);
		if (!fullpath) return false;
		sprintf(fullpath, "%s/%s", path, FILE.name);

		int fd = open(fullpath, 0);
		if (fd == -1) {
			fprintf_color(stderr, YELLOW, "Ran into an error listing files! ");
			iterate_over_open_err();
			free(fullpath);
			continue;
		}

		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			FILE.ok_st = false;
			assert(errno != EBADF);
			// will only happen if LSF is compiled in 32 bit and tries to ls a 64 bit file //
			if (errno == EOVERFLOW) {
				fprintf_color(stderr, BLUE, "%s ", FILE.name);
				fprintf_color(stderr, YELLOW, "has an inode, block count, or size that is too big to represent!\n ", FILE.name);
			} continue;
		}
		FILE.ok_st = true;
		FILE.stat = st;

		if (S_ISDIR(st.st_mode) && (ARG_RECURSE(args->args) > 0)) {
			args->operandv[args->operandc] = fullpath; // re-use instead of re-allocating.
			++args->operandc;
			had_dirs = true;
		} else free(fullpath);
		#undef FILE
	}

	closedir(dfp);
	if (had_dirs) {
		uint8_t recurse = ARG_RECURSE(args->arg);
		--recurse;
		args->arg |= recurse << 8;
	}
	return true;
}

bool condition_isdir(mode_t stat) { return S_ISDIR(stat); }
bool condition_isndir(mode_t stat) { return !S_ISDIR(stat); }
bool condition_dontcare(mode_t stat) { (void)stat; return false; }

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
	#ifndef NDEBUG
	printf(
		"args: %b\n"
		"operands %d\n",
		args.args, args.operandc
	);
	#endif

	table_t* f_ext_map = NULL;
	if (!(args.args & no_nerdfont)) {
		f_ext_map = init_filetype_dict();
		if (!f_ext_map) {
			if (errno == ENOMEM) {
				fprintf_color(stderr, RED, "Didn't have enough memory for file extension table for nerdfonts!\n");
				return 1;
			}
			fprintf_color(stderr, RED, "Encountered error making file extension table for nerdfonts!\n");
			return 254;
		}
	}

	// main loop //
	uint8_t retcode = 0;
	unsigned int* da_fd = (unsigned*)calloc(100, sizeof(da_fd));
	size_t fd_len = 1;
	for (uint16_t i=0; i<args.operandc; ++i) {
		#define OPERAND args.operandv[i]

		int fd = open(OPERAND, 0);
		if (fd == -1) {
			// shouldn't happen, somethings gone wrong if it has //
			assert(errno != EMFILE);

			fprintf_color(stderr, YELLOW, "Could not list ");
			fprintf_color(stderr, BLUE, "%s ", OPERAND);
			if (errno == ENFILE) {
				fprintf_color(stderr, RED, "(hit the system-wide limit of open file descriptors!)");
				fprintf_color(stderr, YELLOW, "!");
				retcode = 253;
				goto really_bad;
			} else if (errno == ENOMEM) {
				fprintf_color(stderr, RED, "(kernel is out of memory!)");
				fprintf_color(stderr, YELLOW, "!");
				retcode = 252;
				goto really_bad;
			}
			iterate_over_open_err();
			retcode += 2;
			continue;
		}

		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			assert(errno != EBADF);

			fprintf_color(stderr, YELLOW, "Coudln't get stat on ");
			fprintf_color(stderr, BLUE, "%s ", OPERAND);
			switch (errno) {
				case EACCES:
					fprintf_color(stderr, YELLOW, "(do you have access to it?)");
					break;
				case ENOMEM:
					fprintf_color(stderr, RED, "(kernel is out of memory!)");
					goto really_bad;
				case EOVERFLOW:
					fprintf_color(stderr, YELLOW, "(File's size, inode, or block count, can't be represented on this system!)");
					break;
				default:
					fprintf_color(stderr, YELLOW, "(uh oh, errno: %d!)", errno);
					break;
			}
			fprintf_color(stderr, YELLOW, "! ");
			retcode += 2;
			continue;
		}

		if (i > 1 || strcmp(args.operandv[i], ".")) {
			char* operand_copy = strdupa(OPERAND);

			char* f_ext = "\0";
			char* tok = strtok(operand_copy, ".");
			while (tok) {
				f_ext = tok;
				tok = strtok(NULL, ".");
			}

			if (!S_ISDIR(st.st_mode)) {
				// avoid RCE //
				for (size_t j=0; j<strlen(OPERAND); ++j) {
					if (OPERAND[j] == ';' || OPERAND[j] == '|' || OPERAND[j] == '&') {
						fprintf_color(stderr, YELLOW, "Not showing file contents for security! Could result in remote code execution!\n");
						goto potential_rce;
					}

					// TODO: go off PAGE environment variable //
					if (execl("/bin/env", "env", "bat", "-p", OPERAND, NULL) == -1) {
						errno = 0;
						if (execl("/bin/env", "env", "more", "-f", OPERAND, NULL) == -1) {
							fprintf_color(stderr, YELLOW, "Failed to list ");
							fprintf_color(stderr, BLUE, "%s", OPERAND);
							switch (errno) {
								case ENOENT:
									fprintf_color(stderr, YELLOW, "(neither bat or more were found!)");
									break;
								case ENOEXEC:
									fprintf_color(stderr, YELLOW, "(either more or bat and more are not in a recognized format!)");
									break;
								default:
									fprintf_color(stderr, YELLOW, "(uh oh, errno: %d!)", errno);
									break;
							} fprintf_color(stderr, YELLOW, "!");
						}
					}
				} continue;
			} potential_rce:

			const char* nerdicon = f_ext_map->get(f_ext_map, f_ext).s;
			const char* nerdicon_nfound = S_ISDIR(st.st_mode) ? "" : "";
			printf_color(S_ISDIR(st.st_mode) ? BLUE : RESET, "%s %s:\n", nerdicon ? nerdicon : nerdicon_nfound, OPERAND);
		}
		file_t* da_files = (file_t*)calloc(16, sizeof(file_t));
		size_t file_len = 1;
		if (!query_files(OPERAND, fd, &da_files, &file_len, 16, da_fd, &fd_len, &args)) {
			switch (errno) {
				case EOVERFLOW:
					fprintf_color(stderr, RED, "Failed to allocate memory for files! (File capacity overflowed!)\n");
					retcode += 2;
					continue;
				case ENOMEM:
					fprintf_color(stderr, RED, "Failed to allocate memory for files! (Ran out of memory!)\n");
					retcode += 2;
					continue;
				case ELOOP:
					fprintf_color(stderr, YELLOW, "Encountered a symnolic link loop in a file!\n");
					break;
				case EACCES:
					fprintf_color(stderr, YELLOW, "Wasn't allowed to get stat on a file!\n");
					break;
				case EPERM:
					fprintf_color(stderr, YELLOW, "Didn't have permission to get stat on a file!\n");
					break;
				default:
					fprintf_color(stderr, YELLOW, "Encountered an error! (%s : %d)!\n", strerror(errno), errno);
					break;
			}
		}

		struct winsize tty_size = {0};
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_size);
		const size_t longest_f_string = get_longest_f_string(da_files, file_len, args);
		#ifndef NDEBUG
		printf("longest file string: %d\n", longest_f_string);
		#endif
		const size_t f_per_row = tty_size.ws_col / longest_f_string;

		bool list_files_failed = false;
		if (ARG_SORT(args.args) == 1)
			list_files_failed = list_files(da_files, file_len, longest_f_string, f_ext_map, condition_dontcare, f_per_row, args);
        else if (ARG_SORT(args.args) == 2) {
        	list_files_failed = list_files(da_files, file_len, longest_f_string, f_ext_map, condition_isdir, f_per_row, args);
			list_files_failed |= list_files(da_files, file_len, longest_f_string, f_ext_map, condition_isndir, f_per_row, args);
        } else {
			list_files_failed = list_files(da_files, file_len, longest_f_string, f_ext_map, condition_isndir, f_per_row, args);
			list_files_failed |= list_files(da_files, file_len, longest_f_string, f_ext_map, condition_isdir, f_per_row, args);
        }

		if (list_files_failed) {
			fprintf_color(stderr, RED, "Failed to allocate memory for showing file size!\n");
			retcode += 2;
		}

		for (size_t i=0; i<file_len-1; ++i) free(da_files[i].name);
		free(da_files);

		#undef OPERAND
	}

	really_bad:
	if (da_fd) {
		close_range_binding(da_fd[0], da_fd[fd_len - 1], 0);
		free(da_fd);
	}
	free_operands(args);
	if (f_ext_map) ht_free(f_ext_map);
	return retcode;
}
