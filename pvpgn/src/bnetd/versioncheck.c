/*
 * Copyright (C) 2000 Onlyer (onlyer@263.net)
 * Copyright (C) 2001 Ross Combs (ross@bnetd.org)
 * Copyright (C) 2002 Gianluigi Tiesi (sherpya@netfarm.it)
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
#define VERSIONCHECK_INTERNAL_ACCESS
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
#ifdef HAVE_MKTIME
# ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif
#endif
#include "compat/strchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include <ctype.h>
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"
#include "common/proginfo.h"
#include "common/token.h"
#include "common/field_sizes.h"
#include "prefs.h"
#include "versioncheck.h"
#include "common/setup_after.h"


static t_list * versioninfo_head=NULL;
static t_versioncheck dummyvc={ "A=42 B=42 C=42 4 A=A^S B=B^B C=C^C A=A^S", "IX86ver1.mpq", "NoVC" };

static int versioncheck_compare_exeinfo(t_parsed_exeinfo * pattern, t_parsed_exeinfo * match);

extern t_versioncheck * versioncheck_create(char const * archtag, char const * clienttag)
{
    t_elem const *   curr;
    t_versioninfo *  vi;
    t_versioncheck * vc;
    
    LIST_TRAVERSE_CONST(versioninfo_head,curr)
    {
        if (!(vi = elem_get_data(curr))) /* should not happen */
        {
            eventlog(eventlog_level_error,"versioncheck_create","version list contains NULL item");
            continue;
        }
	
	eventlog(eventlog_level_debug,"versioncheck_create","version check entry archtag=%s, clienttag=%s",vi->archtag,vi->clienttag);
	if (strcmp(vi->archtag,archtag)!=0)
	    continue;
	if (strcmp(vi->clienttag,clienttag)!=0)
	    continue;
	
	/* FIXME: randomize the selection if more than one match */
	if (!(vc = malloc(sizeof(t_versioncheck))))
	{
	    eventlog(eventlog_level_error,"versioncheck_create","unable to allocate memory for vc");
	    return &dummyvc;
	}
	if (!(vc->eqn = strdup(vi->eqn)))
	{
	    eventlog(eventlog_level_error,"versioncheck_create","unable to allocate memory for eqn");
	    free(vc);
	    return &dummyvc;
	}
	if (!(vc->mpqfile = strdup(vi->mpqfile)))
	{
	    eventlog(eventlog_level_error,"versioncheck_create","unable to allocate memory for mpqfile");
	    free((void *)vc->eqn); /* avoid warning */
	    free(vc);
	    return &dummyvc;
	}
	vc->versiontag = strdup(clienttag);
	
	return vc;
    }
    
    /*
     * No entries in the file that match, return the dummy because we have to send
     * some equation and auth mpq to the client.  The client is not going to pass the
     * validation later unless skip_versioncheck or allow_unknown_version is enabled.
     */
    return &dummyvc;
}


extern int versioncheck_destroy(t_versioncheck * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,"versioncheck_destroy","got NULL vc");
	return -1;
    }
    
    if (vc==&dummyvc)
	return 0;
    
    free((void *)vc->versiontag);
    free((void *)vc->mpqfile);
    free((void *)vc->eqn);
    free(vc);
    
    return 0;
}

extern int versioncheck_set_versiontag(t_versioncheck * vc, char const * versiontag)
{
    if (!vc) {
	eventlog(eventlog_level_error,"versioncheck_set_versiontag","got NULL vc");
	return -1;
    }
    if (!versiontag) {
	eventlog(eventlog_level_error,"versioncheck_set_versiontag","got NULL versiontag");
	return -1;
    }
    
    if (vc->versiontag!=NULL) free((void *)vc->versiontag);
    vc->versiontag = strdup(versiontag);
    return 0;
}


extern char const * versioncheck_get_versiontag(t_versioncheck const * vc)
{
    if (!vc) {
	eventlog(eventlog_level_error,"versioncheck_get_versiontag","got NULL vc");
	return NULL;
    }
    
    return vc->versiontag;
}


extern char const * versioncheck_get_mpqfile(t_versioncheck const * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,"versioncheck_get_mpqfile","got NULL vc");
	return NULL;
    }
    
    return vc->mpqfile;
}


