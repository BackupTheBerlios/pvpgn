/*
 * Copyright (C) 2005  Olaf Freyer (aaron@cs.tu-berlin.de)
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
#ifndef HAVE_STRNLEN

#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#include "strnlen.h"
#include "common/setup_after.h"

extern unsigned long strnlen(const char *s, unsigned long maxlen)
{
    unsigned long length = 0;
    const char * symbol=s;

    if (!s)
      return 0;

    while ((length<maxlen) && (*symbol!='\0'))
    {
	    length ++;
	    symbol++;
    }
    return length;

}

#else
typedef int filenotempty; /* make ISO standard happy */
#endif
