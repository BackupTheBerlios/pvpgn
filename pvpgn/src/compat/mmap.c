/*
 * Copyright (C) 2003 Dizzy
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
#ifndef HAVE_MMAP
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "mmap.h"
#include "common/setup_after.h"

extern void * pmmap(void *addr, unsigned len, int prot, int flags, int fd, unsigned offset)
{
    void *mem;
    int pos, res;

    if ((mem = malloc(len)) == NULL) return MAP_FAILED;
    pos = 0;
    while(pos < len) {
	res = read(fd, (char *)mem + pos, len - pos);
	if (res < 0) {
	    free(mem);
	    return MAP_FAILED;
	}
	pos += res;
    }

    return mem;
}

extern int pmunmap(void *addr, unsigned len)
{
    free(addr);
    return 0;
}
#else
typedef int filenotempty; /* make ISO standard happy */
#endif
