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
#include "common/packet.h"
#include "common/tag.h"
#include "common/bn_type.h"
#include "zlib/pvpgn_zlib.h"
#include "tournament.h"
#include "anongame_maplists.h"
#include "anongame_infos.h"
#include "common/setup_after.h"

static FILE * fp = NULL;

static t_anongame_infos * anongame_infos;

static int zlib_compress(void const * src, int srclen, char ** dest, int * destlen);
static int anongame_infos_data_load(void);

static int anongame_infos_URL_init(t_anongame_infos * anongame_infos)
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

static int anongame_infos_URL_destroy(t_anongame_infos_URL * anongame_infos_URL)
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

static t_anongame_infos_DESC * anongame_infos_DESC_init(void)
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
	anongame_infos_DESC->gametype_sffa_short	= NULL;
	anongame_infos_DESC->gametype_sffa_long		= NULL;
	anongame_infos_DESC->gametype_tffa_short	= NULL;
	anongame_infos_DESC->gametype_tffa_long		= NULL;
	anongame_infos_DESC->gametype_2v2v2_short	= NULL;
	anongame_infos_DESC->gametype_2v2v2_long	= NULL;
	anongame_infos_DESC->gametype_3v3v3_short	= NULL;
	anongame_infos_DESC->gametype_3v3v3_long	= NULL;
	anongame_infos_DESC->gametype_4v4v4_short	= NULL;
	anongame_infos_DESC->gametype_4v4v4_long	= NULL;
	anongame_infos_DESC->gametype_2v2v2v2_short	= NULL;
	anongame_infos_DESC->gametype_2v2v2v2_long	= NULL;
	anongame_infos_DESC->gametype_3v3v3v3_short	= NULL;
	anongame_infos_DESC->gametype_3v3v3v3_long	= NULL;
	anongame_infos_DESC->gametype_5v5_short		= NULL;
	anongame_infos_DESC->gametype_5v5_long		= NULL;
	anongame_infos_DESC->gametype_6v6_short		= NULL;
	anongame_infos_DESC->gametype_6v6_long		= NULL;


	return anongame_infos_DESC;
}

static int anongame_infos_DESC_destroy(t_anongame_infos_DESC * anongame_infos_DESC)
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
	if (anongame_infos_DESC->gametype_sffa_short)	free((void *)anongame_infos_DESC->gametype_sffa_short);
	if (anongame_infos_DESC->gametype_sffa_long)		free((void *)anongame_infos_DESC->gametype_sffa_long);
	if (anongame_infos_DESC->gametype_tffa_short)	free((void *)anongame_infos_DESC->gametype_tffa_short);
	if (anongame_infos_DESC->gametype_tffa_long)		free((void *)anongame_infos_DESC->gametype_tffa_long);
	if (anongame_infos_DESC->gametype_2v2v2_short)	free ((void *)anongame_infos_DESC->gametype_2v2v2_short);
	if (anongame_infos_DESC->gametype_2v2v2_long)	free ((void *)anongame_infos_DESC->gametype_2v2v2_long);
	if (anongame_infos_DESC->gametype_3v3v3_short)	free ((void *)anongame_infos_DESC->gametype_3v3v3_short);
	if (anongame_infos_DESC->gametype_3v3v3_long)	free ((void *)anongame_infos_DESC->gametype_3v3v3_long);
	if (anongame_infos_DESC->gametype_4v4v4_short)	free ((void *)anongame_infos_DESC->gametype_4v4v4_short);
	if (anongame_infos_DESC->gametype_4v4v4_long)	free ((void *)anongame_infos_DESC->gametype_4v4v4_long);
	if (anongame_infos_DESC->gametype_2v2v2v2_short)	free ((void *)anongame_infos_DESC->gametype_2v2v2v2_short);
	if (anongame_infos_DESC->gametype_2v2v2v2_long)	free ((void *)anongame_infos_DESC->gametype_2v2v2v2_long);
	if (anongame_infos_DESC->gametype_3v3v3v3_short)	free ((void *)anongame_infos_DESC->gametype_3v3v3v3_short);
	if (anongame_infos_DESC->gametype_3v3v3v3_long)	free ((void *)anongame_infos_DESC->gametype_3v3v3v3_long);
	if (anongame_infos_DESC->gametype_5v5_short)	free((void *)anongame_infos_DESC->gametype_5v5_short);
	if (anongame_infos_DESC->gametype_5v5_long)		free((void *)anongame_infos_DESC->gametype_5v5_long);
	if (anongame_infos_DESC->gametype_6v6_short)	free((void *)anongame_infos_DESC->gametype_6v6_short);
	if (anongame_infos_DESC->gametype_6v6_long)		free((void *)anongame_infos_DESC->gametype_6v6_long);
	

	free((void *)anongame_infos_DESC);

	return 0;
}

static int anongame_infos_THUMBSDOWN_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_THUMBSDOWN = malloc(sizeof(t_anongame_infos_THUMBSDOWN))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_THUMBSDOWN");
	    return -1;
	}
    
	anongame_infos_THUMBSDOWN->PG_1v1 = 0;
	anongame_infos_THUMBSDOWN->PG_2v2 = 0;
	anongame_infos_THUMBSDOWN->PG_3v3 = 0;
	anongame_infos_THUMBSDOWN->PG_4v4 = 0;
	anongame_infos_THUMBSDOWN->PG_ffa = 0;
	anongame_infos_THUMBSDOWN->AT_2v2 = 0;
	anongame_infos_THUMBSDOWN->AT_3v3 = 0;
	anongame_infos_THUMBSDOWN->AT_4v4 = 0;
	anongame_infos_THUMBSDOWN->AT_ffa = 0;	
	anongame_infos_THUMBSDOWN->PG_5v5 = 0;
	anongame_infos_THUMBSDOWN->PG_6v6 = 0;	
	anongame_infos_THUMBSDOWN->PG_2v2v2 = 0;
	anongame_infos_THUMBSDOWN->PG_3v3v3 = 0;
	anongame_infos_THUMBSDOWN->PG_4v4v4 = 0;
	anongame_infos_THUMBSDOWN->PG_2v2v2v2 = 0;	
	anongame_infos_THUMBSDOWN->PG_3v3v3v3 = 0;	
	anongame_infos_THUMBSDOWN->AT_2v2v2 = 0;	
	
	anongame_infos->anongame_infos_THUMBSDOWN=anongame_infos_THUMBSDOWN;

	return 0;
}

static int anongame_infos_THUMBSDOWN_destroy(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN)
{
	if (!(anongame_infos_THUMBSDOWN))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
		return -1;
	}

	free((void *)anongame_infos_THUMBSDOWN);

	return 0;
}

static int anongame_infos_ICON_REQ_WAR3_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_ICON_REQ_WAR3 = malloc(sizeof(t_anongame_infos_ICON_REQ_WAR3))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_ICON_REQ_WAR3");
	    return -1;
	}
    
	anongame_infos_ICON_REQ_WAR3->Level1 = 25;
	anongame_infos_ICON_REQ_WAR3->Level2 = 250;
	anongame_infos_ICON_REQ_WAR3->Level3 = 500;
	anongame_infos_ICON_REQ_WAR3->Level4 = 1500;
	
	anongame_infos->anongame_infos_ICON_REQ_WAR3=anongame_infos_ICON_REQ_WAR3;

	return 0;
}

static int anongame_infos_ICON_REQ_WAR3_destroy(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3)
{
	if (!(anongame_infos_ICON_REQ_WAR3))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_WAR3");
		return -1;
	}

	free((void *)anongame_infos_ICON_REQ_WAR3);

	return 0;
}

static int anongame_infos_ICON_REQ_W3XP_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_ICON_REQ_W3XP = malloc(sizeof(t_anongame_infos_ICON_REQ_W3XP))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_ICON_REQ_W3XP");
	    return -1;
	}
    
	anongame_infos_ICON_REQ_W3XP->Level1 = 25;
	anongame_infos_ICON_REQ_W3XP->Level2 = 150;
	anongame_infos_ICON_REQ_W3XP->Level3 = 350;
	anongame_infos_ICON_REQ_W3XP->Level4 = 750;
	anongame_infos_ICON_REQ_W3XP->Level5 = 1500;
	
	anongame_infos->anongame_infos_ICON_REQ_W3XP=anongame_infos_ICON_REQ_W3XP;

	return 0;
}

static int anongame_infos_ICON_REQ_W3XP_destroy(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP)
{
	if (!(anongame_infos_ICON_REQ_W3XP))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
		return -1;
	}

	free((void *)anongame_infos_ICON_REQ_W3XP);

	return 0;
}


static int anongame_infos_ICON_REQ_TOURNEY_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_ICON_REQ_TOURNEY = malloc(sizeof(t_anongame_infos_ICON_REQ_TOURNEY))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_ICON_REQ_TOURNEY");
	    return -1;
	}
    
	anongame_infos_ICON_REQ_TOURNEY->Level1 = 10;
	anongame_infos_ICON_REQ_TOURNEY->Level2 = 75;
	anongame_infos_ICON_REQ_TOURNEY->Level3 = 150;
	anongame_infos_ICON_REQ_TOURNEY->Level4 = 250;
	anongame_infos_ICON_REQ_TOURNEY->Level5 = 500;
	
	anongame_infos->anongame_infos_ICON_REQ_TOURNEY=anongame_infos_ICON_REQ_TOURNEY;

	return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_destroy(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY)
{
	if (!(anongame_infos_ICON_REQ_TOURNEY))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
		return -1;
	}

	free((void *)anongame_infos_ICON_REQ_TOURNEY);

	return 0;
}

static t_anongame_infos_data_lang * anongame_infos_data_lang_init(char * langID)
{
	t_anongame_infos_data_lang * anongame_infos_data_lang;

	if (!(anongame_infos_data_lang = malloc(sizeof(t_anongame_infos_data_lang))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_data_lang");
	    return NULL;
	}

	anongame_infos_data_lang->langID = strdup(langID);

	anongame_infos_data_lang->desc_data = NULL;
	anongame_infos_data_lang->ladr_data = NULL;

	anongame_infos_data_lang->desc_comp_data = NULL;
	anongame_infos_data_lang->ladr_comp_data = NULL;

	anongame_infos_data_lang->desc_len = 0;
	anongame_infos_data_lang->ladr_len = 0;

	anongame_infos_data_lang->desc_comp_len = 0;
	anongame_infos_data_lang->ladr_comp_len = 0;

	return anongame_infos_data_lang;
}

static int anongame_infos_data_lang_destroy(t_anongame_infos_data_lang * anongame_infos_data_lang)
{
	if (!(anongame_infos_data_lang))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_data_lang");
		return -1;
	}

	if(anongame_infos_data_lang->langID) free((void *)anongame_infos_data_lang->langID);

	if(anongame_infos_data_lang->desc_data) free((void *)anongame_infos_data_lang->desc_data);
	if(anongame_infos_data_lang->ladr_data) free((void *)anongame_infos_data_lang->ladr_data);

	if(anongame_infos_data_lang->desc_comp_data) free((void *)anongame_infos_data_lang->desc_comp_data);
	if(anongame_infos_data_lang->ladr_comp_data) free((void *)anongame_infos_data_lang->ladr_comp_data);

	free((void *)anongame_infos_data_lang);

	return 0;
}

