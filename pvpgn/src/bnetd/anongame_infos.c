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
#define TIMER_INTERNAL_ACCESS
#include "common/setup_before.h"
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
#include "compat/strrchr.h"
#include <errno.h>
#include "compat/strerror.h"
#include "common/list.h"
#include "common/eventlog.h"
#include "common/util.h"
#include "anongame_infos.h"
#include "common/setup_after.h"

static FILE * fp = NULL;

t_anongame_infos * anongame_infos;

int anongame_infos_URL_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_URL * anongame_infos_URL;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_URL = malloc(sizeof(t_anongame_infos_URL))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_URL");
	    return -1;
	}
    
	anongame_infos_URL->player_URL			= NULL;
	anongame_infos_URL->server_URL			= NULL;
	anongame_infos_URL->tourney_URL			= NULL;

	anongame_infos_URL->ladder_PG_1v1_URL	= NULL;
	anongame_infos_URL->ladder_PG_ffa_URL	= NULL;
	anongame_infos_URL->ladder_PG_team_URL	= NULL;

	anongame_infos_URL->ladder_AT_2v2_URL	= NULL;
	anongame_infos_URL->ladder_AT_3v3_URL	= NULL;
	anongame_infos_URL->ladder_AT_4v4_URL	= NULL;

	anongame_infos->anongame_infos_URL=anongame_infos_URL;

	return 0;
}

int anongame_infos_URL_destroy(t_anongame_infos_URL * anongame_infos_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}
    
	if (anongame_infos_URL->player_URL)			free((void *)anongame_infos_URL->player_URL);
	if (anongame_infos_URL->server_URL)			free((void *)anongame_infos_URL->server_URL);
	if (anongame_infos_URL->tourney_URL)		free((void *)anongame_infos_URL->tourney_URL);

	if (anongame_infos_URL->ladder_PG_1v1_URL)	free((void *)anongame_infos_URL->ladder_PG_1v1_URL);
	if (anongame_infos_URL->ladder_PG_ffa_URL)	free((void *)anongame_infos_URL->ladder_PG_ffa_URL);
	if (anongame_infos_URL->ladder_PG_team_URL)	free((void *)anongame_infos_URL->ladder_PG_team_URL);

	if (anongame_infos_URL->ladder_AT_2v2_URL)	free((void *)anongame_infos_URL->ladder_AT_2v2_URL);
	if (anongame_infos_URL->ladder_AT_3v3_URL)	free((void *)anongame_infos_URL->ladder_AT_3v3_URL);
	if (anongame_infos_URL->ladder_AT_4v4_URL)	free((void *)anongame_infos_URL->ladder_AT_4v4_URL);

	free((void *)anongame_infos_URL);

	return 0;
}

t_anongame_infos_DESC * anongame_infos_DESC_init()
{
	t_anongame_infos_DESC * anongame_infos_DESC;

	if (!(anongame_infos_DESC = malloc(sizeof(t_anongame_infos_DESC))))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_DESC");
		return NULL;
	}

	anongame_infos_DESC->langID					= NULL;

	anongame_infos_DESC->ladder_PG_1v1_desc		= NULL;
	anongame_infos_DESC->ladder_PG_ffa_desc		= NULL;
	anongame_infos_DESC->ladder_PG_team_desc	= NULL;

	anongame_infos_DESC->ladder_AT_2v2_desc		= NULL;
	anongame_infos_DESC->ladder_AT_3v3_desc		= NULL;
	anongame_infos_DESC->ladder_AT_4v4_desc		= NULL;

	anongame_infos_DESC->gametype_1v1_short		= NULL;
	anongame_infos_DESC->gametype_1v1_long		= NULL;
	anongame_infos_DESC->gametype_2v2_short		= NULL;
	anongame_infos_DESC->gametype_2v2_long		= NULL;
	anongame_infos_DESC->gametype_3v3_short		= NULL;
	anongame_infos_DESC->gametype_3v3_long		= NULL;
	anongame_infos_DESC->gametype_4v4_short		= NULL;
	anongame_infos_DESC->gametype_4v4_long		= NULL;
	anongame_infos_DESC->gametype_ffa_short		= NULL;
	anongame_infos_DESC->gametype_ffa_long		= NULL;

	return anongame_infos_DESC;
}

