#ifndef _STDLIB_
#	include <stddef.h>
#endif

typedef struct {
	char* str;
	void* last;
	size_t len;
} strbuild_t;

// returns bytes moved //
size_t sb_append(strbuild_t* sb, const char* appendee);
strbuild_t sb_new(void);