/*
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2002,2003 Dizzy 
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
#include "compat/strchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include <ctype.h>
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include "compat/char_bit.h"
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
#include <errno.h>
#include "compat/strerror.h"
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
# ifdef WIN32
#  include <io.h>
# endif
#endif
#include "compat/pdir.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"
#include "common/field_sizes.h"
#include "common/bnethash.h"
#define CLAN_INTERNAL_ACCESS
#define ACCOUNT_INTERNAL_ACCESS
#include "common/introtate.h"
#include "account.h"
#include "common/hashtable.h"
#include "storage.h"
#include "common/list.h"
#include "connection.h"
#include "watch.h"
#include "clan.h"
#undef ACCOUNT_INTERNAL_ACCESS
#undef CLAN_INTERNAL_ACCESS
#include "common/tag.h"
#include "common/setup_after.h"

/* file storage API functions */

static int file_init(const char *);
static int file_close(void);
static t_storage_info * file_create_account(char const *);
static t_storage_info * file_get_defacct(void);
static int file_free_info(t_storage_info *);
static int file_read_attrs(t_storage_info *, t_read_attr_func, void *);
static void * file_read_attr(t_storage_info *, const char *);
static int file_write_attrs(t_storage_info *, void *);
static int file_read_accounts(t_read_accounts_func, void *);
static void * file_read_account(t_read_account_func, const char *);
static int file_cmp_info(t_storage_info *, t_storage_info *);
static const char * file_escape_key(const char *);
static int file_load_clans(t_load_clans_func);
static int file_write_clan(void *);
static int file_remove_clan(int);
static int file_remove_clanmember(int);

/* storage struct populated with the functions above */

t_storage storage_file = {
    file_init,
    file_close,
    file_create_account,
    file_get_defacct,
    file_free_info,
    file_read_attrs,
    file_write_attrs,
    file_read_attr,
    file_read_accounts,
    file_read_account,
    file_cmp_info,
    file_escape_key,
    file_load_clans,
    file_write_clan,
    file_remove_clan,
    file_remove_clanmember
};

/* start of actual file storage code */

static const char *accountsdir = NULL;
static const char *clansdir = NULL;
static const char *defacct = NULL;

static int file_init(const char *path)
{
    char *tok, *copy, *tmp, *p;
    const char *dir = NULL;
    const char *clan = NULL;
    const char *def = NULL;

    if (path == NULL || path[0] == '\0') {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL or empty path");
	return -1;
    }

    if ((copy = strdup(path)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not duplicate path");
	return -1;
    }

    tmp = copy;
    while((tok = strtok(tmp, ";")) != NULL) {
	tmp = NULL;
	if ((p = strchr(tok, '=')) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "invalid storage_path, no '=' present in token");
	    free((void*)copy);
	    return -1;
	}
	*p = '\0';
	if (strcasecmp(tok, "dir") == 0)
	    dir = p + 1;
    else if (strcasecmp(tok, "clan") == 0)
	    clan = p + 1;
	else if (strcasecmp(tok, "default") == 0)
	    def = p + 1;
	else eventlog(eventlog_level_warn, __FUNCTION__, "unknown token in storage_path : '%s'", tok);
    }

    if (def == NULL || clan == NULL || dir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "invalid storage_path line for file module (doesnt have a 'dir', a 'clan' and a 'default' token)");
	free((void*)copy);
	return -1;
    }

    if (accountsdir) file_close();

    if ((accountsdir = strdup(dir)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory to store accounts dir");
	free((void*)copy);
	return -1;
    }

    if ((clansdir = strdup(clan)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory to store clans dir");
	free((void*)copy);
	return -1;
    }

    if ((defacct = strdup(def)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory to store default account path");
	free((void*)accountsdir); accountsdir = NULL;
	free((void*)copy);
	return -1;
    }

    free((void*)copy);

    return 0;
}

static int file_close(void)
{
    if (accountsdir) free((void*)accountsdir);
    accountsdir = NULL;

    if (clansdir) free((void*)clansdir);
    clansdir = NULL;

    if (defacct) free((void*)defacct);
    defacct = NULL;

    return 0;
}

static t_storage_info * file_create_account(const char * username)
{
    char *temp;

    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return NULL;
    }

    if (prefs_get_savebyname())
    {
	char const * safename;

	if (!(safename = escape_fs_chars(username,strlen(username))))
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not escape username");
	    return NULL;
	}
	if (!(temp = malloc(strlen(accountsdir)+1+strlen(safename)+1))) /* dir + / + name + NUL */
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for temp");
	    free((void *)safename);
	    return NULL;
	}
	sprintf(temp,"%s/%s",accountsdir,safename);
	free((void *)safename); /* avoid warning */
    } else {
	if (!(temp = malloc(strlen(accountsdir)+1+8+1))) /* dir + / + uid + NUL */
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for temp");
	    return NULL;
	}
	sprintf(temp,"%s/%06u",accountsdir,maxuserid+1); /* FIXME: hmm, maybe up the %06 to %08... */
    }

    return temp;
}

