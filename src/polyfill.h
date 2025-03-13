#ifndef __GLIBC__

#ifdef REALLOCARRAY_IMPLEMENTATION
#	include <stdlib.h>
#	define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t)*4))
static inline void* reallocarray(void* ptr, size_t nmemb, size_t size) {
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) && nmemb > 0 && SIZE_MAX/nmemb < size) {
		return NULL;
	} return realloc(ptr, size * nmemb);
}
#elif defined(MEMPCPY_IMPLEMENTATION)
#	include <string.h>
static inline void* mempcpy(void* dest, const void* src, size_t n) {
	memcpy(dest, src, n);
	return (void*)dest + n;
}
#endif

#endif // __GLIBC__