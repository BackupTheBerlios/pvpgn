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
#include "compat/strchr.h"
#include "compat/strdup.h"
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"
#include "common/proginfo.h"
#include "news.h"
#include <time.h>
#include "common/setup_after.h"

static char * news_read_file(FILE * fp);

static t_list * news_head=NULL;
static FILE * fp = NULL;

extern int news_load(const char *filename)
{
    unsigned int	line;
    unsigned int	len;
    char		*buff;
    struct tm		*date;
    char date_set;
    t_news_index	*ni, *previous_ni;
    
    date_set = 0;
    previous_ni = NULL;

    if (!filename) {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL fullname");
	return -1;
    }

    if (!(news_head = list_create())) {
	eventlog(eventlog_level_error, __FUNCTION__,"could create list");
	return -1;
    }

    if ((fp = fopen(filename,"r"))==NULL) {
	eventlog(eventlog_level_error, __FUNCTION__,"can't open news file");
	return -1;
    }

	setbuf(fp,NULL);

    date=malloc(sizeof(struct tm));
    
    for (line=1; (buff = news_read_file(fp)); line++) {
	len = strlen(buff);
	
	if (buff[0]=='{') {
	    int		flag;
	    char	*dpart = malloc(5);
	    int		dpos;
	    unsigned 	pos;

	    date->tm_hour= 6; 
	    date->tm_min = 6;  // need to set non-zero values or else date is displayed wrong
	    date->tm_sec = 6;  
	    date->tm_isdst=-1;
	    dpos=0;

	    for (pos=1, flag=0; pos<len; pos++) {
		if ((buff[pos]=='/') || (buff[pos]=='}')) {
	    	    pos++;
		    dpart[dpos]='\0';
	    	    
		    switch (flag++) {
			case 0:
		    	    date->tm_mon=atoi(dpart)-1;
					if ((date->tm_mon<0) || (date->tm_mon>11))
						eventlog(eventlog_level_error,__FUNCTION__,"found invalid month (%i) in news date. (format: {MM/DD/YYYY}) on line %u",date->tm_mon,line);
		    	    break;
			case 1:
		    	    date->tm_mday=atoi(dpart);
					if ((date->tm_mday<1) || (date->tm_mday>31))
						eventlog(eventlog_level_error,__FUNCTION__,"found invalid month day (%i) in news date. (format: {MM/DD/YYYY}) on line %u",date->tm_mday,line);
		    	    break;
			case 2:
		    	    date->tm_year=atoi(dpart)-1900;
		    	    break;
			default:
		    	    eventlog(eventlog_level_error,__FUNCTION__,"error parsing news date on line %u",line);
		    	    free((void *)dpart);
					free((void *)date);
					free((void *)buff);
					fclose(fp);
		    		return -1;
		    }
	    	    
		    dpos=0;
		} 
		
		if (buff[pos]=='}')
	    	    break;

		dpart[dpos++]=buff[pos];
	    }
		
	    if (((dpos>1) && (flag<2)) || ((dpos>3) && (flag>1))) {
		eventlog(eventlog_level_error,__FUNCTION__,"dpos: %d, flag: %d", dpos, flag);
	    	eventlog(eventlog_level_error,"news_load","error parsing news date");
	    	free((void *)dpart);
		free((void *)date);
		free((void *)buff);
		fclose(fp);
		return -1;
	    }
	    date_set = 1;
	    free((void *)dpart);
	} else {
	    if (!(ni = (t_news_index*)malloc(sizeof(t_news_index)))) {
		eventlog(eventlog_level_error,"news_load","could not allocate memory for news index");
		return -1;
	    }
	    			
	    if (date_set==1) 
			ni->date=mktime(date);
		else
		{
			ni->date=time(0);
			eventlog(eventlog_level_error,__FUNCTION__,"(first) news entry seems to be missing a timestamp, please check your news file on line %u",line);
		}

	    if ((previous_ni) && (ni->date == previous_ni->date))
	    {
		eventlog(eventlog_level_error,__FUNCTION__,"found another news item for same date, trying to join both");
		
		if ((strlen(previous_ni->body) + strlen(buff) +2) > 1023)
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"failed in joining news, cause news too long - skipping");
		  free((void *)ni);
		}
		else
		{
		  previous_ni->body = realloc(previous_ni->body,strlen(previous_ni->body)+1+strlen(buff)+1);
		  sprintf(previous_ni->body,"%s\n%s",previous_ni->body,buff);
		  free((void *)ni);
		}
		
	    }
	    else
	    {
	      if ( strlen(buff)<1023 )
	      {
	        ni->body=strdup(buff);
	    
	        if (list_append_data(news_head,ni)<0) {
		
		  eventlog(eventlog_level_error,"news_load","could not append item");
		  if (ni)
		  {
		    if (ni->body) free(ni->body);
		    free(ni);
		  }
		  continue;
	        }
	        previous_ni = ni;
	      }
	      else
	      {
		eventlog(eventlog_level_error,__FUNCTION__,"news too long - skipping");
		free((void*)ni);
	      }
	    }
	}
	free((void *)buff);
    }	
    free((void *)date);
    
    fclose(fp);
    fp = NULL;
    return 0;
}