static int anongame_infos_data_init(t_anongame_infos * anongame_infos)
{
	t_anongame_infos_data * anongame_infos_data;
	t_list * anongame_infos_data_lang;

	if (!(anongame_infos))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos");
		return -1;
	}

	if (!(anongame_infos_data = malloc(sizeof(t_anongame_infos_data))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_data");
	    return -1;
	}

	if (!(anongame_infos_data_lang = list_create()))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_data_lang");
	    return -1;
	}

	anongame_infos_data->url_data = NULL;
	anongame_infos_data->map_data = NULL;
	anongame_infos_data->type_data = NULL;
	anongame_infos_data->desc_data = NULL;
	anongame_infos_data->ladr_data = NULL;

	anongame_infos_data->url_comp_data = NULL;
	anongame_infos_data->map_comp_data = NULL;
	anongame_infos_data->type_comp_data = NULL;
	anongame_infos_data->desc_comp_data = NULL;
	anongame_infos_data->ladr_comp_data = NULL;

	anongame_infos_data->url_len = 0;
	anongame_infos_data->map_len = 0;
	anongame_infos_data->type_len = 0;
	anongame_infos_data->desc_len = 0;
	anongame_infos_data->ladr_len = 0;

	anongame_infos_data->url_comp_len = 0;
	anongame_infos_data->map_comp_len = 0;
	anongame_infos_data->type_comp_len = 0;
	anongame_infos_data->desc_comp_len = 0;
	anongame_infos_data->ladr_comp_len = 0;

	anongame_infos->anongame_infos_data_war3=anongame_infos_data;
	anongame_infos->anongame_infos_data_lang_war3=anongame_infos_data_lang;

	if (!(anongame_infos_data = malloc(sizeof(t_anongame_infos_data))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_data");
	    return -1;
	}

	if (!(anongame_infos_data_lang = list_create()))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for anongame_infos_data_lang");
	    return -1;
	}

	anongame_infos_data->url_data = NULL;
	anongame_infos_data->map_data = NULL;
	anongame_infos_data->type_data = NULL;
	anongame_infos_data->desc_data = NULL;
	anongame_infos_data->ladr_data = NULL;

	anongame_infos_data->url_comp_data = NULL;
	anongame_infos_data->map_comp_data = NULL;
	anongame_infos_data->type_comp_data = NULL;
	anongame_infos_data->desc_comp_data = NULL;
	anongame_infos_data->ladr_comp_data = NULL;

	anongame_infos_data->url_len = 0;
	anongame_infos_data->map_len = 0;
	anongame_infos_data->type_len = 0;
	anongame_infos_data->desc_len = 0;
	anongame_infos_data->ladr_len = 0;

	anongame_infos_data->url_comp_len = 0;
	anongame_infos_data->map_comp_len = 0;
	anongame_infos_data->type_comp_len = 0;
	anongame_infos_data->desc_comp_len = 0;
	anongame_infos_data->ladr_comp_len = 0;

	anongame_infos->anongame_infos_data_w3xp=anongame_infos_data;
	anongame_infos->anongame_infos_data_lang_w3xp=anongame_infos_data_lang;

	return 0;    
}

static int anongame_infos_data_destroy(t_anongame_infos_data * anongame_infos_data, t_list * anongame_infos_data_lang)
{
	t_elem * curr;
	t_anongame_infos_data_lang * entry;

	if(anongame_infos_data->url_data) free((void *)anongame_infos_data->url_data);
	if(anongame_infos_data->map_data) free((void *)anongame_infos_data->map_data);
	if(anongame_infos_data->type_data) free((void *)anongame_infos_data->type_data);
	if(anongame_infos_data->desc_data) free((void *)anongame_infos_data->desc_data);
	if(anongame_infos_data->ladr_data) free((void *)anongame_infos_data->ladr_data);

	if(anongame_infos_data->url_comp_data) free((void *)anongame_infos_data->url_comp_data);
	if(anongame_infos_data->map_comp_data) free((void *)anongame_infos_data->map_comp_data);
	if(anongame_infos_data->type_comp_data) free((void *)anongame_infos_data->type_comp_data);
	if(anongame_infos_data->desc_comp_data) free((void *)anongame_infos_data->desc_comp_data);
	if(anongame_infos_data->ladr_comp_data) free((void *)anongame_infos_data->ladr_comp_data);

	free((void *)anongame_infos_data);

	if (anongame_infos_data_lang)
    {
		LIST_TRAVERSE(anongame_infos_data_lang,curr)
		{
			if (!(entry = elem_get_data(curr)))
			eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
			else
			{
				anongame_infos_data_lang_destroy(entry);
			}
			list_remove_elem(anongame_infos_data_lang,curr);
		}
		list_destroy(anongame_infos_data_lang);
    }
	return 0;
}

t_anongame_infos *  anongame_infos_init(void)
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

	if (anongame_infos_THUMBSDOWN_init(anongame_infos)!=0)
	{
	  anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	  free((void *)anongame_infos);
	  return NULL;
	}

	if (anongame_infos_ICON_REQ_WAR3_init(anongame_infos)!=0)
	{
	  anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	  anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	  free((void *)anongame_infos);
	  return NULL;
	}

	if (anongame_infos_ICON_REQ_W3XP_init(anongame_infos)!=0)
	{
	  anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	  anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	  anongame_infos_ICON_REQ_WAR3_destroy(anongame_infos->anongame_infos_ICON_REQ_WAR3);
	  free((void *)anongame_infos);
	  return NULL;
	}

	if (anongame_infos_ICON_REQ_TOURNEY_init(anongame_infos)!=0)
	{
	  anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	  anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	  anongame_infos_ICON_REQ_WAR3_destroy(anongame_infos->anongame_infos_ICON_REQ_WAR3);
	  anongame_infos_ICON_REQ_W3XP_destroy(anongame_infos->anongame_infos_ICON_REQ_W3XP);
	  free((void *)anongame_infos);
	  return NULL;
	}

	if (anongame_infos_data_init(anongame_infos)!=0)
	{
	  anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	  anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	  anongame_infos_ICON_REQ_WAR3_destroy(anongame_infos->anongame_infos_ICON_REQ_WAR3);
	  anongame_infos_ICON_REQ_W3XP_destroy(anongame_infos->anongame_infos_ICON_REQ_W3XP);
	  anongame_infos_ICON_REQ_TOURNEY_destroy(anongame_infos->anongame_infos_ICON_REQ_TOURNEY);
	  free((void *)anongame_infos);
	  return NULL;
	}

	anongame_infos->anongame_infos_DESC = NULL;

    if (!(anongame_infos->anongame_infos_DESC_list = list_create()))
    {
        eventlog(eventlog_level_error,__FUNCTION__,"could not create list");

  	anongame_infos_ICON_REQ_TOURNEY_destroy(anongame_infos->anongame_infos_ICON_REQ_TOURNEY);
  	anongame_infos_ICON_REQ_W3XP_destroy(anongame_infos->anongame_infos_ICON_REQ_W3XP);
  	anongame_infos_ICON_REQ_WAR3_destroy(anongame_infos->anongame_infos_ICON_REQ_WAR3);
        anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	anongame_infos_URL_destroy(anongame_infos->anongame_infos_URL);
	anongame_infos_data_destroy(anongame_infos->anongame_infos_data_war3, anongame_infos->anongame_infos_data_lang_war3);
	anongame_infos_data_destroy(anongame_infos->anongame_infos_data_w3xp, anongame_infos->anongame_infos_data_lang_w3xp);
	free((void *)anongame_infos);
        return NULL;
    }
	return anongame_infos;
}

static int anongame_infos_destroy(t_anongame_infos * anongame_infos)
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
	anongame_infos_THUMBSDOWN_destroy(anongame_infos->anongame_infos_THUMBSDOWN);
	anongame_infos_ICON_REQ_TOURNEY_destroy(anongame_infos->anongame_infos_ICON_REQ_TOURNEY);
  	anongame_infos_ICON_REQ_W3XP_destroy(anongame_infos->anongame_infos_ICON_REQ_W3XP);
  	anongame_infos_ICON_REQ_WAR3_destroy(anongame_infos->anongame_infos_ICON_REQ_WAR3);
	anongame_infos_data_destroy(anongame_infos->anongame_infos_data_war3, anongame_infos->anongame_infos_data_lang_war3);
	anongame_infos_data_destroy(anongame_infos->anongame_infos_data_w3xp, anongame_infos->anongame_infos_data_lang_w3xp);

	free((void *)anongame_infos);

	return 0;
}

static int anongame_infos_set_str(char ** dst, char * src,char * errstr)
{
	char * temp;

	if (!(src))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL %s",errstr);
		return -1;
	}

	if (!(temp = strdup(src)))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"could not allocate mem for %s",errstr);
		return -1;
	}
	if (*dst) free((void *)*dst);
	*dst = temp;

	return 0;
}

static int anongame_infos_URL_set_server_URL(t_anongame_infos_URL * anongame_infos_URL, char * server_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->server_URL,server_URL,"server_URL");
}

extern char * anongame_infos_URL_get_server_url(void)
{
	return anongame_infos->anongame_infos_URL->server_URL;
}

static int anongame_infos_URL_set_player_URL(t_anongame_infos_URL * anongame_infos_URL, char * player_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}


	return anongame_infos_set_str(&anongame_infos_URL->player_URL,player_URL,"player_URL");
}

extern char * anongame_infos_URL_get_player_url(void)
{
	return anongame_infos->anongame_infos_URL->player_URL;
}


static int anongame_infos_URL_set_tourney_URL(t_anongame_infos_URL * anongame_infos_URL, char * tourney_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->tourney_URL,tourney_URL,"tourney_URL");

}

extern char * anongame_infos_URL_get_tourney_url(void)
{
	return anongame_infos->anongame_infos_URL->tourney_URL;
}


static int anongame_infos_URL_set_ladder_PG_1v1_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_1v1_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_PG_1v1_URL,ladder_PG_1v1_URL,"ladder_PG_1v1_URL");

}

extern char * anongame_infos_URL_get_ladder_PG_1v1_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_PG_1v1_URL;
}

static int anongame_infos_URL_set_ladder_PG_ffa_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_ffa_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_PG_ffa_URL,ladder_PG_ffa_URL,"ladder_PG_ffa_URL");
}

extern char * anongame_infos_URL_get_ladder_PG_ffa_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_PG_ffa_URL;
}

static int anongame_infos_URL_set_ladder_PG_team_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_PG_team_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_PG_team_URL,ladder_PG_team_URL,"ladder_PG_team_URL");
}

extern char * anongame_infos_URL_get_ladder_PG_team_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_PG_team_URL;
}

static int anongame_infos_URL_set_ladder_AT_2v2_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_2v2_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_AT_2v2_URL,ladder_AT_2v2_URL,"ladder_AT_2v2_URL");
}

extern char * anongame_infos_URL_get_ladder_AT_2v2_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_AT_2v2_URL;
}

static int anongame_infos_URL_set_ladder_AT_3v3_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_3v3_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_AT_3v3_URL,ladder_AT_3v3_URL,"ladder_AT_3v3_URL");
}

extern char * anongame_infos_URL_get_ladder_AT_3v3_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_AT_3v3_URL;
}

static int anongame_infos_URL_set_ladder_AT_4v4_URL(t_anongame_infos_URL * anongame_infos_URL, char * ladder_AT_4v4_URL)
{
	if (!(anongame_infos_URL))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_URL");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_URL->ladder_AT_4v4_URL,ladder_AT_4v4_URL,"ladder_AT_4v4_URL");
}

extern char * anongame_infos_URL_get_ladder_AT_4v4_url(void)
{
	return anongame_infos->anongame_infos_URL->ladder_AT_4v4_URL;
}


static int anongame_infos_DESC_set_ladder_PG_1v1_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_1v1_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_PG_1v1_desc,ladder_PG_1v1_desc,"ladder_PG_1v1_desc");
}
static int anongame_infos_DESC_set_ladder_PG_ffa_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_ffa_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_PG_ffa_desc,ladder_PG_ffa_desc,"ladder_PG_ffa_desc");
}

