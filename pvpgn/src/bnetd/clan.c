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
#define CLAN_INTERNAL_ACCESS
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
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strrchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
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
#include "common/list.h"
#include "prefs.h"
#include "clan.h"
#include "common/setup_after.h"

static t_list * clanlist_head=NULL;
t_elem        * memberlist_curr;
int max_clanid = 0;

int clan_memberlist_unload(t_clan * clan)
{
  t_elem       * curr;
  t_clanmember * member;

  if (clan->members)
  {
    LIST_TRAVERSE(clan->members,curr)
    {
      if (!(member = elem_get_data(curr)))
      {
        eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
        continue;
      }
      free((void *)member);
      list_remove_elem(clan->members,curr);
    }

    if (list_destroy(clan->members)<0)
      return -1;

    clan->members = NULL;
  }

  return 0;
}

int clan_save(t_clan * clan)
{
  FILE * fp;
  t_elem * curr;
  t_clanmember * member;
  char * clanfile;

  if (!(clanfile = malloc(strlen(prefs_get_clandir())+1+4+1)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for filename");
    return -1;
  }

  sprintf(clanfile,"%s/%c%c%c%c",prefs_get_clandir(),
                                 clan->clanshort[0],clan->clanshort[1],
                                 clan->clanshort[2],clan->clanshort[3]);

  if ((fp = fopen(clanfile,"w"))==NULL)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"can't open clanfile \"%s\"",clanfile);
    free((void *)clanfile);
    return -1;
  }

  fprintf(fp,"\"%s\",\"%s\"%i,%i",clan->clanname,clan->clan_motd,clan->clanid,(int)clan->creation_time);

  LIST_TRAVERSE(clan->members,curr)
  {
     if (!(member = elem_get_data(curr)))
     {
       eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
       continue;
     }
     fprintf(fp,"%i,%c,%i",member->uid,member->status,member->join_time);
  }


  fclose(fp);
  free((void *)clanfile);
  return 0;
}

t_clan * clan_load(char * clanfile, char * clanshort)
{
  t_clan       * clan;
  FILE         * fp;
  char           clanname[26];
  char           motd[51];
  int            cid, creation_time;
  int            member_uid, member_join_time;
  char           member_status;
  t_clanmember * member;
  
  if (!(clanfile))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clanfile");
    return NULL;
  }

  if ((fp = fopen(clanfile,"r"))==NULL)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"can't open clanfile \"%s\"",clanfile);
    return NULL;
  }

  clan = malloc(sizeof(t_clan));
  memcpy(clan->clanshort,clanshort,4);

  fscanf(fp,"\"%[^\"]\",\"%[^\"]\",%i,%i",clanname,motd,&cid,&creation_time);
  clan->clanname = strdup(clanname);
  clan->clan_motd = strdup(motd);
  clan->clanid = cid;
  clan->creation_time=(time_t)creation_time;
 
  eventlog(eventlog_level_trace,__FUNCTION__,"name: %s motd: %s clanid: %i time: %i",clanname,motd,cid,creation_time);

  if (cid>max_clanid) max_clanid=cid;

  clan->members = list_create();

  while (fscanf(fp,"%i,%c,%i",&member_uid,&member_status,&member_join_time)==3)
  {
     if (!(member = malloc(sizeof(t_clanmember))))
     {
        eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clan member");
        free((void *)member);
        clan_memberlist_unload(clan);
        free((void *)clan->clanname);
        free((void *)clan);
        return NULL;
     }
     member->uid       = member_uid;
     member->status    = member_status;
     member->join_time = member_join_time;

     if (list_append_data(clan->members,member)<0)
     {
       eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
       free((void *)member);
       clan_memberlist_unload(clan);
       free((void *)clan->clanname);
       free((void *)clan->clan_motd);
       free((void *)clan);
       return NULL;
     }
     eventlog(eventlog_level_trace,__FUNCTION__,"added member: uid: %i status: %i join_time: %i",member_uid,member_status,member_join_time);
  }

  fclose(fp);
  return clan;
}

