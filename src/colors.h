#include <stdio.h>
#include <unistd.h>

#define RED "\x1b[31m"
#define BLUE "\x1b[34m"
#define CYAN "\x1b[36m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define MAGENTA "\x1b[35m"

#define BOLD "\x1b[1m"
#define ITALIC "\x1b[3m"

#define RESET "\x1b[0m"


const char* escape_code(FILE* fp, const char ansi[5]) {
	if(isatty(fileno(fp))) {
		return ansi;
	} else return NULL;
}

int print_escape_code(FILE* fp, const char ansi[5]) {
	const char* bytes = escape_code(fp, ansi);
	if(!bytes) return 0;
	return fputs(bytes, fp);
}