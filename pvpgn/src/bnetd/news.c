/*
 * Copyright (C) 2000 Alexey Belyaev (spider@omskart.ru)
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

void news_get_lastnews();

static t_list * news_head=NULL;
static FILE * fp = NULL;
int last_news;
int first_news;

extern int news_load(const char *filename){
	unsigned int    line;
    unsigned int    pos;
	unsigned int	len;
	unsigned long	loffset;
	fpos_t	offset;
	struct tm    *date;
	char *	    buff;
    char *	    temp;	
	t_news_index	*ni=NULL;

	if (!filename) {
		eventlog(eventlog_level_error,"news_load","got NULL fullname");
		return -1;
	}

	if (!(news_head = list_create()))
    {
		eventlog(eventlog_level_error,"news_load","could create list");
		return -1;
    }

	if ((fp = fopen(filename,"rb"))==NULL){
		eventlog(eventlog_level_error,"news_load","can't open news file");
		return -1;
	}
	date=malloc(sizeof(struct tm));
	
	for (line=1; (buff = file_get_line(fp)); line++){
		for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
		if (buff[pos]=='#')
		{
		    free(buff);
			continue;
		}
		len = (int)strlen(buff);
		if ((len-pos>=12) && (buff[pos]=='{') && ((buff[pos+11]=='}'))){			
			char	flag;
			char	*dpart;
			char	dpos;			

			date->tm_hour=0;
			date->tm_min=0;
			date->tm_sec=0;
			date->tm_isdst=-1;
			flag = 0;
			dpart=(char*)malloc(5);
			dpos=0;
			for (++pos, flag=0;pos<len;pos++){
				if ((buff[pos]=='/') || (buff[pos]=='}')) {
					dpart[dpos]='\0';
					switch (flag++) 
					{
						case 0:date->tm_mon=atoi(dpart)-1;
							break;
						case 1:date->tm_mday=atoi(dpart);
							break;
						case 2:date->tm_year=atoi(dpart)-1900;
							break;
						default:
							eventlog(eventlog_level_error,"news_load","error parsing news date");
							free(dpart);
							return -1;
					}
					if (buff[pos]=='}')
						break;
					dpos=0;					
				}
				else 
				{	
					if (((dpos>1) && (flag<2)) || ((dpos>3) && (flag>1)))
					{
						eventlog(eventlog_level_error,"news_load","error parsing news date");
						free(dpart);
						return -1;
					}
					dpart[dpos++]=buff[pos];
				}
			}
			free(dpart);			
			if (ni!=NULL) 
			{
				ni->size=line-loffset;
			}
			if (!(ni = (t_news_index*)malloc(sizeof(t_news_index))))
			{
				eventlog(eventlog_level_error,"news_load","could not allocate memory for news index");
				return -1;
			}
			fgetpos(fp,(fpos_t *)&offset);
			ni->date=mktime(date);
			ni->offset=offset;
			loffset=line+1;
			if (list_append_data(news_head,ni)<0)
			{
				eventlog(eventlog_level_error,"news_load","could not append item");
				if (ni)
					free(ni);
				ni=NULL;
				continue;
			}
		}
		free(buff);
	}	
	//fgetpos(fp,(fpos_t *)&offset);
	if (ni!=NULL) 
	{
		first_news = ni->date;
		ni->size=line-loffset;
	}

	free(date);
//	fsetpos(fp,0);
	news_get_lastnews();
	return 0;
}

/* Free up all of the elements in the linked list */
extern int news_unload(void)
{
    t_elem *       curr;
	t_news_index * ni;
    
    if (news_head)
    {    
	LIST_TRAVERSE(news_head,curr)
	{
	    if (!(ni = elem_get_data(curr)))
		eventlog(eventlog_level_error,"news_unload","found NULL entry in list");
	    else
	    {		
		free(ni);
	    }
	    list_remove_elem(news_head,curr);
	}
	
	if (list_destroy(news_head)<0)
	    return -1;
	news_head = NULL;
    }

    // aaron: and now close the file:
    fclose(fp);
    
    return 0;
}

void news_get_lastnews()
{
	t_elem *       curr;
	t_news_index * ni;
	if (news_head)
	{
	LIST_TRAVERSE(news_head,curr)
	{
	    if (!(ni = elem_get_data(curr)))		
		eventlog(eventlog_level_error,"news_get_lastnews","found NULL entry in list");			
	    else
			if (last_news<ni->date)
				last_news=ni->date;
	}
	}
}

extern t_list * newslist(void)
{
    return news_head;
}

extern char const * news_get_body(t_news_index const * news)
{
	char	* buff;
	char	* big_buffer=NULL;
	unsigned int line,len=0,rlen;
	fpos_t	offset;		

	// Load news into buffer
	line=0;
	offset=(fpos_t)news->offset;
	fsetpos(fp,&offset);
	while ((++line<=news->size) && (buff = (char *)file_get_line(fp)))
	{		
		rlen=strlen(buff);
		len+=rlen+2;
		big_buffer=realloc(big_buffer,len);
		big_buffer[len-rlen-2]='\0';
		strncat(big_buffer,buff,rlen);
		strncat(big_buffer,"\r\n",2);
		free(buff);		
	}
	//big_buffer[news->size]='\0';
	return big_buffer;
}

extern int news_unget_body(char const * val)
{
    if (!val)
    {
		eventlog(eventlog_level_error,"news_unget_body","got NULL val");
		return -1;
    }
#ifdef TESTUNGET
    free((void *)val); /* avoid warning */
#endif
    return 0;
}

extern unsigned int const news_get_date(t_news_index const * news)
{
    if (!news)
    {
        eventlog(eventlog_level_warn,"news_get_date","got NULL news");
		return 0;
    }
    
	return news->date;
}