static int anongame_infos_DESC_set_ladder_PG_team_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_PG_team_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_PG_team_desc,ladder_PG_team_desc,"ladder_PG_team_desc");
}
static int anongame_infos_DESC_set_ladder_AT_2v2_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_2v2_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_AT_2v2_desc,ladder_AT_2v2_desc,"ladder_AT_2v2_desc");
}

static int anongame_infos_DESC_set_ladder_AT_3v3_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_3v3_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_AT_3v3_desc,ladder_AT_3v3_desc,"ladder_AT_3v3_desc");
}

static int anongame_infos_DESC_set_ladder_AT_4v4_desc(t_anongame_infos_DESC * anongame_infos_DESC, char * ladder_AT_4v4_desc)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->ladder_AT_4v4_desc,ladder_AT_4v4_desc,"ladder_AT_4v4_desc");
}

static int anongame_infos_DESC_set_gametype_1v1_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_1v1_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_1v1_short,gametype_1v1_short,"gametype_1v1_short");
}

static int anongame_infos_DESC_set_gametype_1v1_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_1v1_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_1v1_long,gametype_1v1_long,"gametype_1v1_long");
}

static int anongame_infos_DESC_set_gametype_2v2_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2_short,gametype_2v2_short,"gametype_2v2_short");
}

static int anongame_infos_DESC_set_gametype_2v2_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2_long,gametype_2v2_long,"gametype_2v2_long");
}

static int anongame_infos_DESC_set_gametype_3v3_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3_short,gametype_3v3_short,"gametype_3v3_short");
}

static int anongame_infos_DESC_set_gametype_3v3_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3_long,gametype_3v3_long,"gametype_3v3_long");
}

static int anongame_infos_DESC_set_gametype_4v4_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_4v4_short,gametype_4v4_short,"gametype_4v4_short");
}

static int anongame_infos_DESC_set_gametype_4v4_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_4v4_long,gametype_4v4_long,"gametype_4v4_long");
}


int anongame_infos_DESC_set_gametype_5v5_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_5v5_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_5v5_short,gametype_5v5_short,"gametype_5v5_short");
}

static int anongame_infos_DESC_set_gametype_5v5_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_5v5_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_5v5_long,gametype_5v5_long,"gametype_5v5_long");
}

static int anongame_infos_DESC_set_gametype_6v6_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_6v6_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_6v6_short,gametype_6v6_short,"gametype_6v6_short");
}

static int anongame_infos_DESC_set_gametype_6v6_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_6v6_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_6v6_long,gametype_6v6_long,"gametype_6v6_long");
}


static int anongame_infos_DESC_set_gametype_sffa_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_sffa_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_sffa_short,gametype_sffa_short,"gametype_sffa_short");
}

static int anongame_infos_DESC_set_gametype_sffa_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_sffa_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_sffa_long,gametype_sffa_long,"gametype_sffa_long");
}

static int anongame_infos_DESC_set_gametype_tffa_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_tffa_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_tffa_short,gametype_tffa_short,"gametype_tffa_short");
}

static int anongame_infos_DESC_set_gametype_tffa_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_tffa_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_tffa_long,gametype_tffa_long,"gametype_tffa_long");
}

static int anongame_infos_DESC_set_gametype_2v2v2_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2v2_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2v2_short,gametype_2v2v2_short,"gametype_2v2v2_short");
}

static int anongame_infos_DESC_set_gametype_2v2v2_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2v2_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2v2_long,gametype_2v2v2_long,"gametype_2v2v2_long");
}


static int anongame_infos_DESC_set_gametype_3v3v3_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3v3_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3v3_short,gametype_3v3v3_short,"gametype_3v3v3_short");
}

static int anongame_infos_DESC_set_gametype_3v3v3_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3v3_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3v3_long,gametype_3v3v3_long,"gametype_3v3v3_long");
}

static int anongame_infos_DESC_set_gametype_4v4v4_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4v4_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_4v4v4_short,gametype_4v4v4_short,"gametype_4v4v4_short");
}

static int anongame_infos_DESC_set_gametype_4v4v4_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_4v4v4_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_4v4v4_long,gametype_4v4v4_long,"gametype_4v4v4_long");
}

static int anongame_infos_DESC_set_gametype_2v2v2v2_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2v2v2_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2v2v2_short,gametype_2v2v2v2_short,"gametype_2v2v2v2_short");
}

static int anongame_infos_DESC_set_gametype_2v2v2v2_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_2v2v2v2_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_2v2v2v2_long,gametype_2v2v2v2_long,"gametype_2v2v2v2_long");
}

static int anongame_infos_DESC_set_gametype_3v3v3v3_short(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3v3v3_short)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3v3v3_short,gametype_3v3v3v3_short,"gametype_3v3v3v3_short");
}

static int anongame_infos_DESC_set_gametype_3v3v3v3_long(t_anongame_infos_DESC * anongame_infos_DESC, char * gametype_3v3v3v3_long)
{
	if (!(anongame_infos_DESC))
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_DESC");
		return -1;
	}

	return anongame_infos_set_str(&anongame_infos_DESC->gametype_3v3v3v3_long,gametype_3v3v3v3_long,"gametype_3v3v3v3_long");
}

static t_anongame_infos_DESC * anongame_infos_get_anongame_infos_DESC_by_langID(t_anongame_infos * anongame_infos, char * langID)
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

/**********/
static char * anongame_infos_DESC_get_gametype_1v1_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_1v1_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_1v1_short;
}

static char * anongame_infos_DESC_get_gametype_1v1_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_1v1_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_long;
}

static char * anongame_infos_DESC_get_gametype_2v2_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_short;
}

static char * anongame_infos_DESC_get_gametype_2v2_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2_long;
}

static char * anongame_infos_DESC_get_gametype_3v3_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3_short;
}

static char * anongame_infos_DESC_get_gametype_3v3_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3_long;
}

static char * anongame_infos_DESC_get_gametype_4v4_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4_short;
}

static char * anongame_infos_DESC_get_gametype_4v4_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4_long;
}

static char * anongame_infos_DESC_get_gametype_5v5_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_5v5_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_5v5_short;
}

static char * anongame_infos_DESC_get_gametype_5v5_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_5v5_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_5v5_long;
}

static char * anongame_infos_DESC_get_gametype_6v6_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_6v6_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_6v6_short;
}

static char * anongame_infos_DESC_get_gametype_6v6_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_6v6_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_6v6_long;
}

static char * anongame_infos_DESC_get_gametype_sffa_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_sffa_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_sffa_short;
}

static char * anongame_infos_DESC_get_gametype_sffa_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_sffa_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_sffa_long;
}

static char * anongame_infos_DESC_get_gametype_tffa_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_tffa_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_tffa_short;
}

static char * anongame_infos_DESC_get_gametype_tffa_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_tffa_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_tffa_long;
}

static char * anongame_infos_DESC_get_gametype_2v2v2_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2v2_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2v2_short;
}

static char * anongame_infos_DESC_get_gametype_2v2v2_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2v2_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2v2_long;
}

static char * anongame_infos_DESC_get_gametype_3v3v3_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3v3_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3v3_short;
}

static char * anongame_infos_DESC_get_gametype_3v3v3_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3v3_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3v3_long;
}

static char * anongame_infos_DESC_get_gametype_4v4v4_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4v4_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4v4_short;
}

static char * anongame_infos_DESC_get_gametype_4v4v4_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_4v4v4_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_4v4v4_long;
}

static char * anongame_infos_DESC_get_gametype_2v2v2v2_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2v2v2_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2v2v2_short;
}

static char * anongame_infos_DESC_get_gametype_2v2v2v2_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_2v2v2v2_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_2v2v2v2_long;
}

static char * anongame_infos_DESC_get_gametype_3v3v3v3_short(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3v3v3_short)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3v3v3_short;
}

static char * anongame_infos_DESC_get_gametype_3v3v3v3_long(char * langID)
{
    char * result;

    if ((result = ((anongame_infos_get_anongame_infos_DESC_by_langID(anongame_infos, langID))->gametype_3v3v3v3_long)))
	return result;
    else
	return anongame_infos->anongame_infos_DESC->gametype_3v3v3v3_long;
}


/**********/
extern char * anongame_infos_get_short_desc(char * langID, int queue)
{
    switch(queue) {
	case ANONGAME_TYPE_1V1:
	    return anongame_infos_DESC_get_gametype_1v1_short(langID);
	case ANONGAME_TYPE_2V2:
	    return anongame_infos_DESC_get_gametype_2v2_short(langID);
	case ANONGAME_TYPE_3V3:
	    return anongame_infos_DESC_get_gametype_3v3_short(langID);
	case ANONGAME_TYPE_4V4:
	    return anongame_infos_DESC_get_gametype_4v4_short(langID);
	case ANONGAME_TYPE_5V5:
	    return anongame_infos_DESC_get_gametype_5v5_short(langID);
	case ANONGAME_TYPE_6V6:
	    return anongame_infos_DESC_get_gametype_6v6_short(langID);
	case ANONGAME_TYPE_2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2_short(langID);
	case ANONGAME_TYPE_3V3V3:
	    return anongame_infos_DESC_get_gametype_3v3v3_short(langID);
	case ANONGAME_TYPE_4V4V4:
	    return anongame_infos_DESC_get_gametype_4v4v4_short(langID);
	case ANONGAME_TYPE_2V2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2v2_short(langID);
	case ANONGAME_TYPE_3V3V3V3:
	    return anongame_infos_DESC_get_gametype_3v3v3v3_short(langID);
        case ANONGAME_TYPE_SMALL_FFA:
	    return anongame_infos_DESC_get_gametype_sffa_short(langID);
        case ANONGAME_TYPE_TEAM_FFA:
	    return anongame_infos_DESC_get_gametype_tffa_short(langID);
        case ANONGAME_TYPE_AT_2V2:
	    return anongame_infos_DESC_get_gametype_2v2_short(langID);
        case ANONGAME_TYPE_AT_3V3:
	    return anongame_infos_DESC_get_gametype_3v3_short(langID);
        case ANONGAME_TYPE_AT_4V4:
	    return anongame_infos_DESC_get_gametype_4v4_short(langID);
	case ANONGAME_TYPE_AT_2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2_short(langID);
        case ANONGAME_TYPE_TY:
	    return tournament_get_format();
        default:
            eventlog(eventlog_level_error,__FUNCTION__, "invalid queue (%d)", queue);
    	    return NULL;
    }
}

extern char * anongame_infos_get_long_desc(char * langID, int queue)
{
    switch(queue) {
	case ANONGAME_TYPE_1V1:
	    return anongame_infos_DESC_get_gametype_1v1_long(langID);
	case ANONGAME_TYPE_2V2:
	    return anongame_infos_DESC_get_gametype_2v2_long(langID);
	case ANONGAME_TYPE_3V3:
	    return anongame_infos_DESC_get_gametype_3v3_long(langID);
	case ANONGAME_TYPE_4V4:
	    return anongame_infos_DESC_get_gametype_4v4_long(langID);
	case ANONGAME_TYPE_5V5:
	    return anongame_infos_DESC_get_gametype_5v5_long(langID);
	case ANONGAME_TYPE_6V6:
	    return anongame_infos_DESC_get_gametype_6v6_long(langID);
	case ANONGAME_TYPE_2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2_long(langID);
	case ANONGAME_TYPE_3V3V3:
	    return anongame_infos_DESC_get_gametype_3v3v3_long(langID);
	case ANONGAME_TYPE_4V4V4:
	    return anongame_infos_DESC_get_gametype_4v4v4_long(langID);
	case ANONGAME_TYPE_2V2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2v2_long(langID);
	case ANONGAME_TYPE_3V3V3V3:
	    return anongame_infos_DESC_get_gametype_3v3v3v3_long(langID);
        case ANONGAME_TYPE_SMALL_FFA:
	    return anongame_infos_DESC_get_gametype_sffa_long(langID);
        case ANONGAME_TYPE_TEAM_FFA:
	    return anongame_infos_DESC_get_gametype_tffa_long(langID);
        case ANONGAME_TYPE_AT_2V2:
	    return anongame_infos_DESC_get_gametype_2v2_long(langID);
        case ANONGAME_TYPE_AT_3V3:
	    return anongame_infos_DESC_get_gametype_3v3_long(langID);
        case ANONGAME_TYPE_AT_4V4:
	    return anongame_infos_DESC_get_gametype_4v4_long(langID);
	case ANONGAME_TYPE_AT_2V2V2:
	    return anongame_infos_DESC_get_gametype_2v2v2_long(langID);
        case ANONGAME_TYPE_TY:
	    return tournament_get_sponsor();;
        default:
            eventlog(eventlog_level_error,__FUNCTION__, "invalid queue (%d)", queue);
    	    return NULL;
    }
}

