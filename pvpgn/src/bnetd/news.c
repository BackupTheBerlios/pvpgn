/*
 * Copyright (C) 2000 Alexey Belyaev (spider@omskart.ru)
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
#define NEWS_INTERNAL_ACCESS
#include "common/setup_before.h"
#include <stdio.h>
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
#include "compat/strtoul.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_TIME_H
# include <time.h>
#endif
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include "compat/strchr.h"
#include "compat/strdup.h"
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/elist.h"
#include "common/util.h"
#include "common/proginfo.h"
#include "common/xalloc.h"
#include "news.h"
#include "common/setup_after.h"

static t_elist news_head;

static int _news_parsetime(char *buff, struct tm *date, unsigned line)
{
    char *p;

    date->tm_hour= 6; 
    date->tm_min = 6;  // need to set non-zero values or else date is displayed wrong
    date->tm_sec = 6;  
    date->tm_isdst=-1;

    if (!(p = strchr(buff,'/'))) return -1;
    *p = '\0';

    date->tm_mon = atoi(buff) - 1;
    if ((date->tm_mon<0) || (date->tm_mon>11)) {
	eventlog(eventlog_level_error,__FUNCTION__,"found invalid month (%i) in news date. (format: {MM/DD/YYYY}) on line %u",date->tm_mon,line);
    }

    buff = p + 1;
    if (!(p = strchr(buff,'/'))) return -1;
    *p = '\0';

    date->tm_mday = atoi(buff);
    if ((date->tm_mday<1) || (date->tm_mday>31)) {
	eventlog(eventlog_level_error,__FUNCTION__,"found invalid month day (%i) in news date. (format: {MM/DD/YYYY}) on line %u",date->tm_mday,line);
	return -1;
    }

    buff = p + 1;
    if (!(p = strchr(buff,'}'))) return -1;
    *p = '\0';

    date->tm_year=atoi(buff)-1900;
    if (date->tm_year>137) //limited due to 32bit t_time
    {
	eventlog(eventlog_level_error,__FUNCTION__,"found invalid year (%i) (>2037) in news date.  on line %u",date->tm_year+1900,line);
	return -1;
    }

    return 0;
}

static void _news_insert_index(t_news_index *ni, char *buff, unsigned len, int date_set)
{
    t_elist *curr;
    t_news_index *cni;

    elist_for_each(curr,&news_head) {
	cni = elist_entry(curr,t_news_index,list);
	if (cni->date <= ni->date) break;
    }

    if (curr != &news_head && cni->date == ni->date) {
	if (date_set == 1)
	    eventlog(eventlog_level_warn,__FUNCTION__,"found another news item for same date, trying to join both");

	if ((lstr_get_len(&cni->body) + len +2) > 1023)
	    eventlog(eventlog_level_error,__FUNCTION__,"failed in joining news, cause news too long - skipping");
	else {
	    lstr_set_str(&cni->body,xrealloc(lstr_get_str(&cni->body),lstr_get_len(&cni->body) + 1 + len + 1));
	    *(lstr_get_str(&cni->body) + lstr_get_len(&cni->body)) = '\n';
	    strcpy(lstr_get_str(&cni->body) + lstr_get_len(&cni->body) + 1, buff);
	    lstr_set_len(&cni->body,lstr_get_len(&cni->body) + 1 + len);
	}
	xfree((void *)ni);
    } else {
	/* adding new index entry */
	lstr_set_str(&ni->body,xstrdup(buff));
	lstr_set_len(&ni->body,len);
	elist_add_tail(curr,&ni->list);
    }
}

extern int news_load(const char *filename)
{
    FILE * 		fp;
    unsigned int	line;
    unsigned int	len;
    char		buff[256];
    struct tm		date;
    char		date_set;
    t_news_index	*ni;

    elist_init(&news_head);

    date_set = 0;

    if (!filename) {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL fullname");
	return -1;
    }

    if ((fp = fopen(filename,"rt"))==NULL) {
	eventlog(eventlog_level_error, __FUNCTION__,"can't open news file");
	return -1;
    }

    for (line=1; fgets(buff,sizeof(buff),fp); line++) {
	len = strlen(buff);
	while(len && (buff[len] == '\n' || buff[len] == '\r')) len--;
	if (!len) continue; /* empty line */
	buff[len] = '\0';

	if (buff[0]=='{') {
	    if (_news_parsetime(buff + 1,&date, line)) {
		eventlog(eventlog_level_error,__FUNCTION__,"error parsing news date on line %u",line);
		return -1;
	    }
	    date_set = 1;
	} else {
	    ni = (t_news_index*)xmalloc(sizeof(t_news_index));
	    if (date_set)
		ni->date = mktime(&date);
	    else {
		ni->date = time(0);
		eventlog(eventlog_level_warn,__FUNCTION__,"(first) news entry seems to be missing a timestamp, please check your news file on line %u",line);
	    }
	    _news_insert_index(ni,buff,len,date_set);
	    date_set = 2;
	}
    }
    fclose(fp);

    return 0;
}

/* Free up all of the elements in the linked list */
extern int news_unload(void)
{
    t_elist *		curr, *save;
    t_news_index * 	ni;

    elist_for_each_safe(curr,&news_head,save)
    {
	ni = elist_entry(curr,t_news_index,list);
	elist_del(&ni->list);
	xfree((void *)lstr_get_str(&ni->body));
	xfree((void *)ni);
    }

    elist_init(&news_head);

    return 0;
}

extern unsigned int news_get_lastnews(void)
{
    if (elist_empty(&news_head)) return 0;
    return ((elist_entry(news_head.next,t_news_index,list))->date);
}

extern unsigned int news_get_firstnews(void)
{
    if (elist_empty(&news_head)) return 0;
    return ((elist_entry(news_head.prev,t_news_index,list))->date);
}

extern unsigned news_traverse(t_news_cb cb, void *data)
{
    unsigned count;
    t_elist *curr;
    t_news_index *cni;

    assert(cb);

    count = 0;
    elist_for_each(curr,&news_head)
    {
	cni = elist_entry(curr,t_news_index,list);
	if (cb(cni->date,&cni->body,data)) break;
	count++;
    }

    return count;
}
