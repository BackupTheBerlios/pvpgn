/*
 * Copyright (C) 2004      ls_sons  (ls@gamelife.org)
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
#include "setup.h"

#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif

#include "connection.h"
#include "prefs.h"
#include "common/eventlog.h"
#include "compat/strcasecmp.h"
#include "compat/strdup.h"
#include "d2charlist.h"

extern void d2charlist_init(t_d2charlist * clist)
{
    clist->first=NULL ;
    clist->last=NULL;
}

extern void d2charlist_destroy(t_d2charlist * clist)
{
    t_d2charlist_internal *pointer;

    while (clist->first!=NULL)
    {
        pointer = clist->first;
        clist->first = pointer->next;
	free((void *)pointer->name);
        free((void *)pointer);
    }
}

extern int d2charlist_add_char(t_d2charlist *charlist, char const * name, bn_int mtime, bn_int level, bn_int exp)
{
    t_d2charlist_internal * charlist_internal;
    t_d2charlist_internal * search;
    char const * d2char_sort;
    d2char_sort = prefs_get_charlist_sort();
    charlist_internal = malloc(sizeof(t_d2charlist_internal));
    if(charlist_internal!=NULL)
    {
	charlist_internal->name = strdup(name);
	charlist_internal->mtime = bn_int_get(mtime);
	charlist_internal->level = bn_int_get(level);
	charlist_internal->exp = bn_int_get(exp);
	charlist_internal->prev = NULL;
	charlist_internal->next = NULL;
	if (charlist->first == NULL) {
	    charlist->first = charlist_internal;
	    charlist->last = charlist_internal;
	}
	else
	{
            if (!strcasecmp(d2char_sort, "name"))
            {
                search = charlist->first;
                while ((search != NULL) && (name > search->name))
                {
                    search = search->next;
                }
                do_add_char(charlist,search,charlist_internal);
            }
	    else if (!strcasecmp(d2char_sort, "mtime"))
	    {
		search = charlist->first;
		while ((search != NULL) && (bn_int_get(mtime) > search->mtime))
		{
		    search = search->next;
		}
		do_add_char(charlist,search,charlist_internal);
	    }
            else if (!strcasecmp(d2char_sort, "level"))
            {
                search = charlist->first;
                while ((search != NULL) && (bn_int_get(level) > search->level))
                {
                    search = search->next;
                }
                while ((search != NULL) && (bn_int_get(level) == search->level) && (bn_int_get(exp) > search->exp))
                {
                    search = search->next;
                }
                while ((search != NULL) && (bn_int_get(level) == search->level) && (bn_int_get(exp) == search->exp) && (name > search->name))
                {
                    search = search->next;
                }
                do_add_char(charlist,search,charlist_internal);
            }
            else
            {
		charlist->last->next = charlist_internal;
		charlist_internal->prev = charlist->last;
		charlist->last = charlist_internal;
            }
        }
        return 0;
    }
    else
    {
        eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for internal charlist");
        return -1;
    }
}

void do_add_char (t_d2charlist * charlist, t_d2charlist_internal * search, t_d2charlist_internal * charlist_internal)
{
    if (search == NULL)
    {
        charlist->last->next = charlist_internal;
        charlist_internal->prev = charlist->last;
        charlist->last = charlist_internal;
    }
    else
    {
        charlist_internal->next = search;
        if (search == charlist->first)
        {
            charlist->first = charlist_internal;
        }
        else
        {
            charlist_internal->prev = search->prev;
            search->prev->next = charlist_internal;
        }
        search->prev = charlist_internal;
    }
}