int anongame_infos_DESC_destroy(t_anongame_infos_DESC * anongame_infos_DESC)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (anongame_infos_DESC->langID)				free((void *)anongame_infos_DESC->langID);
	if (anongame_infos_DESC->ladder_PG_1v1_desc)	free((void *)anongame_infos_DESC->ladder_PG_1v1_desc);
	if (anongame_infos_DESC->ladder_PG_ffa_desc)	free((void *)anongame_infos_DESC->ladder_PG_ffa_desc);
	if (anongame_infos_DESC->ladder_PG_team_desc)	free((void *)anongame_infos_DESC->ladder_PG_team_desc);

	if (anongame_infos_DESC->ladder_AT_2v2_desc)	free((void *)anongame_infos_DESC->ladder_AT_2v2_desc);
	if (anongame_infos_DESC->ladder_AT_3v3_desc)	free((void *)anongame_infos_DESC->ladder_AT_3v3_desc);
	if (anongame_infos_DESC->ladder_AT_4v4_desc)	free((void *)anongame_infos_DESC->ladder_AT_4v4_desc);

	if (anongame_infos_DESC->gametype_1v1_short)	free((void *)anongame_infos_DESC->gametype_1v1_short);
	if (anongame_infos_DESC->gametype_1v1_long)		free((void *)anongame_infos_DESC->gametype_1v1_long);
	if (anongame_infos_DESC->gametype_2v2_short)	free((void *)anongame_infos_DESC->gametype_2v2_short);
	if (anongame_infos_DESC->gametype_2v2_long)		free((void *)anongame_infos_DESC->gametype_2v2_long);
	if (anongame_infos_DESC->gametype_3v3_short)	free((void *)anongame_infos_DESC->gametype_3v3_short);
	if (anongame_infos_DESC->gametype_3v3_long)		free((void *)anongame_infos_DESC->gametype_3v3_long);
	if (anongame_infos_DESC->gametype_4v4_short)	free((void *)anongame_infos_DESC->gametype_4v4_short);
	if (anongame_infos_DESC->gametype_4v4_long)		free((void *)anongame_infos_DESC->gametype_4v4_long);
	if (anongame_infos_DESC->gametype_ffa_short)	free((void *)anongame_infos_DESC->gametype_ffa_short);
	if (anongame_infos_DESC->gametype_ffa_long)		free((void *)anongame_infos_DESC->gametype_ffa_long);

	free((void *)anongame_infos_DESC);

	return 0;
}

