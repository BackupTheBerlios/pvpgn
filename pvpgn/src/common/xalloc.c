#include "common/setup_before.h"
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#include "common/eventlog.h"
#include "common/xalloc.h"
#define XALLOC_INTERNAL_ACCESS
#include "common/setup_after.h"
#undef XALLOC_INTERNAL_ACCESS

void *xmalloc_real(size_t size, const char *fn, unsigned ln)
{
    void *res;

    res = malloc(size);
    if (!res)
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);

    return res;
}

void *xcalloc_real(size_t nmemb, size_t size, const char *fn, unsigned ln)
{
    void *res;

    res = calloc(nmemb,size);
    if (!res)
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);

    return res;
}

void *xrealloc_real(void *ptr, size_t size, const char *fn, unsigned ln)
{
    void *res;

    res = realloc(ptr,size);
    if (!res)
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);

    return res;
}

void xfree_real(void *ptr, const char *fn, unsigned ln)
{
    if (!ptr) {
	eventlog(eventlog_level_fatal, __FUNCTION__, "got NULL ptr (from %s:%u)",fn,ln);
	return; /* avoid sure crash */
    }

    free(ptr);
}
