/*
 * (C) 2004	Olaf Freyer	(aaron@cs.tu-berlin.de)
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

#ifndef INCLUDED_TEAM_TYPES
#define INCLUDED_TEAM_TYPES

# include "common/list.h"
#ifndef JUST_NEED_TYPES
# define JUST_NEED_TYPES
# include "account.h"
# undef JUST_NEED_TYPES
#else
# include "account.h"
#endif

#define MAX_TEAMSIZE 4

typedef struct team
#ifdef TEAM_INTERNAL_ACCESS
{
	unsigned char 	size;
	int		wins;
	int		losses;
	int		xp;
	int		level;
	int		rank;
	unsigned int	teamid;
	unsigned int	teammembers[MAX_TEAMSIZE];
	t_account *     members[MAX_TEAMSIZE];
}
#endif
t_team;

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_TEAM_PROTOS
#define INCLUDED_TEAM_PROTOS

extern int teamlist_load(void);
extern int teamlist_unload(void);
extern int teams_destroy(t_list *);

#endif
#endif