int clanlist_add(t_clan * clan)
{
  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return -1;
  }

  if (!(clan->clanid))
    clan->clanid=++max_clanid;

  if (list_append_data(clanlist_head,clan)<0)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
    clan_memberlist_unload(clan);
    free((void *)clan->clanname);
    free((void *)clan);
    return -1;
  }

  return clan->clanid;
}

int clanlist_load(char const * clansdir)
{
  char const * dentry;
  t_pdir     * clandir;
  char       * pathname;
  char       clanshort[4];
  t_clan     * clan;

  // make sure to unload previous clanlist before loading again
  if (clanlist_head) clanlist_unload();

  if (!(clanlist_head = list_create()))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not create clanlist");
    return -1;
  }

  if (!(clandir = p_opendir(clansdir)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"unable to open clan directory \"%s\" for reading (p_opendir: %s)",clansdir,strerror(errno));
    return -1;
  }
  eventlog(eventlog_level_trace,__FUNCTION__,"start reading clans");
  while (dentry = p_readdir(clandir))
  {
    if (dentry[0]=='.') continue;
    
    if (!(pathname = malloc(strlen(clansdir)+1+strlen(dentry)+1)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for pathname");
      continue;
    }
  
    sprintf(pathname,"%s/%s",clansdir,dentry);

    if (strlen(dentry)>4)
    {
      eventlog(eventlog_level_error,__FUNCTION__,"found too long clan filename in clandir \"%s\"",dentry);
      continue;
    }
    
    strcpy(clanshort,dentry);
    clan = clan_load(pathname,clanshort);
    clanlist_add(clan);
    free((void *)pathname);

  }

  if (p_closedir(clandir)<0)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"unable to close clan directory \"%s\" (p_closedir: %s)",clansdir,strerror(errno));
  }
  eventlog(eventlog_level_trace,__FUNCTION__,"finished reading clans");

  return 0;
}

int clanlist_unload()
{
  t_elem * curr;
  t_clan * clan;

  if (clanlist_head)
  {
    LIST_TRAVERSE(clanlist_head,curr)
    {
      if (!(clan = elem_get_data(curr)))
      {
        eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
        continue;
      }
      if (clan->clanname) free((void *)clan->clanname);
      if (clan->clan_motd) free((void *)clan->clan_motd);
      clan_memberlist_unload(clan);
      free((void *)clan);
      list_remove_elem(clanlist_head,curr);
    }

    if (list_destroy(clanlist_head)<0)
      return -1;

    clanlist_head = NULL;
  }

  return 0;
}

t_clan * get_clan_by_clanid(int cid)
{
  t_elem * curr;
  t_clan * clan;

  if (clanlist_head)
  {
    LIST_TRAVERSE(clanlist_head,curr)
    {
      if (!(clan = elem_get_data(curr)))
      {
        eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
        continue;
      }
      if (clan->clanid==cid) return clan;
    }

  }

  return NULL;
}

t_clan * get_clan_by_clanshort(char clanshort[4])
{
  t_elem * curr;
  t_clan * clan;

  if (clanlist_head)
  {
    LIST_TRAVERSE(clanlist_head,curr)
    {
      if (!(clan = elem_get_data(curr)))
      {
        eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
        continue;
      }
      if ((clan->clanshort[0]==clanshort[0]) &&
          (clan->clanshort[1]==clanshort[1]) &&
          (clan->clanshort[2]==clanshort[2]) &&
          (clan->clanshort[3]==clanshort[3])) return clan;
    }

  }

  return NULL;
}

