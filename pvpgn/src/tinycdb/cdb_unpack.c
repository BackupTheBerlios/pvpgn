/* $Id: cdb_unpack.c,v 1.2 2003/07/30 21:12:31 dizzy Exp $
 * unpack 32bit integer
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "cdb.h"

cdbi_t
cdb_unpack(const unsigned char buf[4])
{
  cdbi_t n = buf[3];
  n <<= 8; n |= buf[2];
  n <<= 8; n |= buf[1];
  n <<= 8; n |= buf[0];
  return n;
}

