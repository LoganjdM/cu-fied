#include <stdio.h>
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

extern bool force_color;

extern const char* get_escape_code(int fd, const char ansi[static 5]);

extern int fprint_escape_code(FILE* fp, const char ansi[static 5]);

extern int fprintf_color(FILE* fp, const char ansi[static 5], char* fmt, ...);

// nasty non-capital macro stemming from laziness //
#define printf_color(ansi, fmt, ...) fprintf_color(stdout, ansi, fmt, __VA_ARGS__)
extern int print_escape_code(const char ansi[static 5]);