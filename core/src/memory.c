#include "nanofilter.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_LINE_SIZE 64

void *nf_aligned_alloc(size_t size) {
    if (size == 0) size = 1; /* posix_memalign with size=0 is implementation-defined */
    void *ptr = NULL;
#if defined(_WIN32)
    ptr = _aligned_malloc(size, CACHE_LINE_SIZE);
#else
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0) {
        return NULL;
    }
#endif
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void nf_aligned_free(void *ptr) {
    if (!ptr) return;
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
