/*
 * Copyright (C) 2004 Dizzy 
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
#include <stdio.h>
#ifdef HAVE_STDARG_H
# include <stdarg.h>
#else
# ifdef HAVE_VARARGS_H
#  include <varargs.h>
# endif
#endif
#include "common/setup_after.h"

#if !defined(HAVE_VSNPRINTF) && !defined(HAVE__VSNPRINTF) && defined(HAVE_DOPRNT) && defined(_IOWRT) && defined(_IOSTRG)

extern int vsnprintf(char *str, int size, const char *format, va_list ap)
{
    FILE b;
    int ret;
#ifdef VMS
    b->_flag = _IOWRT|_IOSTRG;
    b->_ptr = str;
    b->_cnt = size;
#else
    b._flag = _IOWRT|_IOSTRG;
    b._ptr = str;
    b._cnt = size;
#endif
    ret = _doprnt(format, ap, &b);
    putc('\0', &b);
    return ret;
}

#else
typedef int filenotempty; /* make ISO standard happy */
#endif