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
  char *   clan_motd;
  t_list * members;
}
#endif
t_clan;

#define CLAN_CHIEFTAIN 0x04
#define CLAN_SHAMAN    0x03
#define CLAN_GRUNT     0x02
#define CLAN_PEON      0x01
#define CLAN_NEW       0x00

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_CLAN_PROTOS
#define INCLUDED_CLAN_PROTOS

#define JUST_NEED_TYPES
#include "common/list.h"
#undef JUST_NEED_TYPES

int clanlist_load(char const * clanshort);
int clanlist_unload();

int clanlist_add(t_clan * clan);
// adds a clan to the clanlist. does NOT check if clan with same clanshort is allready present
// it returns the clanid of the clan just added or -1 in case of error
// if clan->clanid==0 (in case of a newly created clan) it sets a valid new clan->clanid

int clan_save(t_clan * clan);
// forces the saving of the clan to disc (no other way to save yet)


t_clan * get_clan_by_clanid(int cid);
t_clan * get_clan_by_clanshort(char clanshort[4]);

t_clanmember * clan_get_first_member(t_clan * clan);
t_clanmember * clan_get_next_member();

int    clanmember_get_uid(t_clanmember * member);
char   clanmember_get_status(t_clanmember * member);
time_t clanmember_get_join_time(t_clanmember * member);

char  * clan_get_clanname(t_clan * clan);
char  * clan_get_clan_motd(t_clan * clan);
int     clan_get_clanid(t_clan * clan);
time_t  clan_get_creation_time(t_clan * clan);

int clan_add_member(t_clan * clan, int uid, char status);
// adds a new member with given uid and status to a clan
// does NOT automatically save the clan (and the changes within) to disc

t_clan * create_clan(int chieftain_uid, char clanshort[4], char * clanname, char * motd);
// creates a new clan with the given data and adds the chieftain as first member
// does NOT automatically save the newly created clan to disc

#endif
#endif