t_anongame_infos *  anongame_infos_init()
{
  t_anongame_infos * anongame_infos;

	if (!(anongame_infos = malloc(sizeof(t_anongame_infos))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos");
	    return NULL;
	}

	if (anongame_infos_URL_init(anongame_infos)!=0)
	{
		free((void *)anongame_infos);
		return NULL;
	}

	anongame_infos->anongame_infos_DESC = NULL;

    if (!(anongame_infos->anongame_infos_DESC_list = list_create()))
    {
        eventlog(eventlog_level_error,__FUNCTION__,"could not create list");
		anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
		free((void *)anongame_infos);
        return NULL;
    }
	return anongame_infos;
}

void anongame_infos_set_defaults(t_anongame_infos * anongame_infos)
{
    t_anongame_infos_URL * anongame_infos_URL;
    t_anongame_infos_DESC * anongame_infos_DESC;

    if (!(anongame_infos))
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
	return;
    }

    anongame_infos_URL  = anongame_infos->anongame_infos_URL;
    anongame_infos_DESC = anongame_infos->anongame_infos_DESC;

    if (!(anongame_infos_URL))
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL, trying to init");
	if (anongame_infos_URL_init(anongame_infos)!=0)
        { 
	  eventlog(eventlog_level_error,__FUNCTION__,"failed to init... PANIC!");
	  return;
        }
    }

    if (!(anongame_infos_DESC))
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC, trying to init");
	if (!(anongame_infos_DESC = anongame_infos_DESC_init()))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"failed to init... PANIC!");
	  return;
	}
	else
	anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
    }
    
    // now set default values

    if (!(anongame_infos_URL->server_URL)) 
	anongame_infos_URL_set_server_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->player_URL)) 
	anongame_infos_URL_set_player_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->tourney_URL)) 
	anongame_infos_URL_set_tourney_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_PG_1v1_URL)) 
	anongame_infos_URL_set_ladder_PG_1v1_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_PG_ffa_URL)) 
	anongame_infos_URL_set_ladder_PG_ffa_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_PG_team_URL)) 
	anongame_infos_URL_set_ladder_PG_team_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_AT_2v2_URL)) 
	anongame_infos_URL_set_ladder_AT_2v2_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_AT_3v3_URL)) 
	anongame_infos_URL_set_ladder_AT_3v3_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);
    if (!(anongame_infos_URL->ladder_AT_4v4_URL)) 
	anongame_infos_URL_set_ladder_AT_4v4_URL(anongame_infos_URL,PVPGN_DEFAULT_URL);

    if (!(anongame_infos_DESC->ladder_PG_1v1_desc)) 
	anongame_infos_DESC_set_ladder_PG_1v1_desc(anongame_infos_DESC,PVPGN_PG_1V1_DESC);
    if (!(anongame_infos_DESC->ladder_PG_ffa_desc)) 
	anongame_infos_DESC_set_ladder_PG_ffa_desc(anongame_infos_DESC,PVPGN_PG_FFA_DESC);
    if (!(anongame_infos_DESC->ladder_PG_team_desc)) 
	anongame_infos_DESC_set_ladder_PG_team_desc(anongame_infos_DESC,PVPGN_PG_TEAM_DESC);
    if (!(anongame_infos_DESC->ladder_AT_2v2_desc)) 
	anongame_infos_DESC_set_ladder_AT_2v2_desc(anongame_infos_DESC,PVPGN_AT_2V2_DESC);
    if (!(anongame_infos_DESC->ladder_AT_3v3_desc)) 
	anongame_infos_DESC_set_ladder_AT_3v3_desc(anongame_infos_DESC,PVPGN_AT_3V3_DESC);
    if (!(anongame_infos_DESC->ladder_AT_4v4_desc)) 
	anongame_infos_DESC_set_ladder_AT_4v4_desc(anongame_infos_DESC,PVPGN_AT_4V4_DESC);

    if (!(anongame_infos_DESC->gametype_1v1_short))
	anongame_infos_DESC_set_gametype_1v1_short(anongame_infos_DESC,PVPGN_1V1_GT_DESC);
    if (!(anongame_infos_DESC->gametype_1v1_long))
	anongame_infos_DESC_set_gametype_1v1_long(anongame_infos_DESC,PVPGN_1V1_GT_LONG);
    if (!(anongame_infos_DESC->gametype_2v2_short))
	anongame_infos_DESC_set_gametype_2v2_short(anongame_infos_DESC,PVPGN_2V2_GT_DESC);
    if (!(anongame_infos_DESC->gametype_2v2_long))
	anongame_infos_DESC_set_gametype_2v2_long(anongame_infos_DESC,PVPGN_2V2_GT_LONG);
    if (!(anongame_infos_DESC->gametype_3v3_short))
	anongame_infos_DESC_set_gametype_3v3_short(anongame_infos_DESC,PVPGN_3V3_GT_DESC);
    if (!(anongame_infos_DESC->gametype_3v3_long))
	anongame_infos_DESC_set_gametype_3v3_long(anongame_infos_DESC,PVPGN_3V3_GT_LONG);
    if (!(anongame_infos_DESC->gametype_4v4_short))
	anongame_infos_DESC_set_gametype_4v4_short(anongame_infos_DESC,PVPGN_4V4_GT_DESC);
    if (!(anongame_infos_DESC->gametype_4v4_long))
	anongame_infos_DESC_set_gametype_4v4_long(anongame_infos_DESC,PVPGN_4V4_GT_LONG);
    if (!(anongame_infos_DESC->gametype_ffa_short))
	anongame_infos_DESC_set_gametype_ffa_short(anongame_infos_DESC,PVPGN_FFA_GT_DESC);
    if (!(anongame_infos_DESC->gametype_ffa_long))
	anongame_infos_DESC_set_gametype_ffa_long(anongame_infos_DESC,PVPGN_FFA_GT_LONG);

}

int anongame_infos_destroy(t_anongame_infos * anongame_infos)
{
	t_elem					* curr;
    t_anongame_infos_DESC	* entry;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}
    
	if (anongame_infos->anongame_infos_DESC_list)
    {
		LIST_TRAVERSE(anongame_infos->anongame_infos_DESC_list,curr)
		{
			if (!(entry = elem_get_data(curr)))
			eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
			else
			{
				anongame_infos_DESC_destroy(entry);
			}
			list_remove_elem(anongame_infos->anongame_infos_DESC_list,curr);
		}
		list_destroy(anongame_infos->anongame_infos_DESC_list);
		anongame_infos->anongame_infos_DESC_list = NULL;
    }

	anongame_infos_DESC_destroy(anongame_infos->anongame_infos_DESC);
	anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);

	free((void *)anongame_infos);

	return 0;
}