extern char const * versioncheck_get_eqn(t_versioncheck const * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,"versioncheck_get_mpqfile","got NULL vc");
	return NULL;
    }
    
    return vc->eqn;
}

t_parsed_exeinfo * parse_exeinfo(char const * exeinfo)
{
  t_parsed_exeinfo * parsed_exeinfo;

    if (!exeinfo) {
	return NULL;
    }

    if (!(parsed_exeinfo = malloc(sizeof(t_parsed_exeinfo))))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for parsed exeinfo");
      return NULL;
    }

    if (!(parsed_exeinfo->exe = strdup(exeinfo)))
    {
      free((void *)parsed_exeinfo);
      eventlog(eventlog_level_error,__FUNCTION__,"could not duplicate exeinfo");
      return NULL;
    }
    parsed_exeinfo->time = 0;
    parsed_exeinfo->size = 0;
    
    if (strcmp(prefs_get_version_exeinfo_match(),"parse")==0) 
    {
#ifdef HAVE_MKTIME
	struct tm t1;
	char *exe;
	char mask[MAX_EXEINFO_STR+1];
	char * marker;
	int size;
        char time_invalid = 0;

	if (exeinfo[0]=='\0') //happens when using war3-noCD and having deleted war3.org
	{
          free((void *)parsed_exeinfo->exe);
          free((void *)parsed_exeinfo);
          eventlog(eventlog_level_error,__FUNCTION__,"found empty exeinfo");
          return NULL;
	}

        memset(&t1,0,sizeof(t1));
        t1.tm_isdst = -1;

        exeinfo    = strreverse((char *)exeinfo);
        if (!(marker     = strchr(exeinfo,' ')))
        {
	  free((void *)parsed_exeinfo->exe);
	  free((void *)parsed_exeinfo);
	  return NULL;
        }
	for (; marker[0]==' ';marker++); 

        if (!(marker     = strchr(marker,' ')))
        {
	  free((void *)parsed_exeinfo->exe);
	  free((void *)parsed_exeinfo);
	  return NULL;
	} 
	for (; marker[0]==' ';marker++);

        if (!(marker     = strchr(marker,' ')))
        {
	  free((void *)parsed_exeinfo->exe);
	  free((void *)parsed_exeinfo);
	  return NULL;
	}
        for (; marker[0]==' ';marker++);
        marker--;
        marker[0]  = '\0';
        marker++; 
        
        if (!(exe = strdup(marker)))
        {
          free((void *)parsed_exeinfo->exe);
          free((void *)parsed_exeinfo);
          eventlog(eventlog_level_error,__FUNCTION__,"could not duplicate exe");
          return NULL;
        }
        free((void *)parsed_exeinfo->exe);
        parsed_exeinfo->exe = strreverse((char *)exe);

        exeinfo    = strreverse((char *)exeinfo);

	sprintf(mask,"%%02u/%%02u/%%u %%02u:%%02u:%%02u %%u");

	if (sscanf(exeinfo,mask,&t1.tm_mon,&t1.tm_mday,&t1.tm_year,&t1.tm_hour,&t1.tm_min,&t1.tm_sec,&size)!=7) {
            if (sscanf(exeinfo,"%*s %*s %u",&size) != 1)
            {

	      eventlog(eventlog_level_warn,__FUNCTION__,"parser error while parsing pattern \"%s\"",exeinfo);
	      free((void *)parsed_exeinfo->exe);
	      free((void *)parsed_exeinfo);
	      return NULL; /* neq */
            }
            time_invalid=1;
	}

       /* Now we have a Y2K problem :)  Thanks for using a 2 digit decimal years, Blizzard. */ 
       /* 00-79 -> 2000-2079 
	*             * 80-99 -> 1980-1999 
	*             * 100+ unchanged */ 
       if (t1.tm_year<80) 
	 t1.tm_year = t1.tm_year + 100; 

       if (time_invalid)
         parsed_exeinfo->time = -1;
       else 
         parsed_exeinfo->time = mktime(&t1);
       parsed_exeinfo->size = size;

#else
	eventlog(eventlog_level_error,__FUNCTION__,"Your system does not support mktime(). Please select another exeinfo matching method.");
	return NULL;
#endif
  }

  return parsed_exeinfo;
}

