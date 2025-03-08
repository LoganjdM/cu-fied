#ifndef MEMPCPY_H
#define MEMPCPY_H

#include <string.h>

// Only define mempcpy if it's not already available
#if defined(__APPLE__) && TARGET_OS_MAC==1 || !defined(__GLIBC__)
static inline void* mempcpy(void *dest, const void *src, size_t n) {
    memcpy(dest, src, n);
    return (char*)dest + n;
}
#endif

#endif // MEMPCPY_H