int anongame_infos_URL_set_server_URL(t_anongame_infos_URL * anongame_infos_URL, char * server_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(server_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL server_URL");
		return -1;
	}

	if (!(temp = strdup(server_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for server_URL");
		return -1;
	}
	if (anongame_infos_URL->server_URL) free((void *)anongame_infos_URL->server_URL);
	anongame_infos_URL->server_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_server_url()
{
	return anongame_infos->anongame_infos_URL->server_URL;
}

int anongame_infos_URL_set_player_URL(t_anongame_infos_URL * anongame_infos_URL, char * player_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(player_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL player_URL");
		return -1;
	}

	if (!(temp = strdup(player_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for player_URL");
		return -1;
	}
	if (anongame_infos_URL->player_URL) free((void *)anongame_infos_URL->player_URL);
	anongame_infos_URL->player_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_player_url()
{
	return anongame_infos->anongame_infos_URL->player_URL;
}


int anongame_infos_URL_set_tourney_URL(t_anongame_infos_URL * anongame_infos_URL, char * tourney_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(tourney_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL tourney_URL");
		return -1;
	}

	if (!(temp = strdup(tourney_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for tourney_URL");
		return -1;
	}
	if (anongame_infos_URL->tourney_URL) free((void *)anongame_infos_URL->tourney_URL);
	anongame_infos_URL->tourney_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_tourney_url()
{
	return anongame_infos->anongame_infos_URL->tourney_URL;
}


int anongame_infos_URL_set_ladder_PG_1v1_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_1v1_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_PG_1v1_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_1v1_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_1v1_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_1v1_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_PG_1v1_URL) free((void *)anongame_infos_URL->ladder_PG_1v1_URL);
	anongame_infos_URL->ladder_PG_1v1_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_PG_1v1_url()
{
	return anongame_infos->anongame_infos_URL->ladder_PG_1v1_URL;
}

int anongame_infos_URL_set_ladder_PG_ffa_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_ffa_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_PG_ffa_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_ffa_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_ffa_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_ffa_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_PG_ffa_URL) free((void *)anongame_infos_URL->ladder_PG_ffa_URL);
	anongame_infos_URL->ladder_PG_ffa_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_PG_ffa_url()
{
	return anongame_infos->anongame_infos_URL->ladder_PG_ffa_URL;
}

int anongame_infos_URL_set_ladder_PG_team_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_team_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_PG_team_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_team_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_team_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_team_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_PG_team_URL) free((void *)anongame_infos_URL->ladder_PG_team_URL);
	anongame_infos_URL->ladder_PG_team_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_PG_team_url()
{
	return anongame_infos->anongame_infos_URL->ladder_PG_team_URL;
}

int anongame_infos_URL_set_ladder_AT_2v2_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_2v2_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_AT_2v2_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_AT_2v2_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_2v2_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_2v2_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_AT_2v2_URL) free((void *)anongame_infos_URL->ladder_AT_2v2_URL);
	anongame_infos_URL->ladder_AT_2v2_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_AT_2v2_url()
{
	return anongame_infos->anongame_infos_URL->ladder_AT_2v2_URL;
}

int anongame_infos_URL_set_ladder_AT_3v3_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_3v3_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_AT_3v3_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_AT_3v3_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_3v3_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_3v3_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_AT_3v3_URL) free((void *)anongame_infos_URL->ladder_AT_3v3_URL);
	anongame_infos_URL->ladder_AT_3v3_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_AT_3v3_url()
{
	return anongame_infos->anongame_infos_URL->ladder_AT_3v3_URL;
}

int anongame_infos_URL_set_ladder_AT_4v4_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_4v4_URL)
{
	char * temp;
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	if (!(ladder_AT_4v4_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_AT_4v4_URL");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_4v4_URL)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_4v4_URL");
		return -1;
	}
	if (anongame_infos_URL->ladder_AT_4v4_URL) free((void *)anongame_infos_URL->ladder_AT_4v4_URL);
	anongame_infos_URL->ladder_AT_4v4_URL = temp;

	return 0;
}

extern char * anongame_infos_URL_get_ladder_AT_4v4_url()
{
	return anongame_infos->anongame_infos_URL->ladder_AT_4v4_URL;
}


int anongame_infos_DESC_set_ladder_PG_1v1_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_1v1_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_PG_1v1_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_1v1_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_1v1_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_1v1_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_PG_1v1_desc) free((void *)anongame_infos_DESC->ladder_PG_1v1_desc);
	anongame_infos_DESC->ladder_PG_1v1_desc = temp;

	return 0;
}
int anongame_infos_DESC_set_ladder_PG_ffa_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_ffa_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_PG_ffa_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_ffa_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_ffa_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_ffa_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_PG_ffa_desc) free((void *)anongame_infos_DESC->ladder_PG_ffa_desc);
	anongame_infos_DESC->ladder_PG_ffa_desc = temp;

	return 0;
}

