/* $Id: cdb_make_put.c,v 1.2 2003/07/30 21:12:31 dizzy Exp $
 * "advanced" cdb_make_put routine
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "common/setup_before.h"
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#include "cdb_int.h"
#include "common/setup_after.h"

int
cdb_make_put(struct cdb_make *cdbmp,
	     const void *key, cdbi_t klen,
	     const void *val, cdbi_t vlen,
	     int flags)
{
  unsigned char rlen[8];
  cdbi_t hval = cdb_hash(key, klen);
  struct cdb_rl *rl;
  int c, r;

  switch(flags) {
    case CDB_PUT_REPLACE:
    case CDB_PUT_INSERT:
    case CDB_PUT_WARN:
      c = _cdb_make_find(cdbmp, key, klen, hval, &rl);
      if (c < 0)
	return -1;
      if (c) {
	if (flags == CDB_PUT_INSERT) {
	  errno = EEXIST;
	  return 1;
	}
	else if (flags == CDB_PUT_REPLACE) {
	  --c;
	  r = 1;
	  break;
	}
	else
	  r = 1;
      }
      /* fall */

    case CDB_PUT_ADD:
      rl = cdbmp->cdb_rec[hval&255];
      if (!rl || rl->cnt >= sizeof(rl->rec)/sizeof(rl->rec[0])) {
 	rl = (struct cdb_rl*)malloc(sizeof(struct cdb_rl));
	if (!rl) {
	  errno = ENOMEM;
	  return -1;
	}
	rl->cnt = 0;
	rl->next = cdbmp->cdb_rec[hval&255];
	cdbmp->cdb_rec[hval&255] = rl;
      }
      c = rl->cnt;
      r = 0;
      break;

    default:
      errno = EINVAL;
      return -1;
  }

  if (klen > 0xffffffff - (cdbmp->cdb_dpos + 8) ||
      vlen > 0xffffffff - (cdbmp->cdb_dpos + klen + 8)) {
    errno = ENOMEM;
    return -1;
  }
  rl->rec[c].hval = hval;
  rl->rec[c].rpos = cdbmp->cdb_dpos;
  if (c == rl->cnt) {
    ++rl->cnt;
    ++cdbmp->cdb_rcnt;
  }
  cdb_pack(klen, rlen);
  cdb_pack(vlen, rlen + 4);
  if (_cdb_make_write(cdbmp, rlen, 8) < 0 ||
      _cdb_make_write(cdbmp, key, klen) < 0 ||
      _cdb_make_write(cdbmp, val, vlen) < 0)
    return -1;
  return r;
}

