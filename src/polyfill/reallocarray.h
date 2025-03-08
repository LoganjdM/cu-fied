#ifndef REALLOCARRAY_H
#define REALLOCARRAY_H

#include <stdlib.h>
#include <errno.h>

// Only define reallocarray if it's not already available
#if defined(__APPLE__) && TARGET_OS_MAC==1 || !defined(__GLIBC__)
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

static inline void* reallocarray(void *optr, size_t nmemb, size_t size)
{
    if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
            nmemb > 0 && SIZE_MAX / nmemb < size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(optr, size * nmemb);
}
#endif

#endif // REALLOCARRAY_H
