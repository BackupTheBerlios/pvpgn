/*
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2002 Dizzy 
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
#include "common/setup_after.h"
#include "storage.h"
#include "common/list.h"
#include "connection.h"
//aaron
#include "war3ladder.h"

static t_hashtable * accountlist_head=NULL;

static t_account * default_acct=NULL;
static unsigned int maxuserid=0;

/* This is to force the creation of all accounts when we initially load the accountlist. */
static int force_account_add=0;

static unsigned int account_hash(char const * username);
static int account_insert_attr(t_account * account, char const * key, char const * val);
#ifndef WITH_MYSQL
static t_account * account_load(char const * filename);
#else
static t_account * account_load(unsigned int sid);
#endif
static int account_load_attrs(t_account * account);
static void account_unload_attrs(t_account * account);
static int account_check_name(char const * name);


static unsigned int account_hash(char const * username)
{
    unsigned int i;
    unsigned int pos;
    unsigned int hash;
    unsigned int ch;
    
    if (!username)
    {
	eventlog(eventlog_level_error,"account_hash","got NULL username");
	return 0;
    }
    
    hash = (strlen(username)+1)*120343021;
    for (pos=0,i=0; i<strlen(username); i++)
    {
	if (isascii((int)username[i]) && isupper((int)username[i]))
	    ch = (unsigned int)(unsigned char)tolower((int)username[i]);
	else
	    ch = (unsigned int)(unsigned char)username[i];
	hash ^= ROTL(ch,pos,sizeof(unsigned int)*CHAR_BIT);
        hash ^= ROTL((i+1)*314159,ch&0x1f,sizeof(unsigned int)*CHAR_BIT);
	pos += CHAR_BIT-1;
    }
    
    return hash;
}


extern t_account * account_create(char const * username, char const * passhash1)
{
    t_account * account;
    
    if (username && account_check_name(username)<0)
    { 
        eventlog(eventlog_level_error,"account_create","got bad account name"); 
        return NULL; 
    } 

    if (!(account = malloc(sizeof(t_account))))
    {
	eventlog(eventlog_level_error,"account_create","could not allocate memory for account");
	return NULL;
    }

    account->name     = NULL;
#ifndef WITH_MYSQL
    account->filename = NULL;
#else
    account->storageid= 0;
#endif
    account->attrs    = NULL;
    account->dirty    = 0;
    account->accessed = 0;
    account->age      = 0;
    
    account->namehash = 0; /* hash it later before inserting */
    account->uid      = 0; /* hash it later before inserting */

    if (username) /* actually making a new account */
    {
	char * temp;
	
	if (account_check_name(username)<0)
	{
	    eventlog(eventlog_level_error,"account_create","invalid account name \"%s\"",username);
	    account_destroy(account);
	    return NULL;
	}
	if (!passhash1)
	{
	    eventlog(eventlog_level_error,"account_create","got NULL passhash1");
	    account_destroy(account);
	    return NULL;
	}

#ifdef WITH_MYSQL
	account->storageid =  storage_create_account(username);
	if(!account->storageid) {
	    eventlog(eventlog_level_error,"account_create","failed to add user to db");
	    account_destroy(account);
	    return NULL;
	}
#else
	if (prefs_get_savebyname())
	{
	    char const * safename;
	    
	    if (!(safename = escape_fs_chars(username,strlen(username))))
	    {
		eventlog(eventlog_level_error,"account_create","could not escape username");
		account_destroy(account);
		return NULL;
	    }
	    if (!(temp = malloc(strlen(prefs_get_userdir())+1+strlen(safename)+1))) /* dir + / + name + NUL */
	    {
		eventlog(eventlog_level_error,"account_create","could not allocate memory for temp");
		account_destroy(account);
		return NULL;
	    }
	    sprintf(temp,"%s/%s",prefs_get_userdir(),safename);
	    free((void *)safename); /* avoid warning */
	}
	else
	{
	    if (!(temp = malloc(strlen(prefs_get_userdir())+1+8+1))) /* dir + / + uid + NUL */
	    {
		eventlog(eventlog_level_error,"account_create","could not allocate memory for temp");
		account_destroy(account);
		return NULL;
	    }
	    sprintf(temp,"%s/%06u",prefs_get_userdir(),maxuserid+1); /* FIXME: hmm, maybe up the %06 to %08... */
	}
	account->filename = temp;
	
#endif
        account->loaded = 1;
	
	if ((account->name = strdup(username)) == NULL) {
	    eventlog(eventlog_level_error, "account_create", "could not duplicate username to cache it");
	    account_destroy(account);
	    return NULL;
	}
	
	if (account_set_strattr(account,"BNET\\acct\\username",username)<0)
	{
	    eventlog(eventlog_level_error,"account_create","could not set username");
	    account_destroy(account);
	    return NULL;
	}
	if (account_set_numattr(account,"BNET\\acct\\userid",maxuserid+1)<0)
	{
	    eventlog(eventlog_level_error,"account_create","could not set userid");
	    account_destroy(account);
	    return NULL;
	}
	if (account_set_strattr(account,"BNET\\acct\\passhash1",passhash1)<0)
	{
	    eventlog(eventlog_level_error,"account_create","could not set passhash1");
	    account_destroy(account);
	    return NULL;
	}
	
#ifdef WITH_BITS
	account_set_bits_state(account,account_state_valid);
	if (!bits_master)
	    eventlog(eventlog_level_warn,"account_create","account_create should not be called on BITS clients");
#endif
    }
    else /* empty account to be filled in later */
    {
	account->loaded   = 0;
#ifdef WITH_BITS
	account_set_bits_state(account,account_state_valid);
#endif
    }
    
#ifdef WITH_MYSQL
    /* because the server crashes it doesnt get to save new accounts
     * so we get lots of NULL accounts, lets try to flush them here
     * it should remove this problem
     */
     account_save(account, 1000); /* big delta to force unload */
#endif

    return account;
}

#ifdef WITH_BITS
extern t_account * create_vaccount(const char *username, unsigned int uid)
{
    /* this is a modified(?) version of account_create */
    t_account * account;
    
    account = malloc(sizeof(t_account));
    if (!account)
    {
	eventlog(eventlog_level_error,"create_vaccount","could not allocate memory for account");
	return NULL;
    }
    account->attrs    = NULL;
    account->dirty    = 0;
    account->accessed = 0;
    account->age      = 0;
    
    account->namehash = 0; /* hash it later */
    account->uid      = 0; /* uid? */
    
    account->filename = NULL;	/* there is no local account file */
    account->loaded   = 0;	/* the keys are not yet loaded */
    account->bits_state = account_state_unknown;
    
    if (username)
    {
	if (username[0]!='#') {
	    if (strchr(username,' '))
	    {
	        eventlog(eventlog_level_error,"create_vaccount","username contains spaces");
	        account_destroy(account);
	        return NULL;
	    }
	    if (strlen(username)>=USER_NAME_MAX)
	    {
	        eventlog(eventlog_level_error,"create_vaccount","username \"%s\" is too long (%u chars)",username,strlen(username));
	        account_destroy(account);
	        return NULL;
	    }
	    account_set_strattr(account,"BNET\\acct\\username",username);
            account->namehash = account_hash(username);
	} else {
	    if (str_to_uint(&username[1],&account->uid)<0) {
		eventlog(eventlog_level_warn,"create_vaccount","invalid username \"%s\"",username);
	    }
	}
    }
    account_set_numattr(account,"BNET\\acct\\userid",account->uid);
    return account;
}
#endif

