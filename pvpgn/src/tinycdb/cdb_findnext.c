/* $Id: cdb_findnext.c,v 1.2 2003/07/30 21:12:31 dizzy Exp $
 * sequential cdb_find routines
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

/* see cdb_find.c for comments */

#include "common/setup_before.h"
#include "cdb_int.h"
#include "common/setup_after.h"

#ifdef HAVE_MMAP
int
cdb_findinit(struct cdb_find *cdbfp, struct cdb *cdbp,
             const void *key, cdbi_t klen)
{
  cdbi_t n, pos;

  cdbfp->cdb_cdbp = cdbp;
  cdbfp->cdb_key = key;
  cdbfp->cdb_klen = klen;
  cdbfp->cdb_hval = cdb_hash(key, klen);

  cdbfp->cdb_htp = cdbp->cdb_mem + ((cdbfp->cdb_hval << 3) & 2047);
  n = cdb_unpack(cdbfp->cdb_htp + 4);
  cdbfp->cdb_httodo = n << 3;
  if (!n)
    return 0;
  pos = cdb_unpack(cdbfp->cdb_htp);
  if (n > (cdbp->cdb_fsize >> 3)
      || pos > cdbp->cdb_fsize
      || cdbfp->cdb_httodo > cdbp->cdb_fsize - pos)
  {
    errno = EPROTO;
    return -1;
  }

  cdbfp->cdb_htab = cdbp->cdb_mem + pos;
  cdbfp->cdb_htend = cdbfp->cdb_htab + cdbfp->cdb_httodo;
  cdbfp->cdb_htp = cdbfp->cdb_htab + (((cdbfp->cdb_hval >> 8) % n) << 3);

  return 0;
}

int cdb_findnext(struct cdb_find *cdbfp) {
  cdbi_t pos, n;
  struct cdb *cdbp = cdbfp->cdb_cdbp;

  while(cdbfp->cdb_httodo) {
    pos = cdb_unpack(cdbfp->cdb_htp + 4);
    if (!pos)
      return 0;
    n = cdb_unpack(cdbfp->cdb_htp) == cdbfp->cdb_hval;
    if ((cdbfp->cdb_htp += 8) >= cdbfp->cdb_htend)
      cdbfp->cdb_htp = cdbfp->cdb_htab;
    cdbfp->cdb_httodo -= 8;
    if (n) {
      if (pos > cdbp->cdb_fsize - 8) {
	errno = EPROTO;
	return -1;
      }
      if (cdb_unpack(cdbp->cdb_mem + pos) == cdbfp->cdb_klen) {
	if (cdbp->cdb_fsize - cdbfp->cdb_klen < pos + 8) {
	  errno = EPROTO;
	  return -1;
	}
	if (memcmp(cdbfp->cdb_key,
	    cdbp->cdb_mem + pos + 8, cdbfp->cdb_klen) == 0) {
	  n = cdb_unpack(cdbp->cdb_mem + pos + 4);
	  pos += 8 + cdbfp->cdb_klen;
	  if (cdbp->cdb_fsize < n || cdbp->cdb_fsize - n < pos) {
	    errno = EPROTO;
	    return -1;
	  }
	  cdbp->cdb_vpos = pos;
	  cdbp->cdb_vlen = n;
	  return 1;
	}
      }
    }
  }

  return 0;

}
#endif /* HAVE_MMAP */