#define safe_toupper(X) (islower((int)X)?toupper((int)X):(X))

/* This implements some dumb kind of pattern matching. Any '?'
 * signs in the pattern are treated as "don't care" signs. This
 * means that it doesn't matter what's on this place in the match.
 */
//static int versioncheck_compare_exeinfo(char const * pattern, char const * match)
static int versioncheck_compare_exeinfo(t_parsed_exeinfo * pattern, t_parsed_exeinfo * match)
{
    if (!pattern) {
	eventlog(eventlog_level_error,"versioncheck_compare_exeinfo","got NULL pattern");
	return -1; /* neq/fail */
    }
    if (!match) {
	eventlog(eventlog_level_error,"versioncheck_compare_exeinfo","got NULL match");
	return -1; /* neq/fail */
    }

    if (strlen(pattern->exe)!=strlen(match->exe))
    	return 1; /* neq */
    
    if (strcmp(prefs_get_version_exeinfo_match(),"exact")==0) {
	return strcasecmp(pattern->exe,match->exe);
    } else if (strcmp(prefs_get_version_exeinfo_match(),"exactcase")==0) {
	return strcmp(pattern->exe,match->exe);
    } else if (strcmp(prefs_get_version_exeinfo_match(),"wildcard")==0) {
    	unsigned int i;
    	
    	for (i=0;i<strlen(pattern->exe);i++)
    	    if ((pattern->exe[i]!='?')&& /* out "don't care" sign */
    	    	(safe_toupper(pattern->exe[i])!=safe_toupper(match->exe[i])))
    	    	return 1; /* neq */
    	return 0; /* ok */
    } else if (strcmp(prefs_get_version_exeinfo_match(),"parse")==0) {
       
       if (strcasecmp(pattern->exe,match->exe)!=0)
            {
            eventlog(eventlog_level_trace,__FUNCTION__,"filename differs");
	    return 1; /* neq */
            }
	if (pattern->size!=match->size)
            {
            eventlog(eventlog_level_trace,__FUNCTION__,"size differs");
	    return 1; /* neq */
            }
	if ((pattern->time!=-1) && (abs(pattern->time-match->time)>(signed)prefs_get_version_exeinfo_maxdiff()))
            {
            eventlog(eventlog_level_trace,__FUNCTION__,"time differs by %i",abs(pattern->time-match->time));
	    return 1;
            }
	return 0; /* ok */
    } else {
	eventlog(eventlog_level_error,"versioncheck_compare_exeinfo","unknown version exeinfo match method \"%s\"",prefs_get_version_exeinfo_match());
	return -1; /* neq/fail */
    }
}

void free_parsed_exeinfo(t_parsed_exeinfo * parsed_exeinfo)
{
  if (parsed_exeinfo)
  {
    if (parsed_exeinfo->exe) 
      free((void *)parsed_exeinfo->exe);
    free((void *)parsed_exeinfo);
  }
}