static void account_unload_attrs(t_account * account)
{
    t_attribute const * attr;
    t_attribute const * temp;
    
/*    eventlog(eventlog_level_debug,"account_unload_attrs","unloading \"%s\"",account->filename);*/
    if (!account)
    {
	eventlog(eventlog_level_error,"account_unload_attrs","got NULL account");
	return;
    }
    
    for (attr=account->attrs; attr; attr=temp)
    {
	if (attr->key)
	    free((void *)attr->key); /* avoid warning */
	if (attr->val)
	    free((void *)attr->val); /* avoid warning */
        temp = attr->next;
	free((void *)attr); /* avoid warning */
    }
    account->attrs = NULL;
    if (account->name) {
	free(account->name);
	account->name = NULL;
    }
    account->loaded = 0;
}

extern void account_destroy(t_account * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_destroy","got NULL account");
	return;
    }
    account_unload_attrs(account);
#ifndef WITH_MYSQL
    if (account->filename)
	free((void *)account->filename); /* avoid warning */
#endif
    free(account);
}


extern unsigned int account_get_uid(t_account const * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_uid","got NULL account");
	return -1;
    }
/* FIXME: uids should be treated the same */
#ifndef WITH_MYSQL    
    return account->uid;
#else
    return account->storageid;
#endif
}


extern int account_match(t_account * account, char const * username)
{
    unsigned int userid=0;
    unsigned int namehash;
    char const * tname;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_match","got NULL account");
	return -1;
    }
    if (!username)
    {
	eventlog(eventlog_level_error,"account_match","got NULL username");
	return -1;
    }
    
    if (username[0]=='#')
        if (str_to_uint(&username[1],&userid)<0)
            userid = 0;
    
    if (userid)
    {
    /* FIXME: treat uids the same */
#ifndef WITH_MYSQL
        if (account->uid==userid)
#else
	if (account->storageid==userid)
#endif
            return 1;
    }
    else
    {
	namehash = account_hash(username);
        if (account->namehash==namehash &&
	    (tname = account_get_name(account)))
	{
	    if (strcasecmp(tname,username)==0)
	    {
		account_unget_name(tname);
		return 1;
	    }
	    else
	      { account_unget_name(tname); }
	}
    }
    
    return 0;
}


