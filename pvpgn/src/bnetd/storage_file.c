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
#define ACCOUNT_INTERNAL_ACCESS
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
#include "compat/pdir.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"
#include "common/field_sizes.h"
#include "common/bnethash.h"
#ifdef WITH_BITS
# include "connection.h"
# include "bits_va.h"
# include "bits.h"
#endif
#include "common/introtate.h"
#include "account.h"
#include "common/hashtable.h"
#include "storage.h"
#include "common/list.h"
#include "connection.h"
#include "watch.h"
#include "common/tag.h"
#include "common/setup_after.h"

/* file storage API functions */

static int file_init(const char *);
static int file_close(void);
static t_storage_info * file_create_account(char const *);
static t_storage_info * file_get_defacct(void);
static int file_free_info(t_storage_info *);
static int file_read_attrs(t_storage_info *, t_read_attr_func, void *);
static int file_write_attrs(t_storage_info *, void *);
static int file_read_accounts(t_read_accounts_func, void *);
static int file_cmp_info(t_storage_info *, t_storage_info *);
static const char * file_escape_key(const char *);

/* storage struct populated with the functions above */

t_storage storage_file = {
    file_init,
    file_close,
    file_create_account,
    file_get_defacct,
    file_free_info,
    file_read_attrs,
    file_write_attrs,
    file_read_accounts,
    file_cmp_info,
    file_escape_key
};

/* start of actual file storage code */

static const char *accountsdir = NULL;

static int file_init(const char *path)
{
    if (path == NULL || path[0] == '\0') {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL or empty account dir path");
	return -1;
    }

    if (accountsdir) file_close();

    if ((accountsdir = strdup(path)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory to store accounts dir");
	return -1;
    }

    return 0;
}

static int file_close(void)
{
    if (accountsdir) free((void*)accountsdir);
    accountsdir = NULL;
}

static t_storage_info * file_create_account(const char * username)
{
    t_storage_info *info;
    char *temp;

    if (accountsdir == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "file storage not initilized");
	return NULL;
    }

    if ((info = malloc(sizeof(t_storage_info))) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for storage info");
	return NULL;
    }

    info = NULL;
    if (prefs_get_savebyname())
    {
	char const * safename;

	if (!(safename = escape_fs_chars(username,strlen(username))))
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not escape username");
	    file_free_info(info);
	    return NULL;
	}
	if (!(temp = malloc(strlen(accountsdir)+1+strlen(safename)+1))) /* dir + / + name + NUL */
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for temp");
	    file_free_info(info);
	    return NULL;
	}
	sprintf(temp,"%s/%s",accountsdir,safename);
	free((void *)safename); /* avoid warning */
    } else {
	if (!(temp = malloc(strlen(accountsdir)+1+8+1))) /* dir + / + uid + NUL */
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for temp");
	    file_free_info(info);
	    return NULL;
	}
	sprintf(temp,"%s/%06u",accountsdir,maxuserid+1); /* FIXME: hmm, maybe up the %06 to %08... */
    }
    info = temp;

    return info;
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
	    eventlog(eventlog_level_error, __FUNCTION__, "could not delete account file \"%s\" (remove: %s)", info, strerror(errno));
            free(tempname);
	    return -1;
	}
    }
#endif
   
    if (rename(tempname,(const char *)info)<0) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not rename account file to \"%s\" (rename: %s)", info, strerror(errno));
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

    eventlog(eventlog_level_debug, __FUNCTION__, "loading \"%s\"",info);
    if (!(accountfile = fopen((const char *)info,"r"))) {
	eventlog(eventlog_level_error, __FUNCTION__,"could not open account file \"%s\" for reading (fopen: %s)", info, strerror(errno));
	return -1;
    }
    
    for (line=1; (buff=file_get_line(accountfile)); line++) {
	if (buff[0]=='#' || buff[0]=='\0') {
	    free((void *)buff); /* avoid warning */
	    continue;
	}

	if (strlen(buff)<6) /* "?"="" */ {
	    eventlog(eventlog_level_error, __FUNCTION__, "malformed line %d of account file \"%s\"", line, info);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	
	len = strlen(buff)-5+1; /* - ""="" + NUL */
	if (!(esckey = malloc(len))) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for esckey on line %d of account file \"%s\"", line, info);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	if (!(escval = malloc(len))) {
	    eventlog(eventlog_level_error, __FUNCTION__,"could not allocate memory for escval on line %d of account file \"%s\"", line, info);
	    free((void *)buff); /* avoid warning */
	    free(esckey);
	    continue;
	}
	
	if (sscanf(buff,"\"%[^\"]\" = \"%[^\"]\"",esckey,escval)!=2) {
	    if (sscanf(buff,"\"%[^\"]\" = \"\"",esckey)!=1) /* hack for an empty value field */ {
		eventlog(eventlog_level_error, __FUNCTION__,"malformed entry on line %d of account file \"%s\"", line, info);
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
	eventlog(eventlog_level_error, __FUNCTION__, "could not close account file \"%s\" after reading (fclose: %s)", info, strerror(errno));

    return 0;
}

static int file_free_info(t_storage_info *info)
{
    if (info) free((void*)info);
    return 0;
}

static t_storage_info * file_get_defacct(void)
{
    const char * str;
    t_storage_info * info;

    if ((info = strdup(prefs_get_defacct())) == NULL) {
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
    t_storage_info *info;

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

	if ((info = malloc(sizeof(t_storage_info))) == NULL) {
	   eventlog(eventlog_level_error, __FUNCTION__, "could not allocate memory for storage path");
	   continue;
	}
	info = pathname;

	cb(info, data);
    }

    if (p_closedir(accountdir)<0)
	eventlog(eventlog_level_error,"accountlist_reload","unable to close user directory \"%s\" (p_closedir: %s)", accountsdir,strerror(errno));

    return 0;
}

static int file_cmp_info(t_storage_info *info1, t_storage_info *info2)
{
    return strcmp((const char *)info1, (const char *)info2);
}

static const char * file_escape_key(const char * key)
{
    return key;
}
