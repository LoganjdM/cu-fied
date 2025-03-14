#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#ifndef C23
#	include <stdbool.h>
#endif

#define RED "\x1b[31m"
#define BLUE "\x1b[34m"
#define CYAN "\x1b[36m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define MAGENTA "\x1b[35m"

#define BOLD "\x1b[1m"
#define ITALIC "\x1b[3m"

#define RESET "\x1b[0m"

bool force_color = false;

const char* get_escape_code(int fd, const char ansi[5]) {
	if(isatty(fd) || force_color) {
		return ansi;
	} else return "\0";
}

int print_escape_code(FILE* fp, const char ansi[5]) {
	const char* bytes = get_escape_code(fileno(fp), ansi);
	if(!bytes) return 0;
	return fputs(bytes, fp);
}

int printf_color(FILE* fp, const char ansi[5], char* fmt, ...) {
	fputs(get_escape_code(fileno(fp), ansi), fp);

	int printed = 0;
	va_list args;
	va_start(args, fmt);
	printed += vfprintf(fp, fmt, args);
	va_end(args);
	fputs(get_escape_code(fileno(fp), RESET), fp);
	return printed;
}