/**********/
static int anongame_infos_THUMBSDOWN_set_PG_1v1(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_1v1 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_2v2(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_2v2 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_3v3(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_3v3 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_4v4(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_4v4 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_5v5(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_5v5 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_6v6(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_6v6 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_ffa(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_ffa = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_AT_ffa(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->AT_ffa = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_AT_2v2(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->AT_2v2 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_AT_3v3(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->AT_3v3 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_AT_4v4(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->AT_4v4 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_2v2v2(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_2v2v2 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_3v3v3(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_3v3v3 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_4v4v4(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_4v4v4 = value;

    return 0;
}


static int anongame_infos_THUMBSDOWN_set_AT_2v2v2(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->AT_2v2v2 = value;

    return 0;
}

static int anongame_infos_THUMBSDOWN_set_PG_2v2v2v2(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_2v2v2v2 = value;

    return 0;
}


static int anongame_infos_THUMBSDOWN_set_PG_3v3v3v3(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value)
{
    if (!anongame_infos_THUMBSDOWN)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_THUMBSDOWN");
	return -1;
    }

    anongame_infos_THUMBSDOWN->PG_3v3v3v3 = value;

    return 0;
}

/**********/
extern char anongame_infos_get_thumbsdown(int queue)
{
    switch(queue) {
	case ANONGAME_TYPE_1V1:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_1v1;
	case ANONGAME_TYPE_2V2:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_2v2;
	case ANONGAME_TYPE_3V3:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_3v3;
	case ANONGAME_TYPE_4V4:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_4v4;
	case ANONGAME_TYPE_5V5:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_5v5;
	case ANONGAME_TYPE_6V6:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_6v6;
	case ANONGAME_TYPE_2V2V2:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_2v2v2;
	case ANONGAME_TYPE_3V3V3:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_3v3v3;
	case ANONGAME_TYPE_4V4V4:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_4v4v4;
	case ANONGAME_TYPE_2V2V2V2:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_2v2v2v2;
	case ANONGAME_TYPE_3V3V3V3:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_3v3v3v3;
        case ANONGAME_TYPE_SMALL_FFA:
	    return anongame_infos->anongame_infos_THUMBSDOWN->PG_ffa;
        case ANONGAME_TYPE_TEAM_FFA:
	    return anongame_infos->anongame_infos_THUMBSDOWN->AT_ffa;
        case ANONGAME_TYPE_AT_2V2:
	    return anongame_infos->anongame_infos_THUMBSDOWN->AT_2v2;
        case ANONGAME_TYPE_AT_3V3:
	    return anongame_infos->anongame_infos_THUMBSDOWN->AT_3v3;
        case ANONGAME_TYPE_AT_4V4:
	    return anongame_infos->anongame_infos_THUMBSDOWN->AT_3v3;
	case ANONGAME_TYPE_AT_2V2V2:
	    return anongame_infos->anongame_infos_THUMBSDOWN->AT_2v2v2;
        case ANONGAME_TYPE_TY:
	    return tournament_get_thumbs_down();
        default:
                eventlog(eventlog_level_error,__FUNCTION__, "invalid queue (%d)", queue);
        return 1;
    }
}

/**********/

static int anongame_infos_ICON_REQ_WAR3_set_Level1(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3, int value)
{
    if (!anongame_infos_ICON_REQ_WAR3)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_WAR3");
	return -1;
    }

    anongame_infos_ICON_REQ_WAR3->Level1 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_WAR3_set_Level2(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3, int value)
{
    if (!anongame_infos_ICON_REQ_WAR3)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_WAR3");
	return -1;
    }

    anongame_infos_ICON_REQ_WAR3->Level2 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_WAR3_set_Level3(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3, int value)
{
    if (!anongame_infos_ICON_REQ_WAR3)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_WAR3");
	return -1;
    }

    anongame_infos_ICON_REQ_WAR3->Level3 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_WAR3_set_Level4(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3, int value)
{
    if (!anongame_infos_ICON_REQ_WAR3)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_WAR3");
	return -1;
    }

    anongame_infos_ICON_REQ_WAR3->Level4 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_W3XP_set_Level1(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value)
{
    if (!anongame_infos_ICON_REQ_W3XP)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
	return -1;
    }

    anongame_infos_ICON_REQ_W3XP->Level1 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_W3XP_set_Level2(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value)
{
    if (!anongame_infos_ICON_REQ_W3XP)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
	return -1;
    }

    anongame_infos_ICON_REQ_W3XP->Level2 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_W3XP_set_Level3(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value)
{
    if (!anongame_infos_ICON_REQ_W3XP)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
	return -1;
    }

    anongame_infos_ICON_REQ_W3XP->Level3 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_W3XP_set_Level4(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value)
{
    if (!anongame_infos_ICON_REQ_W3XP)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
	return -1;
    }

    anongame_infos_ICON_REQ_W3XP->Level4 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_W3XP_set_Level5(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value)
{
    if (!anongame_infos_ICON_REQ_W3XP)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_W3XP");
	return -1;
    }

    anongame_infos_ICON_REQ_W3XP->Level5 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_set_Level1(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value)
{
    if (!anongame_infos_ICON_REQ_TOURNEY)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
	return -1;
    }

    anongame_infos_ICON_REQ_TOURNEY->Level1 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_set_Level2(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value)
{
    if (!anongame_infos_ICON_REQ_TOURNEY)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
	return -1;
    }

    anongame_infos_ICON_REQ_TOURNEY->Level2 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_set_Level3(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value)
{
    if (!anongame_infos_ICON_REQ_TOURNEY)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
	return -1;
    }

    anongame_infos_ICON_REQ_TOURNEY->Level3 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_set_Level4(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value)
{
    if (!anongame_infos_ICON_REQ_TOURNEY)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
	return -1;
    }

    anongame_infos_ICON_REQ_TOURNEY->Level4 = value;

    return 0;
}

static int anongame_infos_ICON_REQ_TOURNEY_set_Level5(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value)
{
    if (!anongame_infos_ICON_REQ_TOURNEY)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame_infos_ICON_REQ_TOURNEY");
	return -1;
    }

    anongame_infos_ICON_REQ_TOURNEY->Level5 = value;

    return 0;
}

extern short anongame_infos_get_ICON_REQ_WAR3(int Level)
{
  switch(Level) 
    {
    case 0:
      return 0;
    case 1:
      return anongame_infos->anongame_infos_ICON_REQ_WAR3->Level1;
    case 2:
      return anongame_infos->anongame_infos_ICON_REQ_WAR3->Level2;
    case 3:
      return anongame_infos->anongame_infos_ICON_REQ_WAR3->Level3;
    case 4:
      return anongame_infos->anongame_infos_ICON_REQ_WAR3->Level4;
    default:
      eventlog(eventlog_level_error,__FUNCTION__, "invalid Level (%d)", Level);
      return -1;
    }
}

extern short anongame_infos_get_ICON_REQ_W3XP(int Level)
{
  switch(Level) 
    {
    case 0:
      return 0;
    case 1:
      return anongame_infos->anongame_infos_ICON_REQ_W3XP->Level1;
    case 2:
      return anongame_infos->anongame_infos_ICON_REQ_W3XP->Level2;
    case 3:
      return anongame_infos->anongame_infos_ICON_REQ_W3XP->Level3;
    case 4:
      return anongame_infos->anongame_infos_ICON_REQ_W3XP->Level4;
    case 5:
      return anongame_infos->anongame_infos_ICON_REQ_W3XP->Level5;
    default:
      eventlog(eventlog_level_error,__FUNCTION__, "invalid Level (%d)", Level);
      return -1;
    }
}

extern short anongame_infos_get_ICON_REQ_TOURNEY(int Level)
{
  switch(Level) 
    {
    case 0:
      return 0;
    case 1:
      return anongame_infos->anongame_infos_ICON_REQ_TOURNEY->Level1;
    case 2:
      return anongame_infos->anongame_infos_ICON_REQ_TOURNEY->Level2;
    case 3:
      return anongame_infos->anongame_infos_ICON_REQ_TOURNEY->Level3;
    case 4:
      return anongame_infos->anongame_infos_ICON_REQ_TOURNEY->Level4;
    case 5:
      return anongame_infos->anongame_infos_ICON_REQ_TOURNEY->Level5;
    default:
      eventlog(eventlog_level_error,__FUNCTION__, "invalid Level (%d)", Level);
      return -1;
    }
}

/**********/

extern char * anongame_infos_data_get_url(char const * clienttag, int versionid, int * len)
{
	if(strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_war3->url_len;
			return anongame_infos->anongame_infos_data_war3->url_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_war3->url_comp_len;
			return anongame_infos->anongame_infos_data_war3->url_comp_data;
		}
	}
	else
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->url_len;
			return anongame_infos->anongame_infos_data_w3xp->url_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->url_comp_len;
			return anongame_infos->anongame_infos_data_w3xp->url_comp_data;
		}
	}
}

extern char * anongame_infos_data_get_map(char const * clienttag, int versionid, int * len)
{
	if(strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_war3->map_len;
			return anongame_infos->anongame_infos_data_war3->map_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_war3->map_comp_len;
			return anongame_infos->anongame_infos_data_war3->map_comp_data;
		}
	}
	else
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->map_len;
			return anongame_infos->anongame_infos_data_w3xp->map_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->map_comp_len;
			return anongame_infos->anongame_infos_data_w3xp->map_comp_data;
		}
	}
}

extern char * anongame_infos_data_get_type(char const * clienttag, int versionid, int * len)
{
	if(strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_war3->type_len;
			return anongame_infos->anongame_infos_data_war3->type_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_war3->type_comp_len;
			return anongame_infos->anongame_infos_data_war3->type_comp_data;
		}
	}
	else
	{
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->type_len;
			return anongame_infos->anongame_infos_data_w3xp->type_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->type_comp_len;
			return anongame_infos->anongame_infos_data_w3xp->type_comp_data;
		}
	}
}

extern char * anongame_infos_data_get_desc(char const * langID, char const * clienttag, int versionid, int * len)
{
	t_elem * curr;
	t_anongame_infos_data_lang * entry;
	if(strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	{
		if(langID != NULL)
		{
			LIST_TRAVERSE(anongame_infos->anongame_infos_data_lang_war3, curr)
			{
				if((entry = elem_get_data(curr)) && strcmp(entry->langID, langID) == 0)
				{
					if(versionid < 0x0000000D)
					{
						(* len) = entry->desc_len;
						return entry->desc_data;
					}
					else
					{
						(* len) = entry->desc_comp_len;
						return entry->desc_comp_data;
					}
				}
			}
		}
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_war3->desc_len;
			return anongame_infos->anongame_infos_data_war3->desc_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_war3->desc_comp_len;
			return anongame_infos->anongame_infos_data_war3->desc_comp_data;
		}
	}
	else
	{
		if(langID != NULL)
		{
			LIST_TRAVERSE(anongame_infos->anongame_infos_data_lang_w3xp, curr)
			{
				if((entry = elem_get_data(curr)) && strcmp(entry->langID, langID) == 0)
				{
					if(versionid < 0x0000000D)
					{
						(* len) = entry->desc_len;
						return entry->desc_data;
					}
					else
					{
						(* len) = entry->desc_comp_len;
						return entry->desc_comp_data;
					}
				}
			}
		}
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->desc_len;
			return anongame_infos->anongame_infos_data_w3xp->desc_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->desc_comp_len;
			return anongame_infos->anongame_infos_data_w3xp->desc_comp_data;
		}
	}
}

extern char * anongame_infos_data_get_ladr(char const * langID, char const * clienttag, int versionid, int * len)
{
	t_elem * curr;
	t_anongame_infos_data_lang * entry;
	if(strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	{
		if(langID != NULL)
		{
			LIST_TRAVERSE(anongame_infos->anongame_infos_data_lang_war3, curr)
			{
				if((entry = elem_get_data(curr)) && strcmp(entry->langID, langID) == 0)
				{
					if(versionid < 0x0000000D)
					{
						(* len) = entry->ladr_len;
						return entry->ladr_data;
					}
					else
					{
						(* len) = entry->ladr_comp_len;
						return entry->ladr_comp_data;
					}
				}
			}
		}
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_war3->ladr_len;
			return anongame_infos->anongame_infos_data_war3->ladr_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_war3->ladr_comp_len;
			return anongame_infos->anongame_infos_data_war3->ladr_comp_data;
		}
	}
	else
	{
		if(langID != NULL)
		{
			LIST_TRAVERSE(anongame_infos->anongame_infos_data_lang_w3xp, curr)
			{
				if((entry = elem_get_data(curr)) && strcmp(entry->langID, langID) == 0)
				{
					if(versionid < 0x0000000D)
					{
						(* len) = entry->ladr_len;
						return entry->ladr_data;
					}
					else
					{
						(* len) = entry->ladr_comp_len;
						return entry->ladr_comp_data;
					}
				}
			}
		}
		if(versionid < 0x0000000D)
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->ladr_len;
			return anongame_infos->anongame_infos_data_w3xp->ladr_data;
		}
		else
		{
			(* len) = anongame_infos->anongame_infos_data_w3xp->ladr_comp_len;
			return anongame_infos->anongame_infos_data_w3xp->ladr_comp_data;
		}
	}
}

