#ifndef MEMPCPY_H
#define MEMPCPY_H

#if !defined(HAVE_MEMPCPY)

#include <string.h> // memcpy

static inline void *
mempcpy(void *dest, const void *src, size_t n) {
    memcpy(dest, src, n);
    return (char*)dest + n;
}

#endif /* !defined(HAVE_MEMPCPY)) */

#endif /* MEMPCPY_H */
