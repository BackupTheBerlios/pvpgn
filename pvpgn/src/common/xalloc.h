#ifndef INCLUDED_XALLOC_TYPES
#define INCLUDED_XALLOC_TYPES

#endif /* INCLUDED_XALLOC_TYPES */

#ifndef INCLUDED_XALLOC_PROTOS
#define INCLUDED_XALLOC_PROTOS

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

#define xmalloc(size) xmalloc_real(size,__FILE__,__LINE__)
void *xmalloc_real(size_t size, const char *fn, unsigned ln);
#define xcalloc(no,size) xcalloc_real(no,size,__FILE__,__LINE__)
void *xcalloc_real(size_t nmemb, size_t size, const char *fn, unsigned ln);
#define xrealloc(ptr,size) xrealloc_real(ptr,size,__FILE__,__LINE__)
void *xrealloc_real(void *ptr, size_t size, const char *fn, unsigned ln);
#define xstrdup(str) xstrdup_real(str,__FILE__,__LINE__)
char *xstrdup_real(const char *str, const char *fn, unsigned ln);
#define xfree(ptr) xfree_real(ptr,__FILE__,__LINE__)
void xfree_real(void *ptr, const char *fn, unsigned ln);

#endif /* INCLUDED_XALLOC_PROTOS */