extern int versioncheck_validate(t_versioncheck * vc, char const * archtag, char const * clienttag, char const * exeinfo, unsigned long versionid, unsigned long gameversion, unsigned long checksum)
{
    t_elem const     * curr;
    t_versioninfo    * vi;
    int                badexe,badcs;
    t_parsed_exeinfo * parsed_exeinfo;
    
    if (!vc)
    {
	eventlog(eventlog_level_error,"versioncheck_validate","got NULL vc");
	return -1;
    }
    
    badexe=badcs = 0;
    parsed_exeinfo = parse_exeinfo(exeinfo);
    LIST_TRAVERSE_CONST(versioninfo_head,curr)
    {
        if (!(vi = elem_get_data(curr))) /* should not happen */
        {
	    eventlog(eventlog_level_error,"versioncheck_validate","version list contains NULL item");
	    continue;
        }
	
	if (strcmp(vi->eqn,vc->eqn)!=0)
	    continue;
	if (strcmp(vi->mpqfile,vc->mpqfile)!=0)
	    continue;
	if (strcmp(vi->archtag,archtag)!=0)
	    continue;
	if (strcmp(vi->clienttag,clienttag)!=0)
	    continue;
	
	if (vi->versionid && vi->versionid != versionid)
	    continue;
	
	if (vi->gameversion && vi->gameversion != gameversion)
	    continue;

	
	if ((!(parsed_exeinfo)) || (vi->parsed_exeinfo && (versioncheck_compare_exeinfo(vi->parsed_exeinfo,parsed_exeinfo) != 0)))
	{
	    /*
	     * Found an entry matching but the exeinfo doesn't match.
	     * We need to rember this because if no other matching versions are found
	     * we will return badversion.
	     */
	    badexe = 1;
	}
	else
	    badexe = 0;
	
	if (vi->checksum && vi->checksum != checksum)
	{
	    /*
	     * Found an entry matching but the checksum doesn't match.
	     * We need to rember this because if no other matching versions are found
	     * we will return badversion.
	     */
	    badcs = 1;
	}
	else
	    badcs = 0;
	
	if (vc->versiontag)
	    free((void *)vc->versiontag);
	vc->versiontag = strdup(vi->versiontag);
	
	if (badexe || badcs)
	    continue;
	
	/* Ok, version and checksum matches or exeinfo/checksum are disabled
	 * anyway we have found a complete match */
	eventlog(eventlog_level_info,"versioncheck_validate","got a matching entry: %s",vc->versiontag);
        free_parsed_exeinfo(parsed_exeinfo);
	return 1;
    }
    
    if (badcs) /* A match was found but the checksum was different */
    {
	eventlog(eventlog_level_info,"versioncheck_validate","bad checksum, closest match is: %s",vc->versiontag);
        free_parsed_exeinfo(parsed_exeinfo);
	return -1;
    }
    if (badexe) /* A match was found but the exeinfo string was different */
    {
	eventlog(eventlog_level_info,"versioncheck_validate","bad exeinfo, closest match is: %s",vc->versiontag);
        free_parsed_exeinfo(parsed_exeinfo);
	return -1;
    }
    
    /* No match in list */
    eventlog(eventlog_level_info,"versioncheck_validate","no match in list, setting to: %s",vc->versiontag);
    free_parsed_exeinfo(parsed_exeinfo);
    return 0;
}