t_clanmember * clan_get_first_member(t_clan * clan)
{
  t_clanmember * member;

  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return NULL;
  }

  if (!(clan->members))
  {
     eventlog(eventlog_level_error,__FUNCTION__,"found NULL clan->members");
     return NULL;
  }

  memberlist_curr = list_get_first(clan->members);

  if (!(memberlist_curr))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"clan has no members");
    return NULL;
  }

  if (!(member = elem_get_data(memberlist_curr)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL element in list");
    return NULL;
  }

  return member;
}

t_clanmember * clan_get_next_member()
{
  t_clanmember * member;

  if (!(memberlist_curr))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL memberlist_curr");
    return NULL;
  }

  memberlist_curr = elem_get_next(memberlist_curr);

  if (!(memberlist_curr)) return NULL;

  if (!(member = elem_get_data(memberlist_curr)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL element in list");
    return NULL;
  }

  return member;
}

int clanmember_get_uid(t_clanmember * member)
{
  if (!(member))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clanmember");
    return -1;
  }
  
  return member->uid;
}

char clanmember_get_status(t_clanmember * member)
{
  if (!(member))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clanmember");
    return 0;
  }

  return member->status;
}

time_t clanmember_get_join_time(t_clanmember * member)
{
  if (!(member))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clanmember");
    return 0;
  }

  return member->join_time;
}

char * clan_get_clanname(t_clan * clan)
{
  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return NULL;
  }

  return clan->clanname;
}

char * clan_get_clan_motd(t_clan * clan)
{
  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return NULL;
  }

  return clan->clan_motd;
}

int clan_get_clanid(t_clan * clan)
{
  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return -1;
  }

  return clan->clanid;
}

time_t clan_get_creation_time(t_clan * clan)
{
  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return 0;
  }

  return clan->creation_time;
}

int clan_add_member(t_clan * clan, int uid, char status)
{
  t_clanmember * member;

  if (!(clan))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clan");
    return -1;
  }

  if (!(clan->members))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"found NULL clan->members");
    return -1;
  }

  if (!(member = malloc(sizeof(t_clanmember))))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clanmember");
    return -1;
  }

  member->uid       = uid;
  member->status    = status;
  member->join_time = time(0);

  if (list_append_data(clan->members,member)<0)
  {
     eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
     free((void *)member);
     return -1;
  }

  return 0;
}

t_clan * create_clan(int chieftain_uid, char clanshort[4], char * clanname, char * motd)
{
  t_clan * clan;
  t_clanmember * member;

  if (!(clan = malloc(sizeof(t_clan))))
  {
     eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for new clan");
     return NULL;
  }

  if (!(member = malloc(sizeof(t_clanmember))))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clanmember");
    free((void *)clan);
    return NULL;
  }

  if (!(clanname))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL clanname");
    free((void *)clan);
    free((void *)member);
    return NULL;
  }

  if (!(clan->clanname = strdup(clanname)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clanname");
    free((void *)clan);
    free((void *)member);
    return NULL;
  }

  if (!(motd))
  {
    clan->clan_motd = strdup("This is a newly created clan");
  }
  else
  {
    clan->clan_motd = strdup(motd);
  }
  if (!(clan->clan_motd))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clan_motd");
    free((void *)clan->clanname);
    free((void *)member);
    free((void *)clan);
    return NULL;
  }

  clan->creation_time = time(0);

  memcpy(clan->clanshort,clanshort,4);

  clan->clanid=++max_clanid;

  if ((clan->members = list_create())<0)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not create memberlist");
    free((void *)clan->clanname);
    free((void *)clan->clan_motd);
    free((void *)member);
    free((void *)clan);
    return NULL;
  }

  member->uid       = chieftain_uid;
  member->status    = CLAN_CHIEFTAIN;
  member->join_time = clan->creation_time;

  if (list_append_data(clan->members,member)<0)
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
    free((void *)clan->clanname);
    free((void *)clan->clan_motd);
    free((void *)member);
    list_destroy(clan->members);
    free((void *)clan);
    return NULL;
  }

  return clan;
}