int anongame_infos_DESC_set_ladder_PG_team_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_team_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_PG_team_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_team_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_PG_team_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_PG_team_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_PG_team_desc) free((void *)anongame_infos_DESC->ladder_PG_team_desc);
	anongame_infos_DESC->ladder_PG_team_desc = temp;

	return 0;
}
int anongame_infos_DESC_set_ladder_AT_2v2_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_2v2_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_AT_2v2_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_AT_2v2_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_2v2_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_2v2_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_AT_2v2_desc) free((void *)anongame_infos_DESC->ladder_AT_2v2_desc);
	anongame_infos_DESC->ladder_AT_2v2_desc = temp;

	return 0;
}

int anongame_infos_DESC_set_ladder_AT_3v3_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_3v3_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_AT_3v3_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_AT_3v3_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_3v3_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_3v3_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_AT_3v3_desc) free((void *)anongame_infos_DESC->ladder_AT_3v3_desc);
	anongame_infos_DESC->ladder_AT_3v3_desc = temp;

	return 0;
}

int anongame_infos_DESC_set_ladder_AT_4v4_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_4v4_desc)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(ladder_AT_4v4_desc))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL ladder_PG_4v4_desc");
		return -1;
	}

	if (!(temp = strdup(ladder_AT_4v4_desc)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for ladder_AT_4v4_desc");
		return -1;
	}
	if (anongame_infos_DESC->ladder_AT_4v4_desc) free((void *)anongame_infos_DESC->ladder_AT_4v4_desc);
	anongame_infos_DESC->ladder_AT_4v4_desc = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_1v1_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_1v1_short)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_1v1_short))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_1v1_short");
		return -1;
	}

	if (!(temp = strdup(gametype_1v1_short)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_1v1_short");
		return -1;
	}
	if (anongame_infos_DESC->gametype_1v1_short) free((void *)anongame_infos_DESC->gametype_1v1_short);
	anongame_infos_DESC->gametype_1v1_short = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_1v1_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_1v1_long)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_1v1_long))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_1v1_long");
		return -1;
	}

	if (!(temp = strdup(gametype_1v1_long)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_1v1_long");
		return -1;
	}
	if (anongame_infos_DESC->gametype_1v1_long) free((void *)anongame_infos_DESC->gametype_1v1_long);
	anongame_infos_DESC->gametype_1v1_long = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_2v2_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2_short)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_2v2_short))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_2v2_short");
		return -1;
	}

	if (!(temp = strdup(gametype_2v2_short)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_2v2_short");
		return -1;
	}
	if (anongame_infos_DESC->gametype_2v2_short) free((void *)anongame_infos_DESC->gametype_2v2_short);
	anongame_infos_DESC->gametype_2v2_short = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_2v2_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2_long)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_2v2_long))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_2v2_long");
		return -1;
	}

	if (!(temp = strdup(gametype_2v2_long)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_2v2_long");
		return -1;
	}
	if (anongame_infos_DESC->gametype_2v2_long) free((void *)anongame_infos_DESC->gametype_2v2_long);
	anongame_infos_DESC->gametype_2v2_long = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_3v3_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3_short)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_3v3_short))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_1v1_short");
		return -1;
	}

	if (!(temp = strdup(gametype_3v3_short)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_3v3_short");
		return -1;
	}
	if (anongame_infos_DESC->gametype_3v3_short) free((void *)anongame_infos_DESC->gametype_3v3_short);
	anongame_infos_DESC->gametype_3v3_short = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_3v3_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3_long)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_3v3_long))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_3v3_long");
		return -1;
	}

	if (!(temp = strdup(gametype_3v3_long)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_3v3_long");
		return -1;
	}
	if (anongame_infos_DESC->gametype_3v3_long) free((void *)anongame_infos_DESC->gametype_3v3_long);
	anongame_infos_DESC->gametype_3v3_long = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_4v4_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4_short)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_4v4_short))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_4v4_short");
		return -1;
	}

	if (!(temp = strdup(gametype_4v4_short)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_4v4_short");
		return -1;
	}
	if (anongame_infos_DESC->gametype_4v4_short) free((void *)anongame_infos_DESC->gametype_4v4_short);
	anongame_infos_DESC->gametype_4v4_short = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_4v4_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4_long)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_4v4_long))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_4v4_long");
		return -1;
	}

	if (!(temp = strdup(gametype_4v4_long)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_4v4_long");
		return -1;
	}
	if (anongame_infos_DESC->gametype_4v4_long) free((void *)anongame_infos_DESC->gametype_4v4_long);
	anongame_infos_DESC->gametype_4v4_long = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_ffa_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_ffa_short)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_ffa_short))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_ffa_short");
		return -1;
	}

	if (!(temp = strdup(gametype_ffa_short)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_ffa_short");
		return -1;
	}
	if (anongame_infos_DESC->gametype_ffa_short) free((void *)anongame_infos_DESC->gametype_ffa_short);
	anongame_infos_DESC->gametype_ffa_short = temp;

	return 0;
}

