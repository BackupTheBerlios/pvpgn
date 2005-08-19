/*
 * Copyright (C) 2005  Dizzy 
 *
 * lstr is a structure to be used for cached string lengths for speed
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "common/setup_before.h"
#include "common/xstr.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <assert.h>
#include "common/xalloc.h"
#include "common/setup_after.h"

/* how many bytes allocate at once on enlarging a xstr */
#define XSTR_INCREMENT	256

/* enlarge "dst" enough so it can hold more "size" characters (not including terminator) */
static void xstr_enlarge(t_xstr* dst, int size)
{
	if (dst->alen - dst->ulen - 1 < size) {
		int nalen = ((dst->ulen + size + 10) / XSTR_INCREMENT) * XSTR_INCREMENT;

		dst->str = xrealloc(dst->str, nalen);
		dst->alen = nalen;
	}
}

extern t_xstr* xstr_alloc(void)
{
	t_xstr* xstr = (t_xstr*)xmalloc(sizeof(t_xstr));

	xstr_init(xstr);

	return xstr;
}

extern void xstr_free(t_xstr* xstr)
{
	assert(xstr);

	if (xstr->str) xfree(xstr->str);
	xfree(xstr);
}

extern t_xstr* xstr_cat_xstr(t_xstr* dst, const t_xstr* src)
{
	assert(dst);

	if (!src || !src->ulen) return dst;

	/* need to enlarge dst ? */
	xstr_enlarge(dst, src->ulen);

	memcpy(dst->str + dst->ulen, src->str, src->ulen + 1);
	dst->ulen += src->ulen;

	return dst;
}

extern t_xstr* xstr_cat_str(t_xstr* dst, const char* src)
{
	int len;

	assert(dst);

	if (!src) return dst;

	len = strlen(src);

	/* need to enlarge dst ? */
	xstr_enlarge(dst, len);

	memcpy(dst->str + dst->ulen, src, len + 1);
	dst->ulen += len;

	return dst;
}

extern t_xstr* xstr_ncat_str(t_xstr * dst, const char * src, int len)
{
	const char *p;

	assert(dst);

	if (!src || len < 1 || !src[0]) return dst;

	for(p = src; *p && p - src < len; ++p)
		xstr_cat_char(dst, *p);

	return dst;
}

extern t_xstr* xstr_cat_char(t_xstr * dst, const char ch)
{
	assert(dst);

	xstr_enlarge(dst, 1);
	dst->str[dst->ulen++] = ch;
	dst->str[dst->ulen] = '\0';

	return dst;
}