/**********/

static void anongame_infos_set_defaults(t_anongame_infos * anongame_infos)
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
    if (!(anongame_infos_DESC->gametype_sffa_short))
	anongame_infos_DESC_set_gametype_sffa_short(anongame_infos_DESC,PVPGN_SFFA_GT_DESC);
    if (!(anongame_infos_DESC->gametype_sffa_long))
	anongame_infos_DESC_set_gametype_sffa_long(anongame_infos_DESC,PVPGN_SFFA_GT_LONG);
    if (!(anongame_infos_DESC->gametype_tffa_short))
	anongame_infos_DESC_set_gametype_tffa_short(anongame_infos_DESC,PVPGN_TFFA_GT_DESC);
    if (!(anongame_infos_DESC->gametype_tffa_long))
	anongame_infos_DESC_set_gametype_tffa_long(anongame_infos_DESC,PVPGN_TFFA_GT_LONG);
    if (!(anongame_infos_DESC->gametype_2v2v2_short))
	anongame_infos_DESC_set_gametype_2v2v2_short(anongame_infos_DESC,PVPGN_2V2V2_GT_DESC);
    if (!(anongame_infos_DESC->gametype_2v2v2_long))
	anongame_infos_DESC_set_gametype_2v2v2_long(anongame_infos_DESC,PVPGN_2V2V2_GT_LONG);
    if (!(anongame_infos_DESC->gametype_3v3v3_short))
	anongame_infos_DESC_set_gametype_3v3v3_short(anongame_infos_DESC,PVPGN_3V3V3_GT_DESC);
    if (!(anongame_infos_DESC->gametype_3v3v3_long))
	anongame_infos_DESC_set_gametype_3v3v3_long(anongame_infos_DESC,PVPGN_3V3V3_GT_LONG);
    if (!(anongame_infos_DESC->gametype_4v4v4_short))
	anongame_infos_DESC_set_gametype_4v4v4_short(anongame_infos_DESC,PVPGN_4V4V4_GT_DESC);
    if (!(anongame_infos_DESC->gametype_4v4v4_long))
	anongame_infos_DESC_set_gametype_4v4v4_long(anongame_infos_DESC,PVPGN_4V4V4_GT_LONG);
    if (!(anongame_infos_DESC->gametype_2v2v2v2_short))
	anongame_infos_DESC_set_gametype_2v2v2v2_short(anongame_infos_DESC,PVPGN_2V2V2V2_GT_DESC);
    if (!(anongame_infos_DESC->gametype_2v2v2v2_long))
	anongame_infos_DESC_set_gametype_2v2v2v2_long(anongame_infos_DESC,PVPGN_2V2V2V2_GT_LONG);
    if (!(anongame_infos_DESC->gametype_3v3v3v3_short))
	anongame_infos_DESC_set_gametype_3v3v3v3_short(anongame_infos_DESC,PVPGN_3V3V3V3_GT_DESC);
    if (!(anongame_infos_DESC->gametype_3v3v3v3_long))
	anongame_infos_DESC_set_gametype_3v3v3v3_long(anongame_infos_DESC,PVPGN_3V3V3V3_GT_LONG);
    if (!(anongame_infos_DESC->gametype_5v5_short))
	anongame_infos_DESC_set_gametype_5v5_short(anongame_infos_DESC,PVPGN_5V5_GT_DESC);
    if (!(anongame_infos_DESC->gametype_5v5_long))
	anongame_infos_DESC_set_gametype_5v5_long(anongame_infos_DESC,PVPGN_5V5_GT_LONG);
    if (!(anongame_infos_DESC->gametype_6v6_short))
	anongame_infos_DESC_set_gametype_6v6_short(anongame_infos_DESC,PVPGN_6V6_GT_DESC);
    if (!(anongame_infos_DESC->gametype_6v6_long))
	anongame_infos_DESC_set_gametype_6v6_long(anongame_infos_DESC,PVPGN_6V6_GT_LONG);
	

}

typedef int (* t_URL_string_handler)(t_anongame_infos_URL * anongame_infos_URL, char * text);

typedef struct {
	const char				* anongame_infos_URL_string;
	t_URL_string_handler			URL_string_handler;
} t_anongame_infos_URL_table_row;

typedef int (* t_DESC_string_handler)(t_anongame_infos_DESC * anongame_infos_DESC, char * text);

typedef struct {
	const char				* anongame_infos_DESC_string;
	t_DESC_string_handler			DESC_string_handler;
} t_anongame_infos_DESC_table_row;

typedef int (* t_THUMBSDOWN_string_handler)(t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN, char value);

typedef struct {
	const char				* anongame_infos_THUMBSDOWN_string;
	t_THUMBSDOWN_string_handler		THUMBSDOWN_string_handler;
} t_anongame_infos_THUMBSDOWN_table_row;

typedef int (* t_ICON_REQ_WAR3_string_handler)(t_anongame_infos_ICON_REQ_WAR3 * anongame_infos_ICON_REQ_WAR3, int value);


typedef struct {
	const char				* anongame_infos_ICON_REQ_WAR3_string;
	t_ICON_REQ_WAR3_string_handler		ICON_REQ_WAR3_string_handler;
} t_anongame_infos_ICON_REQ_WAR3_table_row;

typedef int (* t_ICON_REQ_W3XP_string_handler)(t_anongame_infos_ICON_REQ_W3XP * anongame_infos_ICON_REQ_W3XP, int value);

typedef struct {
	const char				* anongame_infos_ICON_REQ_W3XP_string;
	t_ICON_REQ_W3XP_string_handler		ICON_REQ_W3XP_string_handler;
} t_anongame_infos_ICON_REQ_W3XP_table_row;

typedef int (* t_ICON_REQ_TOURNEY_string_handler)(t_anongame_infos_ICON_REQ_TOURNEY * anongame_infos_ICON_REQ_TOURNEY, int value);

typedef struct {
	const char				* anongame_infos_ICON_REQ_TOURNEY_string;
	t_ICON_REQ_TOURNEY_string_handler		ICON_REQ_TOURNEY_string_handler;
} t_anongame_infos_ICON_REQ_TOURNEY_table_row;


static const t_anongame_infos_URL_table_row URL_handler_table[] = 
{
	{ "server_URL",			anongame_infos_URL_set_server_URL },
	{ "player_URL",			anongame_infos_URL_set_player_URL },
	{ "tourney_URL",		anongame_infos_URL_set_tourney_URL },
	{ "ladder_PG_1v1_URL",		anongame_infos_URL_set_ladder_PG_1v1_URL },
	{ "ladder_PG_ffa_URL",		anongame_infos_URL_set_ladder_PG_ffa_URL },
	{ "ladder_PG_team_URL",		anongame_infos_URL_set_ladder_PG_team_URL },
	{ "ladder_AT_2v2_URL",		anongame_infos_URL_set_ladder_AT_2v2_URL },
	{ "ladder_AT_3v3_URL",		anongame_infos_URL_set_ladder_AT_3v3_URL },
	{ "ladder_AT_4v4_URL",		anongame_infos_URL_set_ladder_AT_4v4_URL },
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
	{ "gametype_sffa_short",	anongame_infos_DESC_set_gametype_sffa_short },
	{ "gametype_sffa_long",		anongame_infos_DESC_set_gametype_sffa_long },
	{ "gametype_tffa_short",	anongame_infos_DESC_set_gametype_tffa_short },
	{ "gametype_tffa_long",		anongame_infos_DESC_set_gametype_tffa_long },
	{ "gametype_2v2v2_short",	anongame_infos_DESC_set_gametype_2v2v2_short },
	{ "gametype_2v2v2_long",	anongame_infos_DESC_set_gametype_2v2v2_long },
	{ "gametype_3v3v3_short",	anongame_infos_DESC_set_gametype_3v3v3_short },
	{ "gametype_3v3v3_long",	anongame_infos_DESC_set_gametype_3v3v3_long },
	{ "gametype_4v4v4_short",	anongame_infos_DESC_set_gametype_4v4v4_short },
	{ "gametype_4v4v4_long",	anongame_infos_DESC_set_gametype_4v4v4_long },
	{ "gametype_2v2v2v2_short",	anongame_infos_DESC_set_gametype_2v2v2v2_short },
	{ "gametype_2v2v2v2_long",	anongame_infos_DESC_set_gametype_2v2v2v2_long },
	{ "gametype_3v3v3v3_short",	anongame_infos_DESC_set_gametype_3v3v3v3_short },
	{ "gametype_3v3v3v3_long",	anongame_infos_DESC_set_gametype_3v3v3v3_long },
	{ "gametype_5v5_short",		anongame_infos_DESC_set_gametype_5v5_short },
	{ "gametype_5v5_long",		anongame_infos_DESC_set_gametype_5v5_long },
	{ "gametype_6v6_short",		anongame_infos_DESC_set_gametype_6v6_short },
	{ "gametype_6v6_long",		anongame_infos_DESC_set_gametype_6v6_long },
	
	{ NULL, NULL }
};

