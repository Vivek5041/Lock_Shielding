#ifndef __PADDING_H__
#define __PADDING_H__

#include <topology.h>

#define r_align(n, r) (((n) + (r)-1) & -(r))
#define cache_align(n) r_align(n, L_CACHE_LINE_SIZE)
#define pad_to_cache_line(n) (cache_align(n) - (n))
#define MEMALIGN(ptr, alignment, size)                                         \
    posix_memalign((void **)(ptr), (alignment), (size))

#endif // __PADDING_H__