static int file_write_attrs(t_storage_info *info, void *attributes)
{
    FILE *        accountfile;
    t_attribute * attr;
    char const *  key;
    char const *  val;
    char *        tempname;

    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return -1;
    }

    if (info == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL info storage");
	return -1;
    }

    if (attributes == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attributes");
	return -1;
    }

    if (!(tempname = malloc(strlen(accountsdir)+1+strlen(BNETD_ACCOUNT_TMP)+1))) {
	eventlog(eventlog_level_error, __FUNCTION__, "unable to allocate memory for tempname");
        return -1;
    }

    sprintf(tempname,"%s/%s", accountsdir, BNETD_ACCOUNT_TMP);

    if (!(accountfile = fopen(tempname,"w"))) {
	eventlog(eventlog_level_error, __FUNCTION__, "unable to open file \"%s\" for writing (fopen: %s)",tempname,strerror(errno));
	free(tempname);
	return -1;
    }
   
    for (attr=(t_attribute *)attributes; attr; attr=attr->next) {
	if (attr->key)
	    key = escape_chars(attr->key,strlen(attr->key));
	else {
	    eventlog(eventlog_level_error, __FUNCTION__, "attribute with NULL key in list");
	    key = NULL;
	}

	if (attr->val)
	    val = escape_chars(attr->val,strlen(attr->val));
	else {
	    eventlog(eventlog_level_error, __FUNCTION__, "attribute with NULL val in list");
	    val = NULL;
	}

	if (key && val) {
	    if (strncmp("BNET\\CharacterDefault\\", key, 20) == 0) {
		eventlog(eventlog_level_debug, __FUNCTION__, "skipping attribute key=\"%s\"",attr->key);
	    } else {
		eventlog(eventlog_level_debug, __FUNCTION__, "saving attribute key=\"%s\" val=\"%s\"",attr->key,attr->val);
		fprintf(accountfile,"\"%s\"=\"%s\"\n",key,val);
	    }
	} else eventlog(eventlog_level_error, __FUNCTION__,"could not save attribute key=\"%s\"",attr->key);

	if (key) free((void *)key); /* avoid warning */
	if (val) free((void *)val); /* avoid warning */
    }

    if (fclose(accountfile)<0) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not close account file \"%s\" after writing (fclose: %s)",tempname,strerror(errno));
	free(tempname);
	return -1;
    }
   
#ifdef WIN32
   /* We are about to rename the temporary file
    * to replace the existing account.  In Windows,
    * we have to remove the previous file or the
    * rename function will fail saying the file
    * already exists.  This defeats the purpose of
    * the rename which was to make this an atomic
    * operation.  At least the race window is small.
    */
    if (access((const char *)info, 0) == 0) {
	if (remove((const char *)info)<0) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not delete account file \"%s\" (remove: %s)", (char *)info, strerror(errno));
            free(tempname);
	    return -1;
	}
    }
