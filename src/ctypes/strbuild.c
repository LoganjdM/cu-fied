#include <stdlib.h>
#include <string.h>

#define MEMPCPY_IMPLEMENTATION
#include "../polyfill.h"

typedef struct {
	char* str;
	void* last;
	size_t len;
} strbuild_t;

// I'd use my STB style lib of a stringbuilder, but that uses mmap().. and also this works //
size_t sb_append(strbuild_t* sb, const char* appendee) {
	const size_t appendlen = strlen(appendee);

	// TODO?: we should prob use realloc() but realloc was causing issues, so we just create an entirely new string //
	char* newstr = (char*)malloc(sb->len+appendlen+1); // +1 for \0
	if(!newstr) return 0;
	strcpy(newstr, sb->str);
	free(sb->str); sb->str = newstr;

	sb->last = mempcpy(sb->str+sb->len, appendee, appendlen);
	sb->len += appendlen;

	*((char*)sb->last) = '\0';
	return appendlen;
}

strbuild_t sb_new(void) {
	char* sb_str = (char*)malloc(1); // dont give a shit on size, just have the OS give us a memblock //
	return (strbuild_t){ .str = sb_str, .last = (void*)sb_str, .len = 0};
}
