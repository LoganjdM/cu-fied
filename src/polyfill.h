// so ik more about c than I did before ig, turns out the reason why apple is so weird is because they use apple_libc, musl has all this shtuff, just not apple libc //
#ifdef __APPLE__

#ifdef REALLOCARRAY_IMPLEMENTATION
#	include <stdlib.h>
#	define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t)*4))
static inline void* reallocarray(void* ptr, size_t nmemb, size_t size) {
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) && nmemb > 0 && SIZE_MAX/nmemb < size) {
		return NULL;
	} return realloc(ptr, size * nmemb);
}
#endif
#ifdef MEMPCPY_IMPLEMENTATION
#	include <string.h>
static inline void* mempcpy(void* dest, const void* src, size_t n) {
	memcpy(dest, src, n);
	return (void*)dest + n;
}
#endif
#ifdef STRDUPA_IMPLEMENTATION
#	include <string.h>
#	include <alloca.h>
char* strdupa(const char* s) {
	char* result = alloca(strlen(s) + 1);
	strncpy(result, s, strlen(s) + 1);
	return result;
}
#endif

#endif // __GLIBC__