int anongame_infos_DESC_set_gametype_ffa_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_ffa_long)
{
	char * temp;
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	if (!(gametype_ffa_long))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL gametype_ffa_long");
		return -1;
	}

	if (!(temp = strdup(gametype_ffa_long)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for gametype_ffa_long");
		return -1;
	}
	if (anongame_infos_DESC->gametype_ffa_long) free((void *)anongame_infos_DESC->gametype_ffa_long);
	anongame_infos_DESC->gametype_ffa_long = temp;

	return 0;
}

t_anongame_infos_DESC * anongame_infos_get_anongame_infos_DESC_by_langID(t_anongame_infos * anongame_infos, char * langID)
{
	t_elem					* curr;
    t_anongame_infos_DESC	* entry;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return NULL;
	}

	if (!(langID)) return anongame_infos->anongame_infos_DESC;

	if (!(anongame_infos->anongame_infos_DESC_list))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC_list - default values");
		return anongame_infos->anongame_infos_DESC;
	}
	    
	LIST_TRAVERSE(anongame_infos->anongame_infos_DESC_list,curr)
	{
		if (!(entry = (t_anongame_infos_DESC *)elem_get_data(curr)))
			eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		else
		{
			if ((entry->langID) && (strcmp(entry->langID,langID)==0)) return entry;
		}
	}
    
	return anongame_infos->anongame_infos_DESC;
}

extern char * anongame_infos_DESC_get_ladder_PG_1v1_desc(char * langID)
{
    char * result;
    
    if ((result=((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_PG_1v1_desc)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->ladder_PG_1v1_desc;
	
}

extern char * anongame_infos_DESC_get_ladder_PG_ffa_desc(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_PG_ffa_desc)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->ladder_PG_ffa_desc;
}

extern char * anongame_infos_DESC_get_ladder_PG_team_desc(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_PG_team_desc)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->ladder_PG_team_desc;
}

extern char * anongame_infos_DESC_get_ladder_AT_2v2_desc(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_AT_2v2_desc)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->ladder_AT_2v2_desc;
}

extern char * anongame_infos_DESC_get_ladder_AT_3v3_desc(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_AT_3v3_desc)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->ladder_AT_3v3_desc;
}

extern char * anongame_infos_DESC_get_ladder_AT_4v4_desc(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->ladder_AT_4v4_desc)))
	return result;
    else
    return anongame_infos->anongame_infos_DESC->ladder_AT_4v4_desc;
}


extern char * anongame_infos_DESC_get_gametype_1v1_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_1v1_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_1v1_short;
}

extern char * anongame_infos_DESC_get_gametype_1v1_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_1v1_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_long;
}

extern char * anongame_infos_DESC_get_gametype_2v2_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_short;
}

extern char * anongame_infos_DESC_get_gametype_2v2_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_long;
}

extern char * anongame_infos_DESC_get_gametype_3v3_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3_short;
}

extern char * anongame_infos_DESC_get_gametype_3v3_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3_long;
}

extern char * anongame_infos_DESC_get_gametype_4v4_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4_short;
}

extern char * anongame_infos_DESC_get_gametype_4v4_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4_long;
}

extern char * anongame_infos_DESC_get_gametype_ffa_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_ffa_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_ffa_short;
}

extern char * anongame_infos_DESC_get_gametype_ffa_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_ffa_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_ffa_long;
}

typedef int (* t_URL_string_handler)(t_anongame_infos_URL * anongame_infos_URL, char * text);

typedef struct {
	const char				* anongame_infos_URL_string;
	t_URL_string_handler	URL_string_handler;
} t_anongame_infos_URL_table_row;

typedef int (* t_DESC_string_handler)(t_anongame_infos_DESC * anongame_infos_DESC, char * text);

typedef struct {
	const char				* anongame_infos_DESC_string;
	t_DESC_string_handler	DESC_string_handler;
} t_anongame_infos_DESC_table_row;

static const t_anongame_infos_URL_table_row URL_handler_table[] = 
{
	{ "server_URL",			anongame_infos_URL_set_server_URL },
	{ "player_URL",			anongame_infos_URL_set_player_URL },
	{ "tourney_URL",		anongame_infos_URL_set_tourney_URL },
	{ "ladder_PG_1v1_URL",	anongame_infos_URL_set_ladder_PG_1v1_URL },
	{ "ladder_PG_ffa_URL",	anongame_infos_URL_set_ladder_PG_ffa_URL },
	{ "ladder_PG_team_URL",	anongame_infos_URL_set_ladder_PG_team_URL },
	{ "ladder_AT_2v2_URL",	anongame_infos_URL_set_ladder_AT_2v2_URL },
	{ "ladder_AT_3v3_URL",	anongame_infos_URL_set_ladder_AT_3v3_URL },
	{ "ladder_AT_4v4_URL",	anongame_infos_URL_set_ladder_AT_4v4_URL },
	{ NULL,	NULL }
};

static const t_anongame_infos_DESC_table_row DESC_handler_table[] = 
{
	{ "ladder_PG_1v1_desc",		anongame_infos_DESC_set_ladder_PG_1v1_desc },
	{ "ladder_PG_ffa_desc",		anongame_infos_DESC_set_ladder_PG_ffa_desc },
	{ "ladder_PG_team_desc",	anongame_infos_DESC_set_ladder_PG_team_desc },
	{ "ladder_AT_2v2_desc",		anongame_infos_DESC_set_ladder_AT_2v2_desc },
	{ "ladder_AT_3v3_desc",		anongame_infos_DESC_set_ladder_AT_3v3_desc },
	{ "ladder_AT_4v4_desc",		anongame_infos_DESC_set_ladder_AT_4v4_desc },
	{ "gametype_1v1_short",		anongame_infos_DESC_set_gametype_1v1_short },
	{ "gametype_1v1_long",		anongame_infos_DESC_set_gametype_1v1_long },
	{ "gametype_2v2_short",		anongame_infos_DESC_set_gametype_2v2_short },
	{ "gametype_2v2_long",		anongame_infos_DESC_set_gametype_2v2_long },
	{ "gametype_3v3_short",		anongame_infos_DESC_set_gametype_3v3_short },
	{ "gametype_3v3_long",		anongame_infos_DESC_set_gametype_3v3_long },
	{ "gametype_4v4_short",		anongame_infos_DESC_set_gametype_4v4_short },
	{ "gametype_4v4_long",		anongame_infos_DESC_set_gametype_4v4_long },
	{ "gametype_ffa_short",		anongame_infos_DESC_set_gametype_ffa_short },
	{ "gametype_ffa_long",		anongame_infos_DESC_set_gametype_ffa_long },

	{ NULL, NULL }
};

typedef enum
{
	parse_UNKNOWN,
	parse_URL,
	parse_DESC
} t_parse_mode;

typedef enum
{
	changed,
	unchanged
} t_parse_state;

t_parse_mode switch_parse_mode(char * text, char * langID)
{
	if (!(text)) return parse_UNKNOWN;
	else if (strcmp(text,"[URL]")==0) return parse_URL;
	else if (strcmp(text,"[DEFAULT_DESC]")==0) 
	{
		langID[0] = '\0';
		return parse_DESC;
	}
	else if (strlen(text)==5) 
	{
		strncpy(langID,&(text[1]),3);
		langID[3] = '\0';
		return parse_DESC;
	}
	else
		eventlog(eventlog_level_error,__FUNCTION__,"got invalid section name: %s", text);
	return parse_UNKNOWN;
}

extern int anongame_infos_load(char const * filename)
{
    unsigned int			line;
    unsigned int			pos;
    char *					buff;
	char *					temp;
	char 					langID[4]			= "    ";
	t_parse_mode			parse_mode			= parse_UNKNOWN;
	t_parse_state			parse_state			= unchanged;
	t_anongame_infos_DESC *	anongame_infos_DESC	= NULL;
	char *					pointer;
	char *					variable			= NULL;
	char *					value				= NULL;
	t_anongame_infos_DESC_table_row	const *		DESC_table_row;
	t_anongame_infos_URL_table_row	const *		URL_table_row;
    
	langID[0]='\0';
	
    if (!filename)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
        return -1;
    }
    
    if (!(anongame_infos=anongame_infos_init()))
    {
        eventlog(eventlog_level_error,__FUNCTION__,"could not init anongame_infos");
        return -1;
    }
	
    if (!(fp = fopen(filename,"r")))
    {
        eventlog(eventlog_level_error,"anongameinfo_load","could not open file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
		anongame_infos_destroy(anongame_infos);
	    return -1;
    }
    
    for (line=1; (buff = file_get_line(fp)); line++)
    {
        for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
        if (buff[pos]=='\0' || buff[pos]=='#')
        {
            free(buff);
            continue;
        }
        if ((temp = strrchr(buff,'#')))
        {
	    unsigned int len;
	    unsigned int endpos;
	    
            *temp = '\0';
	    len = strlen(buff)+1;
            for (endpos=len-1; buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
            buff[endpos+1] = '\0';
        }
	
	if (buff[0]!='\0')
		switch(parse_mode)
		{
		case parse_UNKNOWN:
			if ((buff[0]!='[') || (buff[strlen(buff)-1]!=']'))
			{
				eventlog(eventlog_level_error,__FUNCTION__,"expected [] section start, but found %s",buff);
			}
			else
			{
				parse_mode = switch_parse_mode(buff,langID);
				parse_state = changed;
			}
			break;
		case parse_URL:
			if ((buff[0]=='[') && (buff[strlen(buff)-1]==']'))
			{
				if ((parse_mode == parse_DESC) && (parse_state == changed))
				{
					if (langID[0]!='\0')
						list_append_data(anongame_infos->anongame_infos_DESC_list,anongame_infos_DESC);
					else
					{
						if (anongame_infos->anongame_infos_DESC == NULL)
						  anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
						else
						{
							eventlog(eventlog_level_error,__FUNCTION__,"found another default_DESC block, deleting previous");
							anongame_infos_DESC_destroy(anongame_infos->anongame_infos_DESC);
							anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
						}
					}
					anongame_infos_DESC = NULL;
				}
				parse_mode = switch_parse_mode(buff,langID);
				parse_state = changed;
			}
			else
			{

				parse_state = unchanged;
				variable = buff;
				pointer = strchr(variable,'=');
				for(pointer--;pointer[0]==' ';pointer--);
				pointer[1]='\0';
				pointer++;
				pointer++;
				pointer = strchr(pointer,'\"');
				pointer++;
				value = pointer;
				pointer = strchr(pointer,'\"');
				pointer[0]='\0';
				
				for(URL_table_row = URL_handler_table; URL_table_row->anongame_infos_URL_string != NULL; URL_table_row++)
					if (strcmp(URL_table_row->anongame_infos_URL_string, variable)==0) {
						if (URL_table_row->URL_string_handler != NULL) URL_table_row->URL_string_handler(anongame_infos->anongame_infos_URL,value);
					}

			}
			break;
		case parse_DESC:
			if ((buff[0]=='[') && (buff[strlen(buff)-1]==']'))
			{
				if ((parse_mode == parse_DESC) && (parse_state == unchanged))
				{
				  if (langID[0]!='\0')
					list_append_data(anongame_infos->anongame_infos_DESC_list,anongame_infos_DESC);
				 else
					{
						if (anongame_infos->anongame_infos_DESC == NULL)
						  anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
						else
						{
							eventlog(eventlog_level_error,__FUNCTION__,"found another default_DESC block, deleting previous");
							anongame_infos_DESC_destroy(anongame_infos->anongame_infos_DESC);
							anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
						}
					}
					anongame_infos_DESC = NULL;
				}
				parse_mode = switch_parse_mode(buff,langID);
				parse_state = changed;
			}
			else
			{
				if (parse_state == changed) 
				{
					anongame_infos_DESC = anongame_infos_DESC_init();
					parse_state = unchanged;
					eventlog(eventlog_level_info,__FUNCTION__,"got langID: [%s]",langID);
					if (langID[0]!='\0') anongame_infos_DESC->langID = strdup(langID);
				}

				variable = buff;
				pointer = strchr(variable,'=');
				for(pointer--;pointer[0]==' ';pointer--);
				pointer[1]='\0';
				pointer++;
				pointer++;
				pointer = strchr(pointer,'\"');
				pointer++;
				value = pointer;
				pointer = strchr(pointer,'\"');
				pointer[0]='\0';

				for(DESC_table_row = DESC_handler_table; DESC_table_row->anongame_infos_DESC_string != NULL; DESC_table_row++)
					if (strcmp(DESC_table_row->anongame_infos_DESC_string, variable)==0) {
						if (DESC_table_row->DESC_string_handler != NULL) DESC_table_row->DESC_string_handler(anongame_infos_DESC,value);
					}

			}

			break;
		}
	free(buff);
    }
  
    if (anongame_infos_DESC)
      {
	if (langID[0]!='\0')
	{
	  list_append_data(anongame_infos->anongame_infos_DESC_list,anongame_infos_DESC);
	}
	else
	  {
	    if (anongame_infos->anongame_infos_DESC == NULL)
	      anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
	    else
	      {
		eventlog(eventlog_level_error,__FUNCTION__,"found another default_DESC block, deleting previous");
		anongame_infos_DESC_destroy(anongame_infos->anongame_infos_DESC);
		anongame_infos->anongame_infos_DESC = anongame_infos_DESC;
	      }
	  }
      }

    fclose(fp);
    
    anongame_infos_set_defaults(anongame_infos);

    return 0;
}

extern int anongame_infos_unload()
{
	return anongame_infos_destroy(anongame_infos);
}

