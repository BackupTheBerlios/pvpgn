#ifdef WITH_CDB

/* $Id: cdb_hash.c,v 1.1 2003/07/30 20:04:42 dizzy Exp $
 * cdb hashing routine
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "cdb.h"

cdbi_t
cdb_hash(const void *buf, cdbi_t len)
{
  register const unsigned char *p = (const unsigned char *)buf;
  register const unsigned char *end = p + len;
  register cdbi_t hash = 5381;	/* start value */
  while (p < end)
    hash = (hash + (hash << 5)) ^ *p++;
  return hash;
}

#endif /* WITH_CDB */