#endif
   
    if (rename(tempname,(const char *)info)<0) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not rename account file to \"%s\" (rename: %s)", (char *)info, strerror(errno));
	free(tempname);
	return -1;
    }

    free(tempname);

    return 0;
}

static int file_read_attrs(t_storage_info *info, t_read_attr_func cb, void *data)
{
    FILE *       accountfile;
    unsigned int line;
    char const * buff;
    unsigned int len;
    char *       esckey;
    char *       escval;
    char * key;
    char * val;
    
    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return -1;
    }

    if (info == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL info storage");
	return -1;
    }

    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL callback");
	return -1;
    }

    eventlog(eventlog_level_debug, __FUNCTION__, "loading \"%s\"",(char *)info);
    if (!(accountfile = fopen((const char *)info,"r"))) {
	eventlog(eventlog_level_error, __FUNCTION__,"could not open account file \"%s\" for reading (fopen: %s)", (char *)info, strerror(errno));
	return -1;
    }
    
    for (line=1; (buff=file_get_line(accountfile)); line++) {
	if (buff[0]=='#' || buff[0]=='\0') {
	    free((void *)buff); /* avoid warning */
	    continue;
	}

	if (strlen(buff)<6) /* "?"="" */ {
	    eventlog(eventlog_level_error, __FUNCTION__, "malformed line %d of account file \"%s\"", line, (char *)info);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	
	len = strlen(buff)-5+1; /* - ""="" + NUL */
	if (!(esckey = malloc(len))) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for esckey on line %d of account file \"%s\"", line, (char *)info);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	if (!(escval = malloc(len))) {
	    eventlog(eventlog_level_error, __FUNCTION__,"could not allocate memory for escval on line %d of account file \"%s\"", line, (char *)info);
	    free((void *)buff); /* avoid warning */
	    free(esckey);
	    continue;
	}
	
	if (sscanf(buff,"\"%[^\"]\" = \"%[^\"]\"",esckey,escval)!=2) {
	    if (sscanf(buff,"\"%[^\"]\" = \"\"",esckey)!=1) /* hack for an empty value field */ {
		eventlog(eventlog_level_error, __FUNCTION__,"malformed entry on line %d of account file \"%s\"", line, (char *)info);
		free(escval);
		free(esckey);
		free((void *)buff); /* avoid warning */
		continue;
	    }
	    escval[0] = '\0';
	}
	free((void *)buff); /* avoid warning */
	
	key = unescape_chars(esckey);
	val = unescape_chars(escval);

/* eventlog(eventlog_level_debug,"account_load_attrs","strlen(esckey)=%u (%c), len=%u",strlen(esckey),esckey[0],len);*/
	free(esckey);
	free(escval);
	
	if (cb(key,val,data))
	    eventlog(eventlog_level_error, __FUNCTION__, "got error from callback (key: '%s' val:'%s')", key, val);

	if (key) free((void *)key); /* avoid warning */
	if (val) free((void *)val); /* avoid warning */
    }


    if (fclose(accountfile)<0) 
	eventlog(eventlog_level_error, __FUNCTION__, "could not close account file \"%s\" after reading (fclose: %s)", (char *)info, strerror(errno));

    return 0;
}

static void * file_read_attr(t_storage_info *info, const char *key)
{
    /* flat file storage doesnt know to read selective attributes */
    return NULL;
}

static int file_free_info(t_storage_info *info)
{
    if (info) free((void*)info);
    return 0;
}

static t_storage_info * file_get_defacct(void)
{
    t_storage_info * info;

    if (defacct == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return NULL;
    }

    if ((info = strdup(defacct)) == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "could not duplicate default account filename");
	return NULL;
    }

    return info;
}

