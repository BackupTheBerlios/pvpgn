/*
 * Copyright (C) 2004	Aaron
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
#include "errno.h"
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/tag.h"
#include "clienttag.h"
#include "common/setup_after.h"

static t_clienttag str_to_uint(char const * str)
{
    t_clienttag result;

    result  = str[0]<<24;
    result |= str[1]<<16;
    result |= str[2]<<8;
    result |= str[3];
	
    return result;
}

extern t_clienttag clienttag_str_to_uint(char const * clienttag)
{
	if (!clienttag)
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL clienttag");
		return CLIENTTAG_UNKNOWN_UINT;
	}

	return str_to_uint(clienttag);
}

extern char const * clienttag_uint_to_str(t_clienttag clienttag)
{
	switch (clienttag)
	{
	    case CLIENTTAG_BNCHATBOT_UINT:
		return CLIENTTAG_BNCHATBOT;
	    case CLIENTTAG_STARCRAFT_UINT:
		return CLIENTTAG_STARCRAFT;
	    case CLIENTTAG_BROODWARS_UINT:
		return CLIENTTAG_BROODWARS;
	    case CLIENTTAG_SHAREWARE_UINT:
		return CLIENTTAG_SHAREWARE;
	    case CLIENTTAG_DIABLORTL_UINT:
		return CLIENTTAG_DIABLORTL;
	    case CLIENTTAG_DIABLOSHR_UINT:
	    	return CLIENTTAG_DIABLOSHR;
	    case CLIENTTAG_WARCIIBNE_UINT:
	    	return CLIENTTAG_WARCIIBNE;
	    case CLIENTTAG_DIABLO2DV_UINT:
	    	return CLIENTTAG_DIABLO2DV;
	    case CLIENTTAG_STARJAPAN_UINT:
	    	return CLIENTTAG_STARJAPAN;
	    case CLIENTTAG_DIABLO2ST_UINT:
	    	return CLIENTTAG_DIABLO2ST;
	    case CLIENTTAG_DIABLO2XP_UINT:
	    	return CLIENTTAG_DIABLO2XP;
	    case CLIENTTAG_WARCRAFT3_UINT:
	    	return CLIENTTAG_WARCRAFT3;
	    case CLIENTTAG_WAR3XP_UINT:
	    	return CLIENTTAG_WAR3XP;
	    default:
		return CLIENTTAG_UNKNOWN;
	}
}
