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
#ifndef INCLUDED_PMMAP_PROTOS
#define INCLUDED_PMMAP_PROTOS

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#ifndef HAVE_MMAP

#define PROT_NONE       0x00    /* no permissions */
#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */

#define MAP_SHARED      0x0001          /* share changes */
#define MAP_PRIVATE     0x0002          /* changes are private */
#define MAP_COPY        MAP_PRIVATE     /* Obsolete */

#define MAP_FAILED      ((void *)-1)

extern void * mmap(void *addr, unsigned len, int prot, int flags, int fd, unsigned offset);
extern int munmap(void *addr, unsigned len);

#endif

#endif