static const t_anongame_infos_THUMBSDOWN_table_row THUMBSDOWN_handler_table[] =
{
	{ "PG_1v1",			anongame_infos_THUMBSDOWN_set_PG_1v1 },
	{ "PG_2v2",			anongame_infos_THUMBSDOWN_set_PG_2v2 },
	{ "PG_3v3",			anongame_infos_THUMBSDOWN_set_PG_3v3 },
	{ "PG_4v4",			anongame_infos_THUMBSDOWN_set_PG_4v4 },
	{ "PG_ffa",			anongame_infos_THUMBSDOWN_set_PG_ffa },
	{ "AT_2v2",			anongame_infos_THUMBSDOWN_set_AT_2v2 },
	{ "AT_3v3",			anongame_infos_THUMBSDOWN_set_AT_3v3 },
	{ "AT_4v4",			anongame_infos_THUMBSDOWN_set_AT_4v4 },
	{ "AT_ffa",			anongame_infos_THUMBSDOWN_set_AT_ffa },	
	{ "PG_5v5",			anongame_infos_THUMBSDOWN_set_PG_5v5 },
	{ "PG_6v6",			anongame_infos_THUMBSDOWN_set_PG_6v6 },		
	{ "PG_2v2v2",			anongame_infos_THUMBSDOWN_set_PG_2v2v2 },
	{ "PG_3v3v3",			anongame_infos_THUMBSDOWN_set_PG_3v3v3 },
	{ "PG_4v4v4",			anongame_infos_THUMBSDOWN_set_PG_4v4v4 },
	{ "PG_2v2v2v2",			anongame_infos_THUMBSDOWN_set_PG_2v2v2v2 },
	{ "PG_3v3v3v3",			anongame_infos_THUMBSDOWN_set_PG_3v3v3v3 },
	{ "AT_2v2v2",			anongame_infos_THUMBSDOWN_set_AT_2v2v2 },			

	{ NULL, NULL }
};

static const t_anongame_infos_ICON_REQ_WAR3_table_row ICON_REQ_WAR3_handler_table[] =
{
        { "Level1",                     anongame_infos_ICON_REQ_WAR3_set_Level1 },
        { "Level2",                     anongame_infos_ICON_REQ_WAR3_set_Level2 },
        { "Level3",                     anongame_infos_ICON_REQ_WAR3_set_Level3 },
        { "Level4",                     anongame_infos_ICON_REQ_WAR3_set_Level4 },
	{ NULL, NULL }
};

static const t_anongame_infos_ICON_REQ_W3XP_table_row ICON_REQ_W3XP_handler_table[] =
{
        { "Level1",                     anongame_infos_ICON_REQ_W3XP_set_Level1 },
        { "Level2",                     anongame_infos_ICON_REQ_W3XP_set_Level2 },
        { "Level3",                     anongame_infos_ICON_REQ_W3XP_set_Level3 },
        { "Level4",                     anongame_infos_ICON_REQ_W3XP_set_Level4 },
	{ "Level5",                     anongame_infos_ICON_REQ_W3XP_set_Level5 },
	{ NULL, NULL }
};

static const t_anongame_infos_ICON_REQ_TOURNEY_table_row ICON_REQ_TOURNEY_handler_table[] =
{
        { "Level1",                     anongame_infos_ICON_REQ_TOURNEY_set_Level1 },
        { "Level2",                     anongame_infos_ICON_REQ_TOURNEY_set_Level2 },
        { "Level3",                     anongame_infos_ICON_REQ_TOURNEY_set_Level3 },
        { "Level4",                     anongame_infos_ICON_REQ_TOURNEY_set_Level4 },
	{ "Level5",                     anongame_infos_ICON_REQ_TOURNEY_set_Level5 },
	{ NULL, NULL }
};

typedef enum
{
	parse_UNKNOWN,
	parse_URL,
	parse_DESC,
	parse_THUMBSDOWN,
	parse_ICON_REQ_WAR3,
	parse_ICON_REQ_W3XP,
	parse_ICON_REQ_TOURNEY
} t_parse_mode;

typedef enum
{
	changed,
	unchanged
} t_parse_state;