static char * news_read_file(FILE * fp)
{
    char * 	 line;
    char *       newline;
    unsigned int len=256;
    unsigned int pos=0;
    int          curr_char;
    
    if (!(line = malloc(256)))
	return NULL;

    while ((curr_char = fgetc(fp))!=EOF) {
	if (((char)curr_char)=='\r')
	    continue; /* make DOS line endings look Unix-like */
	if (((char)curr_char)=='{'&& pos>0 ) {
	    fseek(fp,ftell(fp)-1,SEEK_SET);
	    break; /* end of news body */
	}
	
	line[pos++] = (char)curr_char;
	if ((pos+1)>=len) {
	    len += 64;
	    if (!(newline = realloc(line,len))) {
		free(line);
		return NULL;
	    }
	    line = newline;
	}
	if (((char)curr_char)=='}'&& pos>0 ) {
	    if ((curr_char = fgetc(fp))!=EOF)
		if (((char)curr_char)!='\n') /* check for return */
		    fseek(fp,ftell(fp)-1,SEEK_SET); /* if not assume body starts and reset fp pos */
	    break; /* end of news date */
	}
    }
    
    if (curr_char==EOF && pos<1)  { /* not even an empty line */
	free(line);
	return NULL;
    }
			    
    if (pos+1<len)
	if ((newline = realloc(line,pos+1))) /* bump the size back down to what we need */
	    line = newline; /* if it fails just ignore it */
    line[pos] = '\0';
    return line;
}
							    
/* Free up all of the elements in the linked list */
extern int news_unload(void)
{
    t_elem *       curr;
    t_news_index * ni;
    
    if (news_head) {
	LIST_TRAVERSE(news_head,curr)
	{
	    if (!(ni = elem_get_data(curr))) {
	    	eventlog(eventlog_level_error,"news_unload","found NULL entry in list");
		continue;
	    }
	    
	    free((void *)ni->body);
	    free((void *)ni);
	    list_remove_elem(news_head,&curr);

	}
	
	if (list_destroy(news_head)<0)
	    return -1;
	
	news_head = NULL;
    }
    return 0;
}

extern unsigned int news_get_lastnews(void)
{
    t_elem	 *curr;
    t_news_index *ni;
    unsigned int last_news = 0;
    
    if (news_head) {
	LIST_TRAVERSE(news_head,curr)
	{
	    if (!(ni = elem_get_data(curr))) {
		eventlog(eventlog_level_error,"news_get_lastnews","found NULL entry in list");
		continue;
	    } else
		if (last_news<ni->date)
		    last_news=ni->date;
	}
    }
    return last_news;
}

extern unsigned int news_get_firstnews(void)
{
    t_elem	 *curr;
    t_news_index *ni;
    unsigned int first_news = 0;
    
    if (news_head) {
	LIST_TRAVERSE(news_head,curr)
	{
	    if (!(ni = elem_get_data(curr))) {
		eventlog(eventlog_level_error,"news_get_lastnews","found NULL entry in list");
		continue;
	    } else
	        if (first_news)
		{
		  if (first_news>ni->date)
		      first_news=ni->date;
		}
		else
		  first_news=ni->date;
	}
    }
    return first_news;
}

extern t_list * newslist(void)
{
    return news_head;
}

extern char * news_get_body(t_news_index const * news)
{
    if (!news) {
	eventlog(eventlog_level_warn,"news_get_date","got NULL news");
	return 0;
    }
    
    return news->body;
}

extern unsigned int news_get_date(t_news_index const * news)
{
    if (!news) {
	eventlog(eventlog_level_warn,"news_get_date","got NULL news");
	return 0;
    }
    
    return news->date;
}