extern int account_save(t_account * account, unsigned int delta)
{
#ifndef WITH_MYSQL
   FILE *        accountfile;
   t_attribute * attr;
   char const *  key;
   char const *  val;
   char *        tempname;
#endif
   
   if (!account)
     {
	eventlog(eventlog_level_error,"account_save","got NULL account");
	return -1;
     }

   
   /* account aging logic */
   if (account->accessed)
     account->age =  0;
   else
     account->age += delta;
   if (account->age>( (3*prefs_get_user_flush_timer()) >>1))
     account->age = ( (3*prefs_get_user_flush_timer()) >>1);
   account->accessed = 0;
#ifdef WITH_BITS
   /* We do not have to save the account information to disk if we are a BITS client */
   if (!bits_master) {
      if (account->age>=prefs_get_user_flush_timer())
	{
	   if (!connlist_find_connection_by_accountname(account_get_name(account)))
	     {
		account_set_bits_state(account,account_state_delete);
		/*	account_set_locked(account,3);  To be deleted */
		/*	bits_va_unlock_account(account); */
	     }
	}
      return 0;
   }
#endif
   
#ifndef WITH_MYSQL
   if (!account->filename)
     {
# ifdef WITH_BITS
	if (!bits_master)
	  return 0; /* It's OK since we don't have the files on the bits clients */
# endif
	eventlog(eventlog_level_error,"account_save","account "UID_FORMAT" has NULL filename",account->uid);
	return -1;
     }
#endif
   
   if (!account->loaded)
     return 0;
   
   if (!account->dirty)
     {
	if (account->age>=prefs_get_user_flush_timer())
	  account_unload_attrs(account);
	return 0;
     }
   
#ifndef WITH_MYSQL
   if (!(tempname = malloc(strlen(prefs_get_userdir())+1+strlen(BNETD_ACCOUNT_TMP)+1)))
     {
	eventlog(eventlog_level_error,"account_save","uable to allocate memory for tempname");
	return -1;
     }
   
   sprintf(tempname,"%s/%s",prefs_get_userdir(),BNETD_ACCOUNT_TMP);
   
   if (!(accountfile = fopen(tempname,"w")))
     {
	eventlog(eventlog_level_error,"account_save","unable to open file \"%s\" for writing (fopen: %s)",tempname,strerror(errno));
	free(tempname);
	return -1;
     }
   
    for (attr=account->attrs; attr; attr=attr->next)
     {
	if (attr->key)
	  key = escape_chars(attr->key,strlen(attr->key));
	else
	  {
	     eventlog(eventlog_level_error,"account_save","attribute with NULL key in list");
	     key = NULL;
	  }
	if (attr->val)
	  val = escape_chars(attr->val,strlen(attr->val));
	else
	  {
	     eventlog(eventlog_level_error,"account_save","attribute with NULL val in list");
	     val = NULL;
	  }
	if (key && val)
	  {
	     if (strncmp("BNET\\CharacterDefault\\", key, 20) == 0)
	       {
		  eventlog(eventlog_level_debug,"account_save","skipping attribute key=\"%s\"",attr->key);
	       }
	     else
	       {
		  eventlog(eventlog_level_debug,"account_save","saving attribute key=\"%s\" val=\"%s\"",attr->key,attr->val);
		  fprintf(accountfile,"\"%s\"=\"%s\"\n",key,val);
	       }
	}
	else
	  eventlog(eventlog_level_error,"account_save","could not save attribute key=\"%s\"",attr->key);
	if (key)
	  free((void *)key); /* avoid warning */
	if (val)
	  free((void *)val); /* avoid warning */
     }
   
   if (fclose(accountfile)<0)
     {
	eventlog(eventlog_level_error,"account_save","could not close account file \"%s\" after writing (fclose: %s)",tempname,strerror(errno));
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
   if (access(account->filename, 0) == 0)
     {
	if (remove(account->filename)<0)
	  {
	     eventlog(eventlog_level_error,"account_save","could not delete account file \"%s\" (remove: %s)",account->filename,strerror(errno));
           free(tempname);
	     return -1;
	  }
     }
#endif
   
   if (rename(tempname,account->filename)<0)
     {
	eventlog(eventlog_level_error,"account_save","could not rename account file to \"%s\" (rename: %s)",account->filename,strerror(errno));
	free(tempname);
	return -1;
    }
   
   free(tempname);
#else
     {
	t_attribute /*const*/ * attr;
	/*    eventlog(eventlog_level_debug,"account_unload_attrs","unloading \"%s\"",account->filename);*/
	
	// aaron: done: just save attributes to MySQL that have marked as dirty...!

	for (attr=account->attrs; attr; attr=attr->next)
	  if (attr->dirty) 
	  	{
		    storage_set(account->storageid, attr->key, attr->val);
	            attr->dirty = 0;
		}
     }
#endif /* WITH_MYSQL */
	
   account->dirty = 0;

   return 1;
}


static int account_insert_attr(t_account * account, char const * key, char const * val)
{
    t_attribute * nattr;
    char *        nkey;
    char *        nval;
    
    if (!(nattr = malloc(sizeof(t_attribute))))
    {
	eventlog(eventlog_level_error,"account_insert_attr","could not allocate attribute");
	return -1;
    }
    if (!(nkey = strdup(key)))
    {
	eventlog(eventlog_level_error,"account_insert_attr","could not allocate attribute key");
	free(nattr);
	return -1;
    }
    if (!(nval = strdup(val)))
    {
	eventlog(eventlog_level_error,"account_insert_attr","could not allocate attribute value");
	free(nkey);
	free(nattr);
	return -1;
    }
    nattr->key  = nkey;
    nattr->val  = nval;

#ifdef WITH_MYSQL
    nattr->dirty = 1;
#endif
    
    nattr->next = account->attrs;
    
    account->attrs = nattr;
    
    return 0;
}

#ifdef DEBUG_ACCOUNT
extern char const * account_get_strattr_real(t_account * account, char const * key, char const * fn, unsigned int ln)
#else
extern char const * account_get_strattr(t_account * account, char const * key)
#endif
{
    char const *        newkey;
    t_attribute * curr, *last, *last2;
   
/*    eventlog(eventlog_level_trace,"account_get_strattr","<<<< ENTER!"); */
    if (!account)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_strattr","got NULL account (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_strattr","got NULL account");
#endif
	return NULL;
    }
    if (!key)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_strattr","got NULL key (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_strattr","got NULL key");
#endif
	return NULL;
    }

    account->accessed = 1;
    
    if (!account->loaded)
        if (account_load_attrs(account)<0)
	{
	    eventlog(eventlog_level_error,"account_get_strattr","could not load attributes");
	    return NULL;
	}

    if (strncasecmp(key,"DynKey",6)==0)
    {
	char * temp;
	
	/* Recent Starcraft clients seems to query DynKey\*\1\rank instead of
	 * Record\*\1\rank. So replace Dynkey with Record for key lookup.
	 */
	if (!(temp = strdup(key)))
	{
	    eventlog(eventlog_level_error,"account_get_strattr","could not allocate memory for temp");
	    return NULL;
	}
	strncpy(temp,"Record",6);
	newkey = temp;
    }
    else
	newkey = key;
 
    last = NULL;
    last2 = NULL;
    if (account->attrs)
	for (curr=account->attrs; curr; curr=curr->next) {
	    if (strcasecmp(curr->key,newkey)==0)
	    {
		if (newkey!=key)
		    free((void *)newkey); /* avoid warning */
/*	        eventlog(eventlog_level_trace,"account_get_strattr","found for \"%s\" value \"%s\"", newkey, curr->val); */
		/* DIZZY: found a match, lets promote it so it would be found faster next time */
		if (last) { 
		    if (last2) {
			last2->next = curr;
		    } else {
			account->attrs = curr;
		    }
		    
		    last->next = curr->next;
		    curr->next = last;
		    
		}
#ifdef TESTUNGET
		return strdup(curr->val);
#else
                return curr->val;
#endif
	    }
	    last2 = last;
	    last = curr;
	}
    if (newkey!=key)
	free((void *)newkey); /* avoid warning */
    
    if (account==default_acct) /* don't recurse infinitely */
	return NULL;
    
    return account_get_strattr(default_acct,key); /* FIXME: this is sorta dangerous because this pointer can go away if we re-read the config files... verify that nobody caches non-username, userid strings */
}


extern int account_unget_strattr(char const * val)
{
    if (!val)
    {
	eventlog(eventlog_level_error,"account_unget_strattr","got NULL val");
	return -1;
    }
#ifdef TESTUNGET
    free((void *)val); /* avoid warning */
#endif
    return 0;
}

#ifdef WITH_BITS
extern int account_set_strattr(t_account * account, char const * key, char const * val)
{
	char const * oldvalue;
	
	if (!account) {
		eventlog(eventlog_level_error,"account_set_strattr(bits)","got NULL account");
		return -1;
	}
	oldvalue = account_get_strattr(account,key); /* To check whether the value has changed. */
	if (oldvalue) {
	    if (val && strcmp(oldvalue,val)==0) {
		account_unget_strattr(oldvalue);
		return 0; /* The value hasn't changed. Don't produce unnecessary traffic. */
	    }
	    /* The value must have changed. Send the update to the msster and update local account. */
	    account_unget_strattr(oldvalue);
	}
	if (send_bits_va_set_attr(account_get_uid(account),key,val,NULL)<0) return -1;
	return account_set_strattr_nobits(account,key,val);
}

extern int account_set_strattr_nobits(t_account * account, char const * key, char const * val)
#else
extern int account_set_strattr(t_account * account, char const * key, char const * val)
#endif
{
    t_attribute * curr;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_set_strattr","got NULL account");
	return -1;
    }
    if (!key)
    {
	eventlog(eventlog_level_error,"account_set_strattr","got NULL key");
	return -1;
    }
    
#ifndef WITH_BITS
    if (!account->loaded)
        if (account_load_attrs(account)<0)
	{
	    eventlog(eventlog_level_error,"account_set_strattr","could not load attributes");
	    return -1;
	}
#endif
    curr = account->attrs;
    if (!curr) /* if no keys in attr list then we need to insert it */
    {
	if (val)
	{
	    account->dirty = 1; /* we are inserting an entry */
	    return account_insert_attr(account,key,val);
	}
	return 0;
    }
    
    if (strcasecmp(curr->key,key)==0) /* if key is already the first in the attr list */
    {
	if (val)
	{
	    char * temp;
	    
	    if (!(temp = strdup(val)))
	    {
		eventlog(eventlog_level_error,"account_set_strattr","could not allocate attribute value");
		return -1;
	    }
	    
	    if (strcmp(curr->val,temp)!=0)
	    {	
		account->dirty = 1; /* we are changing an entry */
#ifdef WITH_MYSQL
		curr->dirty = 1;
#endif
	    }	
	    free((void *)curr->val); /* avoid warning */
	    curr->val = temp;
	}
	else
	{
	    t_attribute * temp;
	    
	    temp = curr->next;
	    
	    account->dirty = 1; /* we are deleting an entry */
	    free((void *)curr->key); /* avoid warning */
	    free((void *)curr->val); /* avoid warning */
	    free((void *)curr); /* avoid warning */
	    
	    account->attrs = temp;
	}
	return 0;
    }
    
    for (; curr->next; curr=curr->next)
	if (strcasecmp(curr->next->key,key)==0)
	    break;
    
    if (curr->next) /* if key is already in the attr list */
    {
	if (val)
	{
	    char * temp;
	    
	    if (!(temp = strdup(val)))
	    {
		eventlog(eventlog_level_error,"account_set_strattr","could not allocate attribute value");
		return -1;
	    }
	    
	    if (strcmp(curr->next->val,temp)!=0)
	    {
		account->dirty = 1; /* we are changing an entry */
#ifdef WITH_MYSQL
		curr->dirty = 1;
#endif
	    }
	    free((void *)curr->next->val); /* avoid warning */
	    curr->next->val = temp;
	}
	else
	{
	    t_attribute * temp;
	    
	    temp = curr->next->next;
	    
	    account->dirty = 1; /* we are deleting an entry */
	    free((void *)curr->next->key); /* avoid warning */
	    free((void *)curr->next->val); /* avoid warning */
	    free(curr->next);
	    
	    curr->next = temp;
	}
	return 0;
    }
    
    if (val)
    {
	account->dirty = 1; /* we are inserting an entry */
	return account_insert_attr(account,key,val);
    }
    return 0;
}


