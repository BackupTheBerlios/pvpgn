/* $Id: cdb_make_find.c,v 1.6 2003/09/10 10:48:50 aaron Exp $
 * routines to search in in-progress cdb file
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "common/setup_before.h"
#include <stdio.h> /* vs.net wants this for SEEK_SET, maybe others, too? */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
#ifdef __BORLANDC__
# include <io.h>
#endif
#endif
#include "cdb_int.h"
#include "common/setup_after.h"

static int
match(int fd, cdbi_t pos, const char *key, cdbi_t klen)
{
  unsigned char buf[64]; /*XXX cdb_buf may be used here instead */
  if (lseek(fd, pos, SEEK_SET) < 0 || read(fd, buf, 8) != 8)
    return -1;
  if (cdb_unpack(buf) != klen)
    return 0;

  while(klen > sizeof(buf)) {
    if (read(fd, buf, sizeof(buf)) != sizeof(buf))
      return -1;
    if (memcmp(buf, key, sizeof(buf)) != 0)
      return 0;
    key += sizeof(buf);
    klen -= sizeof(buf);
  }
  if (klen) {
    if (read(fd, buf, klen) != klen)
      return -1;
    if (memcmp(buf, key, klen) != 0)
      return 0;
  }
  return 1;
}

int
_cdb_make_find(struct cdb_make *cdbmp,
	       const void *key, cdbi_t klen, cdbi_t hval,
	       struct cdb_rl **rlp)
{
  struct cdb_rl *rl = cdbmp->cdb_rec[hval&255];
  int r, i;
  int seeked = 0;
  while(rl) {
    for(i = rl->cnt - 1; i >= 0; --i) { /* search backward */
      if (rl->rec[i].hval != hval)
	continue;
      /*XXX this explicit flush may be unnecessary having
       * smarter match() that looks to cdb_buf too, but
       * most of a time here spent in finding hash values
       * (above), not keys */
      if (cdbmp->cdb_bpos != cdbmp->cdb_buf) {
        if (write(cdbmp->cdb_fd, cdbmp->cdb_buf,
	          cdbmp->cdb_bpos - cdbmp->cdb_buf) < 0)
          return -1;
        cdbmp->cdb_bpos = cdbmp->cdb_buf;
      }
      seeked = 1;
      r = match(cdbmp->cdb_fd, rl->rec[i].rpos, key, klen);
      if (!r)
	continue;
      if (r < 0)
	return -1;
      if (lseek(cdbmp->cdb_fd, cdbmp->cdb_dpos, SEEK_SET) < 0)
        return -1;
      if (rlp)
	*rlp = rl;
      return i + 1;
    }
    rl = rl->next;
  }
  if (seeked && lseek(cdbmp->cdb_fd, cdbmp->cdb_dpos, SEEK_SET) < 0)
    return -1;
  return 0;
}

int
cdb_make_exists(struct cdb_make *cdbmp,
                const void *key, cdbi_t klen)
{
  return _cdb_make_find(cdbmp, key, klen, cdb_hash(key, klen), NULL);
}

