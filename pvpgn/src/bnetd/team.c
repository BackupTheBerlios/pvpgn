/*
 * (C) 2004		Olaf Freyer	(aaron@cs.tu-berlin.de)
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
#define TEAM_INTERNAL_ACCESS
#include "common/setup_before.h"
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strdup.h"
#include "compat/pdir.h"
#include <errno.h>
#include "compat/strerror.h"
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/tag.h"
#include "common/util.h"
#include "common/xalloc.h"
#include "common/list.h"
#include "storage.h"
#include "account.h"
#include "team.h"
#include "common/setup_after.h"

static t_list *teamlist_head = NULL;
int max_teamid = 0;
int teamlist_add_team(t_team * team);

/* callback function for storage use */

static int _cb_load_teams(void *team)
{
    if (teamlist_add_team(team) < 0)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "failed to add team to teamlist");
	return -1;
    }

    if (((t_team *) team)->teamid > max_teamid)
	max_teamid = ((t_team *) team)->teamid;
    return 0;
}


int teamlist_add_team(t_team * team)
{
    t_account * account[MAX_TEAMSIZE];
    int i;
    
    if (!(team))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL team");
	return -1;
    }

    for (i=0; i<team->size; i++)
    {
	if (!(account[i] = accountlist_find_account_by_uid(team->teammembers[i])))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"at least one non-existant member in team %u - discarding team",team->teamid);
	    //FIXME: delete team file now???
	    return team->teamid; //we return teamid even though we have an error, we don't want unintentional overwriting
	}
    }
    
    for (i=0; i<team->size; i++)
      account_add_team(account[i],team);
    
    if (!(team->teamid))
	team->teamid = ++max_teamid;

    list_append_data(teamlist_head, team);

    return team->teamid;
}


int teamlist_load(void)
{
    // make sure to unload previous teamlist before loading again
    if (teamlist_head)
	teamlist_unload();

    teamlist_head = list_create();

    storage->load_teams(_cb_load_teams);

    return 0;
}


int teamlist_unload(void)
{
    t_elem *curr;
    t_team *team;

    if ((teamlist_head))
    {
	LIST_TRAVERSE(teamlist_head, curr)
	{
	    if (!(team = elem_get_data(curr)))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
		continue;
	    }
	    xfree((void *) team);
	    list_remove_elem(teamlist_head, &curr);
	}

	if (list_destroy(teamlist_head) < 0)
	    return -1;

	teamlist_head = NULL;
    }

    return 0;
}

int teams_destroy(t_list * teams)
{
    t_elem *curr;
    t_team *team;
    
    if ((teams))
    {
	LIST_TRAVERSE(teams,curr)
	{
	    if (!(team = elem_get_data(curr)))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
		continue;
	    }
	    list_remove_elem(teams, &curr);
	}
	
	if (list_destroy(teams) < 0)
	    return -1;
    }
    teams = NULL;

    return 0;
}