static int file_read_accounts(t_read_accounts_func cb, void *data)
{
    char const * dentry;
    char *       pathname;
    t_pdir *     accountdir;

    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return -1;
    }

    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL callback");
	return -1;
    }

    if (!(accountdir = p_opendir(accountsdir))) {
	eventlog(eventlog_level_error, __FUNCTION__, "unable to open user directory \"%s\" for reading (p_opendir: %s)",accountsdir,strerror(errno));
	return -1;
    }

    while ((dentry = p_readdir(accountdir))) {
	if (dentry[0]=='.') continue;

	if (!(pathname = malloc(strlen(accountsdir)+1+strlen(dentry)+1))) /* dir + / + file + NUL */
	 {
	   eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for pathname");
	   continue;
	 }
	sprintf(pathname,"%s/%s", accountsdir, dentry);

	cb(pathname, data);
    }

    if (p_closedir(accountdir)<0)
	eventlog(eventlog_level_error, __FUNCTION__,"unable to close user directory \"%s\" (p_closedir: %s)", accountsdir,strerror(errno));

    return 0;
}

static void * file_read_account(t_read_account_func cb, const char * accname)
{
    char *       pathname;

    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return NULL;
    }

    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL callback");
	return NULL;
    }

    if (!(pathname = malloc(strlen(accountsdir)+1+strlen(accname)+1))) /* dir + / + file + NUL */
    {
	eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for pathname");
	return NULL;
    }
    sprintf(pathname,"%s/%s", accountsdir, accname);
    if (access(pathname,0)) /* if it doesn't exist */
    {
	free((void *) pathname);
	return NULL;
    }

    return cb(pathname);
}

static int file_cmp_info(t_storage_info *info1, t_storage_info *info2)
{
    return strcmp((const char *)info1, (const char *)info2);
}

static const char * file_escape_key(const char * key)
{
    return key;
}

static int file_load_clans(t_load_clans_func cb)
{
  char const	*dentry;
  t_pdir	*clandir;
  char		*pathname;
  int       clantag;
  t_clan	*clan;
  FILE		*fp;
  char		clanname[CLAN_NAME_MAX+1];
  char		motd[51];
  int		cid, creation_time;
  int		member_uid, member_join_time;
  char		member_status;
  t_clanmember	*member;
  
    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "get NULL callback");
	return -1;
    }

  if (!(clandir = p_opendir(clansdir)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"unable to open clan directory \"%s\" for reading (p_opendir: %s)",clansdir,strerror(errno));
      return -1;
    }
  eventlog(eventlog_level_trace,__FUNCTION__,"start reading clans");

  if (!(pathname = malloc(strlen(clansdir)+1+4+1)))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for pathname");
	  return -1;
	}

    while ((dentry = p_readdir(clandir)) != NULL)
    {
      if (dentry[0]=='.') continue;
    
      sprintf(pathname,"%s/%s",clansdir,dentry);

      if (strlen(dentry)>4)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"found too long clan filename in clandir \"%s\"",dentry);
	  continue;
	}
    
      clantag = str_to_clantag(dentry);

  if ((fp = fopen(pathname,"r"))==NULL)
    {
      eventlog(eventlog_level_error,__FUNCTION__,"can't open clanfile \"%s\"",pathname);
      continue;
    }

  if(!(clan = malloc(sizeof(t_clan))))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for clan");
    free((void *)pathname);
    p_closedir(clandir);
    return -1;
  }

  clan->clantag=clantag;

  fscanf(fp,"\"%[^\"]\",\"%[^\"]\",%i,%i\n",clanname,motd,&cid,&creation_time);
  clan->clanname = strdup(clanname);
  clan->clan_motd = strdup(motd);
  clan->clanid = cid;
  clan->creation_time=(time_t)creation_time;
  clan->created = 1;
  clan->modified = 0;
  clan->channel_type = prefs_get_clan_channel_default_private();
 
  eventlog(eventlog_level_trace,__FUNCTION__,"name: %s motd: %s clanid: %i time: %i",clanname,motd,cid,creation_time);

  clan->members = list_create();

  while (fscanf(fp,"%i,%c,%i\n",&member_uid,&member_status,&member_join_time)==3)
    {
      if (!(member = malloc(sizeof(t_clanmember))))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"cannot allocate memory for clan member");
	  clan_remove_all_members(clan);
      if(clan->clanname)
	    free((void *)clan->clanname);
      if(clan->clan_motd)
        free((void *)clan->clan_motd);
	  free((void *)clan);
      fclose(fp);
      free((void *)pathname);
      p_closedir(clandir);
	  return -1;
	}
      if(!(member->memberacc   = accountlist_find_account_by_uid(member_uid)))
      {
	    eventlog(eventlog_level_error,__FUNCTION__,"cannot find uid %u", member_uid);
	    free((void *)member);
	    continue;
      }
      member->memberconn  = NULL;
      member->status    = member_status-'0';
      member->join_time = member_join_time;
      member->clan = clan;

      if((member->status==CLAN_NEW)&&(time(NULL)-member->join_time>prefs_get_clan_newer_time()*3600))
      {
          member->status = CLAN_PEON;
          clan->modified = 1;
      }

      if (list_append_data(clan->members,member)<0)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
	  free((void *)member);
	  clan_remove_all_members(clan);
      if(clan->clanname)
	    free((void *)clan->clanname);
      if(clan->clan_motd)
	    free((void *)clan->clan_motd);
	  free((void *)clan);
      fclose(fp);
      free((void *)pathname);
      p_closedir(clandir);
	  return -1;
	}
      account_set_clanmember(member->memberacc, member);
      eventlog(eventlog_level_trace,__FUNCTION__,"added member: uid: %i status: %c join_time: %i",member_uid,member_status+'0',member_join_time);
    }

  fclose(fp);

      cb(clan);

    }

      free((void *)pathname);

  if (p_closedir(clandir)<0)
    {
      eventlog(eventlog_level_error,__FUNCTION__,"unable to close clan directory \"%s\" (p_closedir: %s)",clansdir,strerror(errno));
    }
  eventlog(eventlog_level_trace,__FUNCTION__,"finished reading clans");

  return 0;
}