static int account_load_attrs(t_account * account)
{
#ifndef WITH_MYSQL
    FILE *       accountfile;
    unsigned int line;
    char const * buff;
    unsigned int len;
    char *       esckey;
    char *       escval;
#else
    t_readattr   *readattr;
#endif
    char * key;
    char * val;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_load_attrs","got NULL account");
	return -1;
    }

#ifndef WITH_MYSQL
    if (!account->filename)
    {
#ifndef WITH_BITS
	eventlog(eventlog_level_error,"account_load_attrs","account has NULL filename");
	return -1;
#else /* WITH_BITS */
	if (!bits_uplink_connection) {
		eventlog(eventlog_level_error,"account_load_attrs","account has NULL filename on BITS master");
		return -1;
	}
    	if (account->uid==0) {
	    eventlog(eventlog_level_debug,"account_load_attrs","userid is unknown");
	    return 0;
    	} else if (!account->loaded) {
    	    if (account_get_bits_state(account)==account_state_valid) {
            	eventlog(eventlog_level_debug,"account_load_attrs","bits: virtual account "UID_FORMAT": loading attrs",account->uid);
	    	send_bits_va_get_allattr(account->uid);
	    } else {
		eventlog(eventlog_level_debug,"account_load_attrs","waiting for account "UID_FORMAT" to be locked",account->uid);
	    }
	    return 0;
	}
#endif /* WITH_BITS */
    }
#endif /* WITH_MYSQL */
    
    if (account->loaded) /* already done */
	return 0;
    if (account->dirty) /* if not loaded, how dirty? */
    {
	eventlog(eventlog_level_error,"account_load_attrs","can not load modified account");
	return -1;
    }
    
#ifndef WITH_MYSQL
    eventlog(eventlog_level_debug,"account_load_attrs","loading \"%s\"",account->filename);
    if (!(accountfile = fopen(account->filename,"r")))
#else
    eventlog(eventlog_level_trace,"account_load_attrs","loading uid: %u", account->storageid);
    if (!(readattr = storage_attr_getfirst(account->storageid, &key, &val)))
#endif
    {
#ifndef WITH_MYSQL
	eventlog(eventlog_level_error,"account_load_attrs","could not open account file \"%s\" for reading (fopen: %s)",account->filename,strerror(errno));
#else
	eventlog(eventlog_level_error,"account_load_attrs","could not open account (storage) \"%u\"",account->storageid);
#endif
	return -1;
    }
    
    account->loaded = 1; /* set now so set_strattr works */