static t_parse_mode switch_parse_mode(char * text, char * langID)
{
	if (!(text)) return parse_UNKNOWN;
	else if (strcmp(text,"[URL]")==0) return parse_URL;
	else if (strcmp(text,"[THUMBS_DOWN_LIMIT]")==0) return parse_THUMBSDOWN;
	else if (strcmp(text,"[ICON_REQUIRED_RACE_WINS_WAR3]")==0) return parse_ICON_REQ_WAR3;
	else if (strcmp(text,"[ICON_REQUIRED_RACE_WINS_W3XP]")==0) return parse_ICON_REQ_W3XP;
	else if (strcmp(text,"[ICON_REQUIRED_TOURNEY_WINS]")==0) return parse_ICON_REQ_TOURNEY;
	else if (strcmp(text,"[DEFAULT_DESC]")==0) 
	{
		langID[0] = '\0';
		return parse_DESC;
	}
	else if (strlen(text)==6) 
	{
		strncpy(langID,&(text[1]),4);
		langID[4] = '\0';
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
    char *				buff;
    char *				temp;
    char 				langID[4]			= "    ";
    t_parse_mode			parse_mode			= parse_UNKNOWN;
    t_parse_state			parse_state			= unchanged;
    t_anongame_infos_DESC *		anongame_infos_DESC		= NULL;
    char *				pointer;
    char *				variable;
    char *				value				= NULL;
    t_anongame_infos_DESC_table_row	const *		DESC_table_row;
    t_anongame_infos_URL_table_row	const *		URL_table_row;
    t_anongame_infos_THUMBSDOWN_table_row const *	THUMBSDOWN_table_row;
    t_anongame_infos_ICON_REQ_WAR3_table_row const *    ICON_REQ_WAR3_table_row;
    t_anongame_infos_ICON_REQ_W3XP_table_row const *    ICON_REQ_W3XP_table_row;
    t_anongame_infos_ICON_REQ_TOURNEY_table_row const * ICON_REQ_TOURNEY_table_row;
    int					int_value;
    char				char_value;
    
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
	
	if ((buff[0]=='[') && (buff[strlen(buff)-1]==']'))
	  {
	    if ((parse_state == unchanged) && (anongame_infos_DESC != NULL))
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
	else if (buff[0]!='\0')
	  switch(parse_mode) 
	    {
	    case parse_UNKNOWN:
	      {
		if ((buff[0]!='[') || (buff[strlen(buff)-1]!=']'))
		  {
		    eventlog(eventlog_level_error,__FUNCTION__,"expected [] section start, but found %s on line %u",buff,line);
		  }
		else
		  {
		    parse_mode = switch_parse_mode(buff,langID);
		    parse_state = changed;
		  }
		break;
	      }
	    case parse_URL:
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
		  if (strcmp(URL_table_row->anongame_infos_URL_string, variable)==0) 
		    {
		      if (URL_table_row->URL_string_handler != NULL) URL_table_row->URL_string_handler(anongame_infos->anongame_infos_URL,value);
		    }
		
		break;
	      }
	    case parse_DESC:
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
		  if (strcmp(DESC_table_row->anongame_infos_DESC_string, variable)==0) 
		    {
		      if (DESC_table_row->DESC_string_handler != NULL) DESC_table_row->DESC_string_handler(anongame_infos_DESC,value);
		    }
		break;
	      }
	    case parse_THUMBSDOWN:
	      {
		parse_state = unchanged;
		variable = buff;
		pointer = strchr(variable,'=');
		for(pointer--;pointer[0]==' ';pointer--);
		pointer[1]='\0';
		pointer++;
		pointer++;
		pointer = strchr(pointer,'=');
		pointer++;
		int_value = atoi(pointer);
		if (int_value<0) int_value=0;
		if (int_value>127) int_value=127;
		char_value = (char)int_value;
		
		for(THUMBSDOWN_table_row = THUMBSDOWN_handler_table; THUMBSDOWN_table_row->anongame_infos_THUMBSDOWN_string != NULL; THUMBSDOWN_table_row++)
		  if (strcmp(THUMBSDOWN_table_row->anongame_infos_THUMBSDOWN_string, variable)==0) 
		    {
		      if (THUMBSDOWN_table_row->THUMBSDOWN_string_handler != NULL) THUMBSDOWN_table_row->THUMBSDOWN_string_handler(anongame_infos->anongame_infos_THUMBSDOWN,char_value);
		    }
		break;
	      }
	    case parse_ICON_REQ_WAR3:
	      {
		parse_state = unchanged;
		variable = buff;
		pointer = strchr(variable,'=');
		for(pointer--;pointer[0]==' ';pointer--);
		pointer[1]='\0';
		pointer++;
		pointer++;
		pointer = strchr(pointer,'=');
		pointer++;
		int_value = atoi(pointer);
		if (int_value<0) int_value=0;

		for(ICON_REQ_WAR3_table_row = ICON_REQ_WAR3_handler_table; ICON_REQ_WAR3_table_row->anongame_infos_ICON_REQ_WAR3_string != NULL; ICON_REQ_WAR3_table_row++)
		  if (strcmp(ICON_REQ_WAR3_table_row->anongame_infos_ICON_REQ_WAR3_string, variable)==0) 
		    {
		      if (ICON_REQ_WAR3_table_row->ICON_REQ_WAR3_string_handler != NULL) ICON_REQ_WAR3_table_row->ICON_REQ_WAR3_string_handler(anongame_infos->anongame_infos_ICON_REQ_WAR3,int_value);
		    }
		break;
	      }
	    case parse_ICON_REQ_W3XP:
	      {
		parse_state = unchanged;
		variable = buff;
		pointer = strchr(variable,'=');
		for(pointer--;pointer[0]==' ';pointer--);
		pointer[1]='\0';
		pointer++;
		pointer++;
		pointer = strchr(pointer,'=');
		pointer++;
		int_value = atoi(pointer);
		if (int_value<0) int_value=0;
		
		for(ICON_REQ_W3XP_table_row = ICON_REQ_W3XP_handler_table; ICON_REQ_W3XP_table_row->anongame_infos_ICON_REQ_W3XP_string != NULL; ICON_REQ_W3XP_table_row++)
		  if (strcmp(ICON_REQ_W3XP_table_row->anongame_infos_ICON_REQ_W3XP_string, variable)==0) 
		    {
		      if (ICON_REQ_W3XP_table_row->ICON_REQ_W3XP_string_handler != NULL) ICON_REQ_W3XP_table_row->ICON_REQ_W3XP_string_handler(anongame_infos->anongame_infos_ICON_REQ_W3XP,int_value);
		    }
		break;
	      }
	    case parse_ICON_REQ_TOURNEY:
	      {
		parse_state = unchanged;
		variable = buff;
		pointer = strchr(variable,'=');
		for(pointer--;pointer[0]==' ';pointer--);
		pointer[1]='\0';
		pointer++;
		pointer++;
		pointer = strchr(pointer,'=');
		pointer++;
		int_value = atoi(pointer);
		if (int_value<0) int_value=0;
		
		for(ICON_REQ_TOURNEY_table_row = ICON_REQ_TOURNEY_handler_table; ICON_REQ_TOURNEY_table_row->anongame_infos_ICON_REQ_TOURNEY_string != NULL; ICON_REQ_TOURNEY_table_row++)
		  if (strcmp(ICON_REQ_TOURNEY_table_row->anongame_infos_ICON_REQ_TOURNEY_string, variable)==0) 
		    {
		      if (ICON_REQ_TOURNEY_table_row->ICON_REQ_TOURNEY_string_handler != NULL) ICON_REQ_TOURNEY_table_row->ICON_REQ_TOURNEY_string_handler(anongame_infos->anongame_infos_ICON_REQ_TOURNEY,int_value);
		    }
		break;
	      }
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
	anongame_infos_data_load();
    return 0;
}

static int anongame_infos_data_load(void)
{
	t_elem * curr;
	t_packet * raw;
	int j, k;
	char ladr_count		= 0;
	char desc_count;
	char mapscount_total;
	char value;
	char PG_gamestyles;
	char AT_gamestyles;
	char TY_gamestyles;

	char anongame_prefix[ANONGAME_TYPES][5] = {		/* queue */
	/* PG 1v1	*/	{0x00, 0x00, 0x03, 0x3F, 0x00},	/*  0	*/
	/* PG 2v2	*/	{0x01, 0x00, 0x02, 0x3F, 0x00},	/*  1	*/
	/* PG 3v3	*/	{0x02, 0x00, 0x01, 0x3F, 0x00},	/*  2	*/
	/* PG 4v4	*/	{0x03, 0x00, 0x01, 0x3F, 0x00},	/*  3	*/
	/* PG sffa	*/	{0x04, 0x00, 0x02, 0x3F, 0x00},	/*  4	*/

	/* AT 2v2	*/	{0x00, 0x00, 0x02, 0x3F, 0x02},	/*  5	*/
	/* AT tffa	*/	{0x01, 0x00, 0x02, 0x3F, 0x02},	/*  6	*/
	/* AT 3v3	*/	{0x02, 0x00, 0x02, 0x3F, 0x03},	/*  7	*/
	/* AT 4v4	*/	{0x03, 0x00, 0x02, 0x3F, 0x04},	/*  8	*/

	/* TY		*/	{0x00, 0x01, 0x00, 0x3F, 0x00},	/*  9	*/
	/* PG 5v5	*/	{0x05, 0x00, 0x01, 0x3F, 0x00},	/* 10	*/
	/* PG 6v6	*/	{0x06, 0x00, 0x01, 0x3F, 0x00},	/* 11	*/
	/* PG 2v2v2	*/	{0x07, 0x00, 0x01, 0x3F, 0x00},	/* 12	*/
	/* PG 3v3v3	*/	{0x08, 0x00, 0x01, 0x3F, 0x00},	/* 13	*/
	/* PG 4v4v4	*/	{0x09, 0x00, 0x01, 0x3F, 0x00},	/* 14	*/
	/* PG 2v2v2v2	*/	{0x0A, 0x00, 0x01, 0x3F, 0x00},	/* 15	*/
	/* PG 3v3v3v3	*/	{0x0B, 0x00, 0x01, 0x3F, 0x00},	/* 16	*/
	/* AT 2v2v2	*/	{0x04, 0x00, 0x02, 0x3F, 0x02}	/* 17	*/
	};

	/* hack to give names for new gametypes untill there added to anongame_infos.c */
	char * anongame_gametype_names[ANONGAME_TYPES] = {
	    "One vs. One",
		"Two vs. Two",
	    "Three vs. Three",
	    "Four vs. Four",
		"Small Free for All",
	    "Two vs. Two",
	    "Team Free for All",
		"Three vs. Three",
	    "Four vs. Four",
	    "Tournament Game",
		"Five vs. Five",
	    "Six Vs. Six",
	    "Two vs. Two vs. Two",
		"3 vs. 3 vs. 3",
	    "4 vs. 4 vs. 4",
	    "2 vs. 2 vs. 2 vs. 2",
		"3 vs. 3 vs. 3 vs. 3",
	    "Two vs. Two vs. Two"
	};

	char const * game_clienttag[2] = { CLIENTTAG_WARCRAFT3, CLIENTTAG_WAR3XP };
	
	char anongame_PG_section	= 0x00;
	char anongame_AT_section	= 0x01;
	char anongame_TY_section	= 0x02;
	
	/* set thumbsdown from the conf file */
	for (j=0; j < ANONGAME_TYPES; j++)
		anongame_prefix[j][2] = anongame_infos_get_thumbsdown(j);

	if((raw = packet_create(packet_class_raw)) != NULL)
	{
	    packet_append_string(raw, anongame_infos_URL_get_server_url());
	    packet_append_string(raw, anongame_infos_URL_get_player_url());
	    packet_append_string(raw, anongame_infos_URL_get_tourney_url());
		anongame_infos->anongame_infos_data_war3->url_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_war3->url_data = (char *)malloc(anongame_infos->anongame_infos_data_war3->url_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_war3->url_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_war3->url_len), anongame_infos->anongame_infos_data_war3->url_len);
			zlib_compress(anongame_infos->anongame_infos_data_war3->url_data, anongame_infos->anongame_infos_data_war3->url_len, &anongame_infos->anongame_infos_data_war3->url_comp_data, &anongame_infos->anongame_infos_data_war3->url_comp_len);
		}
		anongame_infos->anongame_infos_data_w3xp->url_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_w3xp->url_data = (char *)malloc(anongame_infos->anongame_infos_data_w3xp->url_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_w3xp->url_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_w3xp->url_len), anongame_infos->anongame_infos_data_w3xp->url_len);
			zlib_compress(anongame_infos->anongame_infos_data_w3xp->url_data, anongame_infos->anongame_infos_data_w3xp->url_len, &anongame_infos->anongame_infos_data_w3xp->url_comp_data, &anongame_infos->anongame_infos_data_w3xp->url_comp_len);
		}


		for(k = 0; k < 2; k++)
		{
		packet_set_size(raw, 0);
	    mapscount_total = maplists_get_totalmaps(game_clienttag[k]);
	    packet_append_data(raw, &mapscount_total, 1);
	    maplists_add_maps_to_packet(raw, game_clienttag[k]);
		if(k == 0)
		{
		anongame_infos->anongame_infos_data_war3->map_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_war3->map_data = (char *)malloc(anongame_infos->anongame_infos_data_war3->map_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_war3->map_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_war3->map_len), anongame_infos->anongame_infos_data_war3->map_len);
			zlib_compress(anongame_infos->anongame_infos_data_war3->map_data, anongame_infos->anongame_infos_data_war3->map_len, &anongame_infos->anongame_infos_data_war3->map_comp_data, &anongame_infos->anongame_infos_data_war3->map_comp_len);
		}
		}
		else
		{
		anongame_infos->anongame_infos_data_w3xp->map_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_w3xp->map_data = (char *)malloc(anongame_infos->anongame_infos_data_w3xp->map_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_w3xp->map_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_w3xp->map_len), anongame_infos->anongame_infos_data_w3xp->map_len);
			zlib_compress(anongame_infos->anongame_infos_data_w3xp->map_data, anongame_infos->anongame_infos_data_w3xp->map_len, &anongame_infos->anongame_infos_data_w3xp->map_comp_data, &anongame_infos->anongame_infos_data_w3xp->map_comp_len);
		}
		}
		}


		for(k = 0; k < 2; k++)
		{
		packet_set_size(raw, 0);
		value = 0;
		PG_gamestyles = 0;
		AT_gamestyles = 0;
		TY_gamestyles = 0;
	    /* count of gametypes (PG, AT, TY) */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (maplists_get_totalmaps_by_queue(game_clienttag[k], j)) {
		    if (!anongame_prefix[j][1] && !anongame_prefix[j][4])
			PG_gamestyles++;
		    if (!anongame_prefix[j][1] && anongame_prefix[j][4])
			AT_gamestyles++;
		    if (anongame_prefix[j][1])
			TY_gamestyles++;
		}
		    
	    if (PG_gamestyles)
		value++;
	    if (AT_gamestyles)
		value++;
	    if (TY_gamestyles)
		value++;
		    
	    packet_append_data(raw, &value, 1);
		    
	    /* PG */
	    if (PG_gamestyles) {
		packet_append_data(raw, &anongame_PG_section,1);
		packet_append_data(raw, &PG_gamestyles,1);
		for (j=0; j < ANONGAME_TYPES; j++)
		    if (!anongame_prefix[j][1] && !anongame_prefix[j][4] &&
			    maplists_get_totalmaps_by_queue(game_clienttag[k],j))
		    {
			packet_append_data(raw, &anongame_prefix[j], 5);
			maplists_add_map_info_to_packet(raw, game_clienttag[k], j);
		    }
	    }
		    
	    /* AT */
	    if (AT_gamestyles) {
		packet_append_data(raw,&anongame_AT_section,1);
		packet_append_data(raw,&AT_gamestyles,1);
		for (j=0; j < ANONGAME_TYPES; j++)
		    if (!anongame_prefix[j][1] && anongame_prefix[j][4] &&
			    maplists_get_totalmaps_by_queue(game_clienttag[k],j))
		    {
			packet_append_data(raw, &anongame_prefix[j], 5);
			maplists_add_map_info_to_packet(raw, game_clienttag[k], j);
		    }
	    }
		    
	    /* TY */
	    if (TY_gamestyles) {
		packet_append_data(raw, &anongame_TY_section,1);
		packet_append_data(raw, &TY_gamestyles,1);
		for (j=0; j < ANONGAME_TYPES; j++)
		    if (anongame_prefix[j][1] &&
			    maplists_get_totalmaps_by_queue(game_clienttag[k],j))
		    {
			/* set tournament races available */
			anongame_prefix[j][3] = tournament_get_races();
			/* set tournament type (PG or AT)
			 * PG = 0
			 * AT = number players per team */
			if (tournament_is_arranged())
			    anongame_prefix[j][4] = tournament_get_game_type();
			else
			    anongame_prefix[j][4] = 0;
			
			packet_append_data(raw, &anongame_prefix[j], 5);
			maplists_add_map_info_to_packet(raw, game_clienttag[k], j);
		    }
	    }
		if(k == 0)
		{
		anongame_infos->anongame_infos_data_war3->type_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_war3->type_data = (char *)malloc(anongame_infos->anongame_infos_data_war3->type_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_war3->type_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_war3->type_len), anongame_infos->anongame_infos_data_war3->type_len);
			zlib_compress(anongame_infos->anongame_infos_data_war3->type_data, anongame_infos->anongame_infos_data_war3->type_len, &anongame_infos->anongame_infos_data_war3->type_comp_data, &anongame_infos->anongame_infos_data_war3->type_comp_len);
		}
		}
		else
		{
		anongame_infos->anongame_infos_data_w3xp->type_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_w3xp->type_data = (char *)malloc(anongame_infos->anongame_infos_data_w3xp->type_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_w3xp->type_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_w3xp->type_len), anongame_infos->anongame_infos_data_w3xp->type_len);
			zlib_compress(anongame_infos->anongame_infos_data_w3xp->type_data, anongame_infos->anongame_infos_data_w3xp->type_len, &anongame_infos->anongame_infos_data_w3xp->type_comp_data, &anongame_infos->anongame_infos_data_w3xp->type_comp_len);
		}
		}
		}

			
		for(k = 0; k < 2; k++)
		{
		desc_count = 0;
		packet_set_size(raw, 0);
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		    desc_count++;
	    packet_append_data(raw,&desc_count,1);
	    /* PG description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (!anongame_prefix[j][1] && !anongame_prefix[j][4] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_PG_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
		    
		    if (anongame_infos_get_short_desc(NULL, j) == NULL)
			packet_append_string(raw,anongame_gametype_names[j]);
		    else
			packet_append_string(raw,anongame_infos_get_short_desc(NULL, j));
		    
		    if (anongame_infos_get_long_desc(NULL, j) == NULL)
			packet_append_string(raw,"No Descreption");
		    else
			packet_append_string(raw,anongame_infos_get_long_desc(NULL, j));
		}
	    /* AT description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (!anongame_prefix[j][1] && anongame_prefix[j][4] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_AT_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
		    packet_append_string(raw,anongame_infos_get_short_desc(NULL, j));
		    packet_append_string(raw,anongame_infos_get_long_desc(NULL, j));
		}
	    /* TY description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (anongame_prefix[j][1] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_TY_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
		    packet_append_string(raw,anongame_infos_get_short_desc(NULL, j));
		    packet_append_string(raw,anongame_infos_get_long_desc(NULL, j));
		}
		if(k == 0)
		{
		anongame_infos->anongame_infos_data_war3->desc_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_war3->desc_data = (char *)malloc(anongame_infos->anongame_infos_data_war3->desc_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_war3->desc_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_war3->desc_len), anongame_infos->anongame_infos_data_war3->desc_len);
			zlib_compress(anongame_infos->anongame_infos_data_war3->desc_data, anongame_infos->anongame_infos_data_war3->desc_len, &anongame_infos->anongame_infos_data_war3->desc_comp_data, &anongame_infos->anongame_infos_data_war3->desc_comp_len);
		}
		}
		else
		{
		anongame_infos->anongame_infos_data_w3xp->desc_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_w3xp->desc_data = (char *)malloc(anongame_infos->anongame_infos_data_w3xp->desc_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_w3xp->desc_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_w3xp->desc_len), anongame_infos->anongame_infos_data_w3xp->desc_len);
			zlib_compress(anongame_infos->anongame_infos_data_w3xp->desc_data, anongame_infos->anongame_infos_data_w3xp->desc_len, &anongame_infos->anongame_infos_data_w3xp->desc_comp_data, &anongame_infos->anongame_infos_data_w3xp->desc_comp_len);
		}
		}
		}


		packet_set_size(raw, 0);
	    /*FIXME: Still adding a static number (5)
	    Also maybe need do do some checks to avoid prefs empty strings.*/
	    ladr_count=6;
	    packet_append_data(raw, &ladr_count, 1);
	    packet_append_data(raw, "OLOS", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_1v1_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_1v1_url());
	    packet_append_data(raw, "MAET", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_team_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_team_url());
	    packet_append_data(raw, " AFF", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_ffa_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_ffa_url());
	    packet_append_data(raw, "2SV2", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_2v2_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_2v2_url());
	    packet_append_data(raw, "3SV3", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_3v3_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_3v3_url());
	    packet_append_data(raw, "4SV4", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_4v4_desc(NULL));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_4v4_url());
		anongame_infos->anongame_infos_data_war3->ladr_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_war3->ladr_data = (char *)malloc(anongame_infos->anongame_infos_data_war3->ladr_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_war3->ladr_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_war3->ladr_len), anongame_infos->anongame_infos_data_war3->ladr_len);
			zlib_compress(anongame_infos->anongame_infos_data_war3->ladr_data, anongame_infos->anongame_infos_data_war3->ladr_len, &anongame_infos->anongame_infos_data_war3->ladr_comp_data, &anongame_infos->anongame_infos_data_war3->ladr_comp_len);
		}
		anongame_infos->anongame_infos_data_w3xp->ladr_len = packet_get_size(raw);
		if((anongame_infos->anongame_infos_data_w3xp->ladr_data = (char *)malloc(anongame_infos->anongame_infos_data_w3xp->ladr_len)) != NULL)
		{
			memcpy(anongame_infos->anongame_infos_data_w3xp->ladr_data, packet_get_data_const(raw, 0, anongame_infos->anongame_infos_data_w3xp->ladr_len), anongame_infos->anongame_infos_data_w3xp->ladr_len);
			zlib_compress(anongame_infos->anongame_infos_data_w3xp->ladr_data, anongame_infos->anongame_infos_data_w3xp->ladr_len, &anongame_infos->anongame_infos_data_w3xp->ladr_comp_data, &anongame_infos->anongame_infos_data_w3xp->ladr_comp_len);
		}


		packet_destroy(raw);
	}

	if((raw = packet_create(packet_class_raw)) != NULL)
	{
	LIST_TRAVERSE(anongame_infos->anongame_infos_DESC_list,curr)
	{
		t_anongame_infos_DESC * anongame_infos_DESC;
		t_anongame_infos_data_lang * anongame_infos_data_lang_war3;
		t_anongame_infos_data_lang * anongame_infos_data_lang_w3xp;
		anongame_infos_DESC = elem_get_data(curr);
		anongame_infos_data_lang_war3 = anongame_infos_data_lang_init(anongame_infos_DESC->langID);
		anongame_infos_data_lang_w3xp = anongame_infos_data_lang_init(anongame_infos_DESC->langID);
		for(k = 0; k < 2; k++)
		{
		desc_count = 0;
		packet_set_size(raw, 0);
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		    desc_count++;
	    packet_append_data(raw,&desc_count,1);
	    /* PG description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (!anongame_prefix[j][1] && !anongame_prefix[j][4] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_PG_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
			    
		    if (anongame_infos_get_short_desc(anongame_infos_DESC->langID, j) == NULL)
			packet_append_string(raw,anongame_gametype_names[j]);
		    else
			packet_append_string(raw,anongame_infos_get_short_desc(anongame_infos_DESC->langID, j));
			    
		    if (anongame_infos_get_long_desc(anongame_infos_DESC->langID, j) == NULL)
			packet_append_string(raw,"No Descreption");
		    else
			packet_append_string(raw,anongame_infos_get_long_desc(anongame_infos_DESC->langID, j));
		}
	    /* AT description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (!anongame_prefix[j][1] && anongame_prefix[j][4] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_AT_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
		    packet_append_string(raw,anongame_infos_get_short_desc(anongame_infos_DESC->langID, j));
		    packet_append_string(raw,anongame_infos_get_long_desc(anongame_infos_DESC->langID, j));
		}
	    /* TY description section */
	    for (j=0; j < ANONGAME_TYPES; j++)
		if (anongame_prefix[j][1] &&
			maplists_get_totalmaps_by_queue(game_clienttag[k], j))
		{
		    packet_append_data(raw, &anongame_TY_section, 1);
		    packet_append_data(raw, &anongame_prefix[j][0], 1);
		    packet_append_string(raw,anongame_infos_get_short_desc(anongame_infos_DESC->langID, j));
		    packet_append_string(raw,anongame_infos_get_long_desc(anongame_infos_DESC->langID, j));
		}
		if(k == 0)
		{
		anongame_infos_data_lang_war3->desc_len = packet_get_size(raw);
		if((anongame_infos_data_lang_war3->desc_data = (char *)malloc(anongame_infos_data_lang_war3->desc_len)) != NULL)
		{
			memcpy(anongame_infos_data_lang_war3->desc_data, packet_get_data_const(raw, 0, anongame_infos_data_lang_war3->desc_len), anongame_infos_data_lang_war3->desc_len);
			zlib_compress(anongame_infos_data_lang_war3->desc_data, anongame_infos_data_lang_war3->desc_len, &anongame_infos_data_lang_war3->desc_comp_data, &anongame_infos_data_lang_war3->desc_comp_len);
		}
		}
		else
		{
		anongame_infos_data_lang_w3xp->desc_len = packet_get_size(raw);
		if((anongame_infos_data_lang_w3xp->desc_data = (char *)malloc(anongame_infos_data_lang_w3xp->desc_len)) != NULL)
		{
			memcpy(anongame_infos_data_lang_w3xp->desc_data, packet_get_data_const(raw, 0, anongame_infos_data_lang_w3xp->desc_len), anongame_infos_data_lang_w3xp->desc_len);
			zlib_compress(anongame_infos_data_lang_w3xp->desc_data, anongame_infos_data_lang_w3xp->desc_len, &anongame_infos_data_lang_w3xp->desc_comp_data, &anongame_infos_data_lang_w3xp->desc_comp_len);
		}
		}
		}


		packet_set_size(raw, 0);
	    /*FIXME: Still adding a static number (5)
	    Also maybe need do do some checks to avoid prefs empty strings.*/
	    ladr_count=6;
	    packet_append_data(raw, &ladr_count, 1);
	    packet_append_data(raw, "OLOS", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_1v1_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_1v1_url());
	    packet_append_data(raw, "MAET", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_team_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_team_url());
	    packet_append_data(raw, " AFF", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_PG_ffa_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_PG_ffa_url());
	    packet_append_data(raw, "2SV2", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_2v2_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_2v2_url());
	    packet_append_data(raw, "3SV3", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_3v3_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_3v3_url());
	    packet_append_data(raw, "4SV4", 4);
	    packet_append_string(raw, anongame_infos_DESC_get_ladder_AT_4v4_desc(anongame_infos_DESC->langID));
	    packet_append_string(raw, anongame_infos_URL_get_ladder_AT_4v4_url());
		anongame_infos_data_lang_war3->ladr_len = packet_get_size(raw);
		if((anongame_infos_data_lang_war3->ladr_data = (char *)malloc(anongame_infos_data_lang_war3->ladr_len)) != NULL)
		{
			memcpy(anongame_infos_data_lang_war3->ladr_data, packet_get_data_const(raw, 0, anongame_infos_data_lang_war3->ladr_len), anongame_infos_data_lang_war3->ladr_len);
			zlib_compress(anongame_infos_data_lang_war3->ladr_data, anongame_infos_data_lang_war3->ladr_len, &anongame_infos_data_lang_war3->ladr_comp_data, &anongame_infos_data_lang_war3->ladr_comp_len);
		}
		list_append_data(anongame_infos->anongame_infos_data_lang_war3, anongame_infos_data_lang_war3);
		anongame_infos_data_lang_w3xp->ladr_len = packet_get_size(raw);
		if((anongame_infos_data_lang_w3xp->ladr_data = (char *)malloc(anongame_infos_data_lang_w3xp->ladr_len)) != NULL)
		{
			memcpy(anongame_infos_data_lang_w3xp->ladr_data, packet_get_data_const(raw, 0, anongame_infos_data_lang_w3xp->ladr_len), anongame_infos_data_lang_w3xp->ladr_len);
			zlib_compress(anongame_infos_data_lang_w3xp->ladr_data, anongame_infos_data_lang_w3xp->ladr_len, &anongame_infos_data_lang_w3xp->ladr_comp_data, &anongame_infos_data_lang_w3xp->ladr_comp_len);
		}
		list_append_data(anongame_infos->anongame_infos_data_lang_w3xp, anongame_infos_data_lang_w3xp);
	}
	packet_destroy(raw);
	}
	return 0;
}

