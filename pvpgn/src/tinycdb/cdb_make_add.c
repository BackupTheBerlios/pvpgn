#ifdef WITH_CDB

/* $Id: cdb_make_add.c,v 1.1 2003/07/30 20:04:42 dizzy Exp $
 * basic cdb_make_add routine
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
cdb_make_add(struct cdb_make *cdbmp,
	     const void *key, cdbi_t klen,
	     const void *val, cdbi_t vlen)
{
  unsigned char rlen[8];
  cdbi_t hval;
  struct cdb_rl *rl;
  if (klen > 0xffffffff - (cdbmp->cdb_dpos + 8) ||
      vlen > 0xffffffff - (cdbmp->cdb_dpos + klen + 8)) {
    errno = ENOMEM;
    return -1;
  }
  hval = cdb_hash(key, klen);
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
  rl->rec[rl->cnt].hval = hval;
  rl->rec[rl->cnt].rpos = cdbmp->cdb_dpos;
  ++rl->cnt;
  ++cdbmp->cdb_rcnt;
  cdb_pack(klen, rlen);
  cdb_pack(vlen, rlen + 4);
  if (_cdb_make_write(cdbmp, rlen, 8) < 0 ||
      _cdb_make_write(cdbmp, key, klen) < 0 ||
      _cdb_make_write(cdbmp, val, vlen) < 0)
    return -1;
  return 0;
}

#endif /* WITH_CDB */