#ifndef WITH_MYSQL
    for (line=1; (buff=file_get_line(accountfile)); line++)
    {
	if (buff[0]=='#' || buff[0]=='\0')
	{
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	if (strlen(buff)<6) /* "?"="" */
	{
	    eventlog(eventlog_level_error,"account_load_attrs","malformed line %d of account file \"%s\"",line,account->filename);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	
	len = strlen(buff)-5+1; /* - ""="" + NUL */
	if (!(esckey = malloc(len)))
	{
	    eventlog(eventlog_level_error,"account_load_attrs","could not allocate memory for esckey on line %d of account file \"%s\"",line,account->filename);
	    free((void *)buff); /* avoid warning */
	    continue;
	}
	if (!(escval = malloc(len)))
	{
	    eventlog(eventlog_level_error,"account_load_attrs","could not allocate memory for escval on line %d of account file \"%s\"",line,account->filename);
	    free((void *)buff); /* avoid warning */
	    free(esckey);
	    continue;
	}
	
	if (sscanf(buff,"\"%[^\"]\" = \"%[^\"]\"",esckey,escval)!=2)
	{
	    if (sscanf(buff,"\"%[^\"]\" = \"\"",esckey)!=1) /* hack for an empty value field */
	    {
		eventlog(eventlog_level_error,"account_load_attrs","malformed entry on line %d of account file \"%s\"",line,account->filename);
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
	
	if (key && val)
#ifdef WITH_BITS
	    account_set_strattr_nobits(account,key,val);
#else
	    account_set_strattr(account,key,val);
#endif
	if (key)
	    free((void *)key); /* avoid warning */
	if (val)
	    free((void *)val); /* avoid warning */
    }
#else /* WITH_MYSQL */
   do {
       eventlog(eventlog_level_trace,"account_load_attrs","loading: \"%s\" \"%s\"", key, val);
       account_set_strattr(account, key, val);
   } while (storage_attr_getnext(readattr, &key, &val) == 0);
#endif /* WITH_MYSQL */

#ifndef WITH_MYSQL
    if (fclose(accountfile)<0)
	eventlog(eventlog_level_error,"account_load_attrs","could not close account file \"%s\" after reading (fclose: %s)",account->filename,strerror(errno));
#else
    if (storage_attr_close(readattr)<0)
	eventlog(eventlog_level_error,"account_load_attrs","could not close account attribute read list");
#endif

    if (account->name) {
	eventlog(eventlog_level_error,"account_load_attrs","found old username chache, check out!");
	free(account->name);
    }
    account->name = NULL;

    account->dirty = 0;
#ifdef WITH_BITS
    account_set_bits_state(account,account_state_valid);
#endif

    return 0;
    
}

#ifndef WITH_MYSQL
static t_account * account_load(char const * filename)
{
    t_account * account;
    
    if (!filename)
    {
	eventlog(eventlog_level_error,"account_load","got NULL filename");
	return NULL;
    }
#else
static t_account * account_load(unsigned int sid)
{
    t_account * account;

    eventlog(eventlog_level_trace, "account_load","<<<< ENTER (uid:%u)!", sid);
#endif
    if (!(account = account_create(NULL,NULL)))
    {
#ifndef WITH_MYSQL
	eventlog(eventlog_level_error,"account_load","could not load account from file \"%s\"",filename);
#else
	eventlog(eventlog_level_error,"account_load","could not load account for id \"%u\"",sid);
#endif
	return NULL;
    }

#ifndef WITH_MYSQL
    if (!(account->filename = strdup(filename)))
    {
	eventlog(eventlog_level_error,"account_load","could not allocate memory for account->filename");
	account_destroy(account);
	return NULL;
    }
#else
    account->storageid = sid;
#endif
    
    return account;
}

extern int accountlist_load_default(void)
{
    if (default_acct)
	account_destroy(default_acct);

#ifndef WITH_MYSQL
    if (!(default_acct = account_load(prefs_get_defacct())))
#else
    if (!(default_acct = account_load(STORAGE_DEFAULT_ID)))
#endif
    {
#ifndef WITH_MYSQL
        eventlog(eventlog_level_error,"accountlist_load_default","could not load default account template from file \"%s\"",prefs_get_defacct());
#else
        eventlog(eventlog_level_error,"accountlist_load_default","could not load default account template from storage id \"%u\"");
#endif
	return -1;
    }

    if (account_load_attrs(default_acct)<0)
    {
	eventlog(eventlog_level_error,"accountlist_load_default","could not load default account template attributes");
	return -1;
    }
    
    eventlog(eventlog_level_debug,"accountlist_load_default","loaded default account template");

    return 0;
}

extern int account_logged_in(t_account * account)
{
  t_elem const * curr;
  t_connection * tc;
  t_account * acc;

  LIST_TRAVERSE_CONST(connlist(),curr)
  {
    if (tc = elem_get_data(curr)) 
    {
      if (acc = conn_get_account(tc))
      {
	if (acc == account) return 1;
      }
    }
    
  }
  return 0;
  
}

extern int accountlist_reload(int all)
{
  unsigned int count;
  t_account *  account;
  t_account *  old_acc;

  t_entry   * curr;
 	
  int starttime = time(NULL);
#ifndef WITH_MYSQL
  char const * dentry;
  char *       pathname;
  t_pdir *     accountdir;
#else
  t_readacct	 *readacct;
  unsigned int uid;
  char	accountuid[16];
#endif
  
#ifdef WITH_BITS
  if (!bits_master)
     {
       eventlog(eventlog_level_info,"accountlist_reload","running as BITS client -> no accounts loaded");
       return 0;
     }
#endif
  
 #ifndef WITH_MYSQL
  if (!(accountdir = p_opendir(prefs_get_userdir())))
#else
    if (!(readacct = storage_account_getfirst(&uid)))
#endif
      {
#ifndef WITH_MYSQL
	eventlog(eventlog_level_error,"accountlist_reload","unable to open user directory \"%s\" for reading (p_opendir: %s)",prefs_get_userdir(),strerror(errno));
#else
	eventlog(eventlog_level_error,"accountlist_reload","unable to start users read");
#endif
	return -1;
      }

  if (all == RELOAD_UPDATE_ALL)
  // go to accountlist and remove everything possible
  // we keep dirty account and accounts of users currently logged in
  {
    eventlog(eventlog_level_info,"accountlist_reload","removing accounts not logged to reload theam all");
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
      if (curr)
      {
        if (old_acc = (t_account *)entry_get_data(curr))
        {
	  if (!((old_acc->dirty) || account_logged_in(old_acc)))
	  {
	    war3_ladders_remove_account(old_acc);
	    account_destroy(old_acc);
	    hashtable_remove_entry(accountlist_head,curr);
  	  }
        }
      }
    }
  }
  
  force_account_add = 1; /* disable the protection */
  
  count = 0;
 #ifndef WITH_MYSQL
  while ((dentry = p_readdir(accountdir)))
     {
       if (dentry[0]=='.')
	 continue;
       if (!(pathname = malloc(strlen(prefs_get_userdir())+1+strlen(dentry)+1))) /* dir + / + file + NUL */
	 {
	   eventlog(eventlog_level_error,"accountlist_reload","could not allocate memory for pathname");
	   continue;
	 }
       sprintf(pathname,"%s/%s",prefs_get_userdir(),dentry);
#else
       do {
#endif
	 
#ifndef WITH_MYSQL
	 if (accountlist_find_account(dentry))
	 {
	   free(pathname);
	   continue;
	 }
	 
	 if (!(account = account_load(pathname)))
#else
	 sprintf(accountuid,"#%u",uid);
	 if (accountlist_find_account(accountuid))
	 {
	   continue;
	 }
	 if (!(account = account_load(uid)))
#endif
	   {
#ifndef WITH_MYSQL
	     eventlog(eventlog_level_error,"accountlist_reload","could not load account from file \"%s\"",pathname);
	     free(pathname);
#else
	     eventlog(eventlog_level_error,"accountlist_reload","could not load account from storage id \"%u\"",uid);
#endif
	     continue;
	   }
	 
	 if (!accountlist_add_account(account))
	   {
#ifndef WITH_MYSQL
	     eventlog(eventlog_level_error,"accountlist_reload","could not add account from file \"%s\" to list",pathname);
	     free(pathname);
#else
	     eventlog(eventlog_level_error,"accountlist_reload","could not add account with storage id \"%u\" to list",uid);
#endif
	     
	     account_destroy(account);
	     continue;
	   }
#ifndef WITH_MYSQL
	 free(pathname);
#endif
	 
	 /* might as well free up the memory since we probably won't need it */
	 account->accessed = 0; /* lie */
	 account_save(account,1000); /* big delta to force unload */
	 
         count++;
       }
#ifdef WITH_MYSQL
       while(storage_account_getnext(readacct, &uid) == 0);
#endif
       
       force_account_add = 0; /* enable the protection */
       
#ifndef WITH_MYSQL
       if (p_closedir(accountdir)<0)
	 eventlog(eventlog_level_error,"accountlist_reload","unable to close user directory \"%s\" (p_closedir: %s)",prefs_get_userdir(),strerror(errno));
#else
       if (storage_account_close(readacct)<0)
	 eventlog(eventlog_level_error,"accountlist_reload","unable to close read user list");
#endif
       
       if (count)
	 eventlog(eventlog_level_info,"accountlist_reload","loaded %u user accounts in %ld seconds",count,time(NULL) - starttime);
       if (all == RELOAD_UPDATE_ALL)
	 eventlog(eventlog_level_info,"accountlist_reload","done reloading all accounts");
       
       if ((count) || (all == RELOAD_UPDATE_ALL)) war3_ladder_update_all_accounts();
       return 0;
     }
  
  
    
extern int accountlist_create(void)
{
	unsigned int count;
    t_account *  account;
	
    int starttime = time(NULL);
#ifndef WITH_MYSQL
    char const * dentry;
    char *       pathname;
    t_pdir *     accountdir;
#else
    t_readacct	 *readacct;
    unsigned int uid;
#endif

    eventlog(eventlog_level_info, "accountlist_create", "started creating accountlist");
    
    if (!(accountlist_head = hashtable_create(prefs_get_hashtable_size())))
    {
        eventlog(eventlog_level_error,"accountlist_create","could not create accountlist_head");
	return -1;
    }
    
#ifdef WITH_BITS
    if (!bits_master)
    {
	eventlog(eventlog_level_info,"accountlist_create","running as BITS client -> no accounts loaded");
	return 0;
    }
#endif

#ifndef WITH_MYSQL
    if (!(accountdir = p_opendir(prefs_get_userdir())))
#else
    if (!(readacct = storage_account_getfirst(&uid)))
#endif
    {
#ifndef WITH_MYSQL
        eventlog(eventlog_level_error,"accountlist_create","unable to open user directory \"%s\" for reading (p_opendir: %s)",prefs_get_userdir(),strerror(errno));
#else
        eventlog(eventlog_level_error,"accountlist_create","unable to start users read");
#endif
        return -1;
    }

    force_account_add = 1; /* disable the protection */
    
    count = 0;
#ifndef WITH_MYSQL
    while ((dentry = p_readdir(accountdir)))
    {
	if (dentry[0]=='.')
	    continue;
	if (!(pathname = malloc(strlen(prefs_get_userdir())+1+strlen(dentry)+1))) /* dir + / + file + NUL */
	{
	    eventlog(eventlog_level_error,"accountlist_create","could not allocate memory for pathname");
	    continue;
	}
	sprintf(pathname,"%s/%s",prefs_get_userdir(),dentry);
#else
    do {
#endif

#ifndef WITH_MYSQL
	if (!(account = account_load(pathname)))
#else
	if (!(account = account_load(uid)))
#endif
	{
#ifndef WITH_MYSQL
	    eventlog(eventlog_level_error,"accountlist_create","could not load account from file \"%s\"",pathname);
	    free(pathname);
#else
	    eventlog(eventlog_level_error,"accountlist_create","could not load account from storage id \"%u\"",uid);
#endif
	    continue;
	}
	
	if (!accountlist_add_account(account))
	{
#ifndef WITH_MYSQL
	    eventlog(eventlog_level_error,"accountlist_create","could not add account from file \"%s\" to list",pathname);
	    free(pathname);
#else
	    eventlog(eventlog_level_error,"accountlist_create","could not add account with storage id \"%u\" to list",uid);
#endif

	    account_destroy(account);
	    continue;
	}
#ifndef WITH_MYSQL
       free(pathname);
#endif
	
	/* might as well free up the memory since we probably won't need it */
	account->accessed = 0; /* lie */
	account_save(account,1000); /* big delta to force unload */
	
        count++;
    }
#ifdef WITH_MYSQL
       while(storage_account_getnext(readacct, &uid) == 0);
#endif

    force_account_add = 0; /* enable the protection */

#ifndef WITH_MYSQL
    if (p_closedir(accountdir)<0)
	eventlog(eventlog_level_error,"accountlist_create","unable to close user directory \"%s\" (p_closedir: %s)",prefs_get_userdir(),strerror(errno));
#else
    if (storage_account_close(readacct)<0)
	eventlog(eventlog_level_error,"accountlist_create","unable to close read user list");
#endif
    
    eventlog(eventlog_level_info,"accountlist_create","loaded %u user accounts in %ld seconds",count,time(NULL) - starttime);
	
	
    return 0;
}


extern int accountlist_destroy(void)
{
    t_entry *   curr;
    t_account * account;
    
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
	if (!(account = entry_get_data(curr)))
	    eventlog(eventlog_level_error,"accountlist_destroy","found NULL account in list");
	else
	{
	    if (account_save(account,0)<0)
		eventlog(eventlog_level_error,"accountlist_destroy","could not save account");
	    
	    account_destroy(account);
	}
	hashtable_remove_entry(accountlist_head,curr);
    }
    
    if (hashtable_destroy(accountlist_head)<0)
	return -1;
    accountlist_head = NULL;
    return 0;
}


extern t_hashtable * accountlist(void)
{
    return accountlist_head;
}


extern void accountlist_unload_default(void)
{
    account_destroy(default_acct);
}


extern unsigned int accountlist_get_length(void)
{
    return hashtable_get_length(accountlist_head);
}


extern int accountlist_save(unsigned int delta)
{
    t_entry *    curr;
    t_account *  account;
    unsigned int scount;
    unsigned int tcount;
    
    scount=tcount = 0;
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
	account = entry_get_data(curr);
	switch (account_save(account,delta))
	{
	case -1:
	    eventlog(eventlog_level_error,"accountlist_save","could not save account");
	    break;
	case 1:
	    scount++;
	    break;
	case 0:
	default:
	    break;
	}
	tcount++;
    }
    
#ifdef WITH_BITS
    bits_va_lock_check();
#endif
    if (scount>0)
	eventlog(eventlog_level_debug,"accountlist_save","saved %u of %u user accounts",scount,tcount);
    return 0;
}


extern t_account * accountlist_find_account(char const * username)
{
    unsigned int userid=0;
    t_entry *    curr;
    t_account *  account;
    
    if (!username)
    {
	eventlog(eventlog_level_error,"accountlist_find_account","got NULL username");
	return NULL;
    }
    
    if ((username[0]=='#') || (!(prefs_get_savebyname())))
        if (str_to_uint(&username[1],&userid)<0)
            userid = 0;

    /* all accounts in list must be hashed already, no need to check */
    
    if (userid)
    {
	HASHTABLE_TRAVERSE(accountlist_head,curr)
	{
	    account = entry_get_data(curr);
	    if (account->uid==userid)
	    {
		hashtable_entry_release(curr);
		return account;
	    }
	}
    }
    else
    {
	unsigned int namehash;
	char const * tname;
	
	namehash = account_hash(username);
	HASHTABLE_TRAVERSE_MATCHING(accountlist_head,curr,namehash)
	{
	    account = entry_get_data(curr);
            if ((tname = account_get_name(account)))
	    {
		if (strcasecmp(tname,username)==0)
		{
		    account_unget_name(tname);
		    hashtable_entry_release(curr);
		    return account;
		}
		else
		  { account_unget_name(tname); }
	    }
	}
    }
    
    return NULL;
}


extern int accountlist_allow_add(void)
{
#ifdef WITH_BITS
    /* Client may tend to fill the accountlist with junk ... let them proceed */
    if (!bits_master)
	return 1; 
#endif

    if (force_account_add)
	return 1; /* the permission was forced */

    if (prefs_get_max_accounts()==0)
	return 1; /* allow infinite accounts */

    if (prefs_get_max_accounts()<=hashtable_get_length(accountlist_head))
    	return 0; /* maximum account limit reached */

    return 1; /* otherwise let them proceed */
}

extern t_account * accountlist_add_account(t_account * account)
{
    unsigned int uid;
    char const * username;
    
    if (!account)
    {
        eventlog(eventlog_level_error,"accountlist_add_account","got NULL account");
        return NULL;
    }
    
    username = account_get_strattr(account,"BNET\\acct\\username");
    uid = account_get_numattr(account,"BNET\\acct\\userid");
    
    if (!username || strlen(username)<1)
    {
        eventlog(eventlog_level_error,"accountlist_add_account","got bad account (empty username)");
        return NULL;
    }
    if (uid<1)
    {
#ifndef WITH_BITS
        eventlog(eventlog_level_error,"accountlist_add_account","got bad account (bad uid)");
	account_unget_name(username);
	return NULL;
#else
	uid = 0;
#endif
    }

    /* check whether the account limit was reached */
    if (!accountlist_allow_add()) {
	eventlog(eventlog_level_warn,"accountlist_add_account","account limit reached (current is %u, storing %u)",prefs_get_max_accounts(),hashtable_get_length(accountlist_head));
	return NULL;
    }
    
    /* delayed hash, do it before inserting account into the list */
    account->namehash = account_hash(username);
    account->uid = uid;
    
    /* mini version of accountlist_find_account(username) || accountlist_find_account(uid)  */
    {
	t_entry *    curr;
	t_account *  curraccount;
	char const * tname;
	
	HASHTABLE_TRAVERSE(accountlist_head,curr)
	{
	    curraccount = entry_get_data(curr);
	    if (curraccount->uid==uid)
	    {
		eventlog(eventlog_level_error,"accountlist_add_account","user \"%s\":"UID_FORMAT" already has an account (\"%s\":"UID_FORMAT")",username,uid,(tname = account_get_name(curraccount)),curraccount->uid);
		account_unget_name(tname);
		hashtable_entry_release(curr);
		account_unget_strattr(username);
		return NULL;
	    }
	    
	    if (curraccount->namehash==account->namehash &&
		(tname = account_get_name(curraccount)))
	    {
		if (strcasecmp(tname,username)==0)
		{
		    eventlog(eventlog_level_error,"accountlist_add_account","user \"%s\":"UID_FORMAT" already has an account (\"%s\":"UID_FORMAT")",username,uid,tname,curraccount->uid);
		    account_unget_name(tname);
		    hashtable_entry_release(curr);
		    account_unget_strattr(username);
		    return NULL;
		}
		else
		  { account_unget_name(tname); }
	    }
	}
    }
    account_unget_strattr(username);
    
    if (hashtable_insert_data(accountlist_head,account,account->namehash)<0)
    {
	eventlog(eventlog_level_error,"accountlist_add_account","could not add account to list");
	return NULL;
    }
    
    if (uid>maxuserid)
        maxuserid = uid;

    // 11-20-2002 aaron --->
    if (account)
      {
	if (account_get_solorank(account))
	  {
	    if (war3_ladder_add(&solo_ladder,
				uid,
				account_get_soloxp(account),
				account_get_solorank(account),
				account,0 )<0)
	      {    
		eventlog(eventlog_level_error,"accountlist_add_account","could not add to war3_ladder(solo)");
		hashtable_remove_data(accountlist_head,account,account->namehash);
		return NULL;
	      }
	  }
	if (account_get_teamrank(account))
	  {
	    if (war3_ladder_add(&team_ladder,
				uid,
				account_get_teamxp(account),
				account_get_teamrank(account),
				account,0 )<0)
	      {
		eventlog(eventlog_level_error,"accountlist_add_account","could not add to war3_ladder(team)");	
		hashtable_remove_data(accountlist_head,account,account->namehash);
		return NULL;
	      }
	  }
	if (account_get_ffarank(account))
	{
          if (war3_ladder_add(&ffa_ladder,
			      uid,
			      account_get_ffaxp(account),
			      account_get_ffarank(account),
			      account,0 )<0)
	  {
	    eventlog(eventlog_level_error,"accountlist_add_account","could not add to war_ladder(ffa)");
	    hashtable_remove_data(accountlist_head,account,account->namehash);
	    return NULL;
	  }
	}
	// user is part of a team
	if (account_get_atteamcount(account))
	{
	  int counter, rank;
	  // for each team he is in...
          for (counter=1; counter<=account_get_atteamcount(account); counter++)
          {
	    rank = account_get_atteamrank(account,counter);
	    // make sure... user has ranking... and team is valid...
	    if ((rank) && account_get_atteammembers(account,counter))
	    {
	       if (war3_ladder_add(&at_ladder,
				   uid,
				   account_get_atteamxp(account,counter),
				   rank,
				   account,
				   counter)<0)
	       {
		 eventlog(eventlog_level_error,"accountlist_add_account","could not add to war3_ladder(at)");
		 hashtable_remove_data(accountlist_head,account,account->namehash);
		 return NULL;

	       }
	    }
          }	   
	}
      }

    return account;
}

// aaron --->
extern int accounts_remove_accounting_infos(void)
{
    t_entry *    curr;
    t_account *  account;
    
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
	account = entry_get_data(curr);
	account_remove_verbose_accounting(account);
    }

    // for file mode, this is enough... on next save... the info's won't get saved...
    // so we effectivly got rid of them...
    // with MySQL mode it's more complicated... the info's remain in the database...
    // so we have to remove them now manually from DB...
#ifdef WITH_MYSQL
    accounts_remove_verbose_columns();
#endif

  return 0;
}

extern int accounts_rank_all(void)
{
    t_entry *    curr;
    t_account *  account;
    unsigned int uid;

    // unload ladders, create new.... !!!
    war3_ladders_destroy();
    war3_ladders_init();
    
    
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
      int counter;
      
      account = entry_get_data(curr);
      uid = account_get_uid(account);
      war3_ladder_add(&solo_ladder,uid,account_get_soloxp(account),account_get_solorank(account),account,0);
      war3_ladder_add(&team_ladder,uid,account_get_teamxp(account),account_get_teamrank(account),account,0);
      war3_ladder_add(&ffa_ladder,uid,account_get_ffaxp(account),account_get_ffarank(account),account,0);
      for (counter=1; counter<=account_get_atteamcount(account);counter++)
      {
	    if (account_get_atteammembers(account,counter) != NULL)
          war3_ladder_add(&at_ladder,uid,account_get_atteamxp(account,counter),account_get_atteamrank(account,counter),account,counter);
      }
    }
    war3_ladder_update_all_accounts();
    return 0;
}
// <---

#ifdef WITH_BITS

extern int accountlist_remove_account(t_account const * account)
{
    return hashtable_remove_data(accountlist_head,account,account->namehash);
}

/* This function checks whether the server knows if an account exists or not. 
 * It returns 1 if the server knows the account and 0 if the server doesn't 
 * know whether it exists. */
extern int account_name_is_unknown(char const * name)
{
    t_account * account;

    if (!name) {
    	eventlog(eventlog_level_error,"account_name_is_unknown","got NULL name");
	return -1;
    }
    if (bits_master) {
	return 0; /* The master server knows about all accounts */
    }
    account = accountlist_find_account(name);
    if (!account) {
	return 1; /* not in the accountlist */
    } else if (account_get_bits_state(account)==account_state_unknown) {
	return 1; /* in the accountlist, but still unknown */
    }
    return 0; /* account is known */
}

extern int account_state_is_pending(t_account const * account)
{
    if (!account) {
	eventlog(eventlog_level_error,"account_state_is_pending","got NULL account");
	return -1;
    }
    if (account_get_bits_state(account)==account_state_pending)
    	return 1;
    else
    	return 0;
}

extern int account_is_ready_for_use(t_account const * account)
{
    if (!account) {
	eventlog(eventlog_level_error,"account_is_ready_for_use","got NULL account");
	return -1;
    }
    if ((account_get_bits_state(account)==account_state_valid)&&(account_is_loaded(account)))
    	return 1;
    else
    	return 0;
}

extern int account_is_invalid(t_account const * account)
{
    if (!account) {
	eventlog(eventlog_level_error,"account_is_invalid","got NULL account");
	return -1;
    }
    if ((account_get_bits_state(account)==account_state_invalid)||(account_get_bits_state(account)==account_state_delete))
    	return 1;
    else
    	return 0;
}

extern t_bits_account_state account_get_bits_state(t_account const * account)
{
    if (!account) {
	eventlog(eventlog_level_error,"account_get_bits_state","got NULL account");
	return -1;
    }
    return account->bits_state;
}

extern int account_set_bits_state(t_account * account, t_bits_account_state state)
{
    if (!account) {
	eventlog(eventlog_level_error,"account_get_bits_state","got NULL account");
	return -1;
    }
    account->bits_state = state;
    return 0;
}


extern int account_set_accessed(t_account * account, int accessed) {
	if (!account) {
		eventlog(eventlog_level_error,"account_set_accessed","got NULL account");
		return -1;
	}
	account->accessed = accessed;
	return 0;
}

extern int account_is_loaded(t_account const * account)
{
	if (!account) {
		eventlog(eventlog_level_error,"account_is_loaded","got NULL account");
		return -1;
	}
	return account->loaded;
}

extern int account_set_loaded(t_account * account, int loaded)
{
	if (!account) {
		eventlog(eventlog_level_error,"account_set_loaded","got NULL account");
		return -1;
	}
	account->loaded = loaded;
	return 0;
}

extern int account_set_uid(t_account * account, int uid)
{
	if (!account) {
		eventlog(eventlog_level_error,"account_set_uid","got NULL account");
		return -1;
	}
	account->uid = uid;
	return account_set_numattr(account,"BNET\\acct\\userid",uid);
}

#endif

extern char const * account_get_first_key(t_account * account)
{
	if (!account) {
		eventlog(eventlog_level_error,"account_get_first_key","got NULL account");
		return NULL;
	}
	if (!account->attrs) {
		return NULL;
	}
	return account->attrs->key;
}

extern char const * account_get_next_key(t_account * account, char const * key)
{
	t_attribute * attr;

	if (!account) {
		eventlog(eventlog_level_error,"account_get_next_key","got NULL account");
		return NULL;
	}
	attr = account->attrs;
	while (attr) {
		if (strcmp(attr->key,key)==0) {
			if (attr->next) {
				return attr->next->key;
			} else {
				return NULL;
			}
		}
		attr = attr->next;
	}

	return NULL;
}


static int account_check_name(char const * name)
{
    unsigned int  i;
	char ch;
    
    if (!name) {
	eventlog(eventlog_level_error,"account_check_name","got NULL name");
	return -1;
    }
/*    if (!isalnum(name[0]))
	return -1;
*/
    
    for (i=0; i<strlen(name); i++)
    {
	// Changed by NonReal -- idiots are using ASCII names and it is annoying
        /* These are the Battle.net rules but they are too strict.
         * We want to allow any characters that wouldn't cause
         * problems so this should test for what is _not_ allowed
         * instead of what is.
         */
        ch = name[i];
        if (isalnum(ch)) continue;
	if (strchr(prefs_get_account_allowed_symbols(),ch)) continue;
        return -1;
    }
    if (i<USER_NAME_MIN || i>=USER_NAME_MAX)
	return -1;
    return 0;
}

#ifdef DEBUG_ACCOUNT
extern char const * account_get_name_real(t_account * account, char const * fn, unsigned int ln)
#else
extern char const * account_get_name(t_account * account)
#endif
{
    char const * temp;
    
    if (!account)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_name","got NULL account (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_name","got NULL account");
#endif
	return NULL; /* FIXME: places assume this can't fail */
    }
    
    if (account->name) { /* we have a cached username so return it */
/*	eventlog(eventlog_level_trace, "account_get_name", "we use the cached value, good!"); */
#ifdef TEST_UNGET
       return strdup(account->name);
#else
       return account->name;
#endif
    }

    /* we dont have a cached username so lets get it from attributes */
    if (!(temp = account_get_strattr(account,"BNET\\acct\\username")))
	eventlog(eventlog_level_error,"account_get_name","account has no username");
    else
	account->name = strdup(temp);
    return temp;
}


