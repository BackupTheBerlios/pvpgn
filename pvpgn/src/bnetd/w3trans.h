/*
 * Copyright (C) 2002
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
#ifndef INCLUDED_W3TRANS_TYPES
#define INCLUDED_W3TRANS_TYPES

#ifdef W3TRANS_INTERNAL_ACCESS

#ifdef JUST_NEED_TYPES
# include "common/addr.h"
#else
# define JUST_NEED_TYPES
# include "common/addr.h"
# undef JUST_NEED_TYPES
#endif

typedef struct
{
    t_netaddr * network;
    t_addr *    output;
} t_w3trans;

#endif

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_W3TRANS_PROTOS
#define INCLUDED_W3TRANS_PROTOS

#define JUST_NEED_TYPES
#include "common/addr.h"
#undef JUST_NEED_TYPES

extern int w3trans_load(char const * filename);
extern int w3trans_unload(void);
extern int w3trans_reload(char const * filename);
extern void w3trans_net(unsigned int clientaddr, unsigned int *w3ip, unsigned short *w3port);

#endif
#endif
