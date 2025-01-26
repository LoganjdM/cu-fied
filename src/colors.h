#ifndef _STDIO_H
// kinda dont want to import all of stdio just for this .. //
extern int fprintf (void*__restrict __stream, const char *__restrict __format, ...) __nonnull ((1));
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

int escape_code(FILE* stream, const char ansi[5]) {
	return fprintf(stream, ansi);
}