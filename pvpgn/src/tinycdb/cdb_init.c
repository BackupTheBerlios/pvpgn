/* $Id: cdb_init.c,v 1.2 2003/07/30 21:12:31 dizzy Exp $
 * cdb_init, cdb_free and cdb_read routines
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "common/setup_before.h"
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include "cdb_int.h"
#include "common/setup_after.h"

int
cdb_init(struct cdb *cdbp, int fd)
{
  struct stat st;
#ifdef HAVE_MMAP
  unsigned char *mem = NULL;
#endif

  /* get file size */
  if (fstat(fd, &st) < 0)
    return -1;
  /* trivial sanity check: at least toc should be here */
  if (st.st_size < 2048) {
    errno = EPROTO;
    return -1;
  }
#ifdef HAVE_MMAP
  /* memory-map file */
  if ((mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) ==
      (unsigned char *)-1)
    return -1;

  cdbp->cdb_mem = mem;
#endif
  cdbp->cdb_fd = fd;
  cdbp->cdb_fsize = st.st_size;

#if 0
  /* XXX don't know well about madvise syscall -- is it legal
     to set different options for parts of one mmap() region?
     There is also posix_madvise() exist, with POSIX_MADV_RANDOM etc...
  */
#ifdef MADV_RANDOM
  /* set madvise() parameters. Ignore errors for now if system
     doesn't support it */
  madvise(mem, 2048, MADV_WILLNEED);
  madvise(mem + 2048, cdbp->cdb_fsize - 2048, MADV_RANDOM);
#endif
#endif

  cdbp->cdb_vpos = cdbp->cdb_vlen = 0;

  return 0;
}

void
cdb_free(struct cdb *cdbp)
{
#ifdef HAVE_MMAP
  if (cdbp->cdb_mem) {
    munmap((void*)cdbp->cdb_mem, cdbp->cdb_fsize);
    cdbp->cdb_mem = NULL;
  }
#endif
  cdbp->cdb_fsize = 0;
}

#ifdef HAVE_MMAP
int
cdb_read(const struct cdb *cdbp, void *buf, unsigned len, cdbi_t pos)
{
  if (pos > cdbp->cdb_fsize || cdbp->cdb_fsize - pos < len) {
    errno = EPROTO;
    return -1;
  }
  memcpy(buf, cdbp->cdb_mem + pos, len);
  return 0;
}
#endif /* HAVE_MMAP */