static int file_write_clan(void * data)
{
  FILE			*fp;
  t_elem		*curr;
  t_clanmember	*member;
  char			*clanfile;
  t_clan        *clan=(t_clan *)data;
  if (!(clanfile = malloc(strlen(clansdir)+1+4+1)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for filename");
      return -1;
    }

  sprintf(clanfile,"%s/%c%c%c%c",clansdir,
	  clan->clantag>>24,(clan->clantag>>16)&0xff,
	  (clan->clantag>>8)&0xff,clan->clantag&0xff);

  if ((fp = fopen(clanfile,"w"))==NULL)
    {
      eventlog(eventlog_level_error,__FUNCTION__,"can't open clanfile \"%s\"",clanfile);
      free((void *)clanfile);
      return -1;
    }

  fprintf(fp,"\"%s\",\"%s\",%i,%i\n",clan->clanname,clan->clan_motd,clan->clanid,(int)clan->creation_time);

  LIST_TRAVERSE(clan->members,curr)
    {
      if (!(member = elem_get_data(curr)))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
	  continue;
	}
    if((member->status==CLAN_NEW)&&(time(NULL)-member->join_time>prefs_get_clan_newer_time()*3600))
      member->status = CLAN_PEON;
    fprintf(fp,"%i,%c,%u\n",account_get_uid(member->memberacc),member->status+'0',(unsigned)member->join_time);
    }

  fclose(fp);
  free((void *)clanfile);
  return 0;
}

static int file_remove_clan(int clantag)
{
    char * tempname;
    if (!(tempname = malloc(strlen(clansdir)+1+4+1)))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for pathname");
	  return -1;
	}
    sprintf(tempname, "%s/%c%c%c%c", clansdir, clantag>>24, (clantag>>16)&0xff, (clantag>>8)&0xff, clantag&0xff);
	if (remove((const char *)tempname)<0) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not delete clan file \"%s\" (remove: %s)", (char *)tempname, strerror(errno));
        free(tempname);
	    return -1;
	}
    free(tempname);
    return 0;
}

static int file_remove_clanmember(int uid)
{
    return 0;
}
