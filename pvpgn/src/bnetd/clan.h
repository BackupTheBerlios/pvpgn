/*
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

#ifndef INCLUDED_CLAN_TYPES
#define INCLUDED_CLAN_TYPES

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

#ifdef CLAN_INTERNAL_ACCESS

#ifdef JUST_NEED_TYPES
# include <stdio.h>
# include "common/list.h"
#else
# define JUST_NEED_TYPES
# include <stdio.h>
# include "common/list.h"
# undef JUST_NEED_TYPES
#endif

#endif

#ifdef CLAN_INTERNAL_ACCESS
typedef struct clanmember
{
  int    uid;
  char   status;
  time_t join_time;
} t_clanmember;
#endif

typedef struct clan
#ifdef CLAN_INTERNAL_ACCESS
{
  int      clanid;
  char     clanshort[4];
  char *   clanname;
  time_t   creation_time;
  t_list * members;
}
#endif
t_clan;

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_CLAN_PROTOS
#define INCLUDED_CLAN_PROTOS

#define JUST_NEED_TYPES
#include "common/list.h"
#undef JUST_NEED_TYPES

int clanlist_load(char const * clanshort);
int clanlist_unload();

#endif
#endif