extern int versioncheck_load(char const * filename)
{
    FILE *	    fp;
    unsigned int    line;
    unsigned int    pos;
    char *	    buff;
    char *	    temp;
    char const *    eqn;
    char const *    mpqfile;
    char const *    archtag;
    char const *    clienttag;
    char const *    exeinfo;
    char const *    versionid;
    char const *    gameversion;
    char const *    checksum;
    char const *    versiontag;
    t_versioninfo * vi;
    
    if (!filename)
    {
	eventlog(eventlog_level_error,"versioncheck_load","got NULL filename");
	return -1;
    }
    
    if (!(versioninfo_head = list_create()))
    {
	eventlog(eventlog_level_error,"versioncheck_load","could create list");
	return -1;
    }
    if (!(fp = fopen(filename,"r")))
    {
	eventlog(eventlog_level_error,"versioncheck_load","could not open file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	list_destroy(versioninfo_head);
	versioninfo_head = NULL;
	return -1;
    }

    line = 1;
    for (; (buff = file_get_line(fp)); line++)
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
	    for (endpos=len-1;  buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
	    buff[endpos+1] = '\0';
	}

	if (!(eqn = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing eqn near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(mpqfile = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing mpqfile near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(archtag = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing archtag near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(clienttag = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing clienttag near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(exeinfo = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing exeinfo near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(versionid = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing versionid near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(gameversion = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing gameversion near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(checksum = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","missing checksum near line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	line++;
	if (!(versiontag = next_token(buff,&pos)))
	{
	    versiontag = NULL;
	}
	
	if (!(vi = malloc(sizeof(t_versioninfo))))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for vi");
	    free(buff);
	    continue;
	}
	if (!(vi->eqn = strdup(eqn)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for eqn");
	    free(vi);
	    free(buff);
	    continue;
	}
	if (!(vi->mpqfile = strdup(mpqfile)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for mpqfile");
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
	}
	if (strlen(archtag)!=4)
	{
	    eventlog(eventlog_level_error,"versioncheck_load","invalid arch tag on line %u of file \"%s\"",line,filename);
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
	}
	if (!(vi->archtag = strdup(archtag)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for archtag");
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
	}
	if (strlen(clienttag)!=4)
	{
	    eventlog(eventlog_level_error,"versioncheck_load","invalid client tag on line %u of file \"%s\"",line,filename);
	    free((void *)vi->archtag); /* avoid warning */
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
	}
	if (!(vi->clienttag = strdup(clienttag)))
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for clienttag");
	    free((void *)vi->archtag); /* avoid warning */
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
	}
	if (strcmp(exeinfo, "NULL") == 0)
	    vi->parsed_exeinfo = NULL;
	else
	{
	    if (!(vi->parsed_exeinfo = parse_exeinfo(exeinfo)))
	    {
		eventlog(eventlog_level_error,"versioncheck_load","encountered an error while parsing exeinfo");
		free((void *)vi->clienttag); /* avoid warning */
		free((void *)vi->archtag); /* avoid warning */
		free((void *)vi->mpqfile); /* avoid warning */
		free((void *)vi->eqn); /* avoid warning */
		free(vi);
		free(buff);
		continue;
	    }
	}

	vi->versionid = strtoul(versionid,NULL,0);
	if (verstr_to_vernum(gameversion,&vi->gameversion)<0)
	{
	    eventlog(eventlog_level_error,"versioncheck_load","malformed version on line %u of file \"%s\"",line,filename);
	    free((void *)vi->parsed_exeinfo); /* avoid warning */
	    free((void *)vi->clienttag); /* avoid warning */
	    free((void *)vi->archtag); /* avoid warning */
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    free(buff);
	    continue;
        }

	vi->checksum = strtoul(checksum,NULL,0);
	if (versiontag)
	{
	    if (!(vi->versiontag = strdup(versiontag)))
	    {
		eventlog(eventlog_level_error,"versioncheck_load","could not allocate memory for versiontag");
		free((void *)vi->parsed_exeinfo); /* avoid warning */
		free((void *)vi->clienttag); /* avoid warning */
		free((void *)vi->archtag); /* avoid warning */
		free((void *)vi->mpqfile); /* avoid warning */
		free((void *)vi->eqn); /* avoid warning */
		free(vi);
		free(buff);
		continue;
	    }
	}
	else
	    vi->versiontag = NULL;
	
	free(buff);
	
	if (list_append_data(versioninfo_head,vi)<0)
	{
	    eventlog(eventlog_level_error,"versioncheck_load","could not append item");
	    if (vi->versiontag)
	      free((void *)vi->versiontag); /* avoid warning */
	    free((void *)vi->parsed_exeinfo); /* avoid warning */
	    free((void *)vi->clienttag); /* avoid warning */
	    free((void *)vi->archtag); /* avoid warning */
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    free(vi);
	    continue;
	}
    }
    
    if (fclose(fp)<0)
	eventlog(eventlog_level_error,"versioncheck_load","could not close versioncheck file \"%s\" after reading (fclose: %s)",filename,strerror(errno));
    
    return 0;
}


extern int versioncheck_unload(void)
{
    t_elem *	    curr;
    t_versioninfo * vi;
    
    if (versioninfo_head)
    {
	LIST_TRAVERSE(versioninfo_head,curr)
	{
	    if (!(vi = elem_get_data(curr))) /* should not happen */
	    {
		eventlog(eventlog_level_error,"versioncheck_unload","version list contains NULL item");
		continue;
	    }
	    
	    if (list_remove_elem(versioninfo_head,curr)<0)
		eventlog(eventlog_level_error,"versioncheck_unload","could not remove item from list");

	    if (vi->parsed_exeinfo)
            {
                if (vi->parsed_exeinfo->exe)
                    free((void *)vi->parsed_exeinfo->exe);
		free((void *)vi->parsed_exeinfo); /* avoid warning */
            }
	    free((void *)vi->clienttag); /* avoid warning */
	    free((void *)vi->archtag); /* avoid warning */
	    free((void *)vi->mpqfile); /* avoid warning */
	    free((void *)vi->eqn); /* avoid warning */
	    if (vi->versiontag)
		free((void *)vi->versiontag); /* avoid warning */
	    free(vi);
	}
	
	if (list_destroy(versioninfo_head)<0)
	    return -1;
	versioninfo_head = NULL;
    }
    
    return 0;
}