extern int anongame_infos_unload(void)
{
	return anongame_infos_destroy(anongame_infos);
}

static int zlib_compress(void const * src, int srclen, char ** dest, int * destlen)
{
    char* tmpdata;
    z_stream zcpr;
    int ret;
    int lorigtodo;
    int lorigdone;
    int all_read_before;

    ret = Z_OK;
    lorigtodo = srclen;
    lorigdone = 0;
    *dest = NULL;

    tmpdata=(unsigned char*)malloc(srclen + (srclen/0x10) + 0x200 + 0x8000);
    if (!tmpdata) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for tmpdata");
	return -1;
    }

    memset(&zcpr,0,sizeof(z_stream));
    pvpgn_deflateInit(&zcpr, 9);
    zcpr.next_in = (void *)src;
    zcpr.next_out = tmpdata;
    do {
	all_read_before = zcpr.total_in;
	zcpr.avail_in = (lorigtodo < 0x8000) ? lorigtodo : 0x8000;
	zcpr.avail_out = 0x8000;
	ret = pvpgn_deflate(&zcpr,(zcpr.avail_in == lorigtodo) ? Z_FINISH : Z_SYNC_FLUSH);
	lorigdone += (zcpr.total_in-all_read_before);
	lorigtodo -= (zcpr.total_in-all_read_before);
    } while (ret == Z_OK);

    (*destlen) = zcpr.total_out;
    if((*destlen)>0)
    {
	(*dest) = malloc((*destlen) + 4);
	if (!(*dest)) {
	    eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for dest");
	    pvpgn_deflateEnd(&zcpr);
	    free((void*)tmpdata);
	    return -1;
	}
	bn_short_set((bn_short*)(*dest), lorigdone);
	bn_short_set((bn_short*)(*dest + 2), *destlen);
	memcpy((*dest)+4, tmpdata, (*destlen));
	(*destlen) += 4;
    }
    pvpgn_deflateEnd(&zcpr);

    free((void*)tmpdata);

    return 0;
}
