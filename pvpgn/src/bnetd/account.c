/*
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2002,2003,2004 Dizzy 
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
#include "common/list.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"
#include "common/field_sizes.h"
#include "common/bnethash.h"
#include "common/introtate.h"
#include "account.h"
#include "common/hashtable.h"
#include "storage.h"
#include "common/list.h"
#include "connection.h"
#include "watch.h"
#include "friends.h"
#include "common/tag.h"
#include "ladder.h"
#include "clan.h"
#include "common/flags.h"
#include "common/xalloc.h"
#include "common/setup_after.h"

static t_hashtable * accountlist_head=NULL;
static t_hashtable * accountlist_uid_head=NULL;

static t_account * default_acct=NULL;
unsigned int maxuserid=0;

/* This is to force the creation of all accounts when we initially load the accountlist. */
static int force_account_add=0;

static int doing_loadattrs=0;

static unsigned int account_hash(char const * username);
static int account_insert_attr(t_account * account, char const * key, char const * val);
static t_account * account_load(t_storage_info *);
static int account_load_attrs(t_account * account);
static void account_unload_attrs(t_account * account);
static int account_load_friends(t_account * account);
static int account_unload_friends(t_account * account);

unsigned int account_hash(char const *username)
{
    register unsigned int h;
    register unsigned int len = strlen(username);

    for (h = 5381; len > 0; --len, ++username) {
        h += h << 5;
	if (isupper((int) *username) == 0)
	    h ^= *username;
	else
	    h ^= tolower((int) *username);
    }
    return h;
}

extern t_account * account_create(char const * username, char const * passhash1)
{
    t_account * account;
    
    account = xmalloc(sizeof(t_account));

    account->name     = NULL;
    account->storage  = NULL;
    account->clanmember = NULL;
    account->attrs    = NULL;
    account->age      = 0;
    account->friends  = NULL;
    account->conn = NULL;
    FLAG_ZERO(&account->flags);

    account->namehash = 0; /* hash it later before inserting */
    account->uid      = 0; /* hash it later before inserting */

    if (username) /* actually making a new account */
    {
	if (!passhash1)
	{
	    eventlog(eventlog_level_error,"account_create","got NULL passhash1");
	    account_destroy(account);
	    return NULL;
	}

	account->storage =  storage->create_account(username);
	if(!account->storage) {
	    eventlog(eventlog_level_error,"account_create","failed to add user to storage");
	    account_destroy(account);
	    return NULL;
	}
	FLAG_SET(&account->flags,ACCOUNT_FLAG_LOADED);

	account->name = xstrdup(username);

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
	
    }
    
    return account;
}


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
	    xfree((void *)attr->key); /* avoid warning */
	if (attr->val)
	    xfree((void *)attr->val); /* avoid warning */
        temp = attr->next;
	xfree((void *)attr); /* avoid warning */
    }
    account->attrs = NULL;
    if (account->name) {
	xfree(account->name);
	account->name = NULL;
    }
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_LOADED);
}

extern void account_destroy(t_account * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_destroy","got NULL account");
	return;
    }
    friendlist_close(account->friends);
    account_unload_attrs(account);
    if (account->storage)
	storage->free_info(account->storage);

    xfree(account);
}


extern unsigned int account_get_uid(t_account const * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_uid","got NULL account");
	return -1;
    }
    return account->uid;
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
        if (account->uid==userid)
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
    if (!account) {
	eventlog(eventlog_level_error,"account_save","got NULL account");
	return -1;
    }

    /* account aging logic */
    if (FLAG_ISSET(account->flags,ACCOUNT_FLAG_ACCESSED))
	account->age >>=  1;
    else
        account->age += delta;
    if (account->age>( (3*prefs_get_user_flush_timer()) >>1))
        account->age = ( (3*prefs_get_user_flush_timer()) >>1);
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_ACCESSED);

    if (!account->storage) {
	eventlog(eventlog_level_error,"account_save","account "UID_FORMAT" has NULL filename",account->uid);
	return -1;
    }

    if (!FLAG_ISSET(account->flags,ACCOUNT_FLAG_LOADED))
	return 0;

    if (!FLAG_ISSET(account->flags,ACCOUNT_FLAG_DIRTY)) {
	if (!delta && account->age>=prefs_get_user_flush_timer()) {
	    account_unload_friends(account);
	    account_unload_attrs(account);
	}
	return 0;
    }

    storage->write_attrs(account->storage, account->attrs);

    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_DIRTY);

    return 1;
}


static int account_insert_attr(t_account * account, char const * key, char const * val)
{
    t_attribute * nattr;
    char *        nkey;
    char *        nval;
    
    nattr = xmalloc(sizeof(t_attribute));

    nkey = (char *)storage->escape_key(key);
    if (nkey == key) nkey = xstrdup(key);
    nval = xstrdup(val);
    nattr->key  = nkey;
    nattr->val  = nval;
    nattr->dirty = 1;

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
    char const *        newkey = key, *newkey2;
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

    FLAG_SET(&account->flags,ACCOUNT_FLAG_ACCESSED);

//    eventlog(eventlog_level_trace, __FUNCTION__, "reading '%s'", key);

    if (strncasecmp(key,"DynKey",6)==0)
      {
	char * temp;
	
	/* Recent Starcraft clients seems to query DynKey\*\1\rank instead of
	 * Record\*\1\rank. So replace Dynkey with Record for key lookup.
	 */
	temp = xstrdup(key);
	strncpy(temp,"Record",6);
	newkey = temp;
      }
    else if (strncmp(key,"Star",4)==0)
      {
	char * temp;
	
	/* Starcraft clients query Star instead of STAR on logon screen.
	 */
	temp = xstrdup(key);
	strncpy(temp,"STAR",6);
	newkey = temp;
      }
    else if (strcasecmp(key,"clan\\name")==0)
      {
	/* we have decided to store the clan in "profile\\clanname" so we don't need an extra table
	 * for this. But when using a client like war3 it is requested from "clan\\name"
	 * let's just redirect this request...
	 */
	eventlog(eventlog_level_info,"get_strattr","we have a clan request");
	return account_get_w3_clanname(account);
      }

    if (newkey != key) {
	newkey2 = storage->escape_key(newkey);
	if (newkey2 != newkey) {
	    xfree((void*)newkey);
	    newkey = newkey2;
	}
    } else newkey = storage->escape_key(key);

    if (!FLAG_ISSET(account->flags,ACCOUNT_FLAG_LOADED))
    {
        if (account_load_attrs(account)<0)
	    {
	        eventlog(eventlog_level_error,"account_get_strattr","could not load attributes");
	        return NULL;
	    }
    }
      
    last = NULL;
    last2 = NULL;
    if (account->attrs)
	for (curr=account->attrs; curr; curr=curr->next) {
	    if (strcasecmp(curr->key,newkey)==0)
	    {
		if (newkey!=key)
		    xfree((void *)newkey); /* avoid warning */
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
		return xstrdup(curr->val);
#else
                return curr->val;
#endif
	    }
	    last2 = last;
	    last = curr;
	}

    if ((curr = (t_attribute *)storage->read_attr(account->storage, newkey)) != NULL) {
	curr->next = account->attrs;
	account->attrs = curr;
	if (newkey!=key) xfree((void *)newkey); /* avoid warning */
	return curr->val;
    }

    if (newkey!=key) xfree((void *)newkey); /* avoid warning */

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
    xfree((void *)val); /* avoid warning */
#endif
    return 0;
}

extern int account_set_strattr(t_account * account, char const * key, char const * val)
{
    t_attribute * curr;
    const char *newkey;
    
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
    
    if (!FLAG_ISSET(account->flags,ACCOUNT_FLAG_LOADED))
    {
        if (account_load_attrs(account)<0)
	    {
	        eventlog(eventlog_level_error,"account_set_strattr","could not load attributes");
	        return -1;
    	}
    }
    curr = account->attrs;
    if (!curr) /* if no keys in attr list then we need to insert it */
    {
	if (val)
	{
	    FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are inserting an entry */
	    return account_insert_attr(account,key,val);
	}
	return 0;
    }

    newkey = storage->escape_key(key);
    if (strcasecmp(curr->key,newkey)==0) /* if key is already the first in the attr list */
    {
	if (val)
	{
	    char * temp;
	    
	    temp = xstrdup(val);
	    
	    if (strcmp(curr->val,temp)!=0)
	    {	
		FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are changing an entry */
	    }
	    xfree((void *)curr->val); /* avoid warning */
	    curr->val = temp;
	    curr->dirty = 1;
	}
	else
	{
	    t_attribute * temp;

	    temp = curr->next;

	    FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are deleting an entry */
	    xfree((void *)curr->key); /* avoid warning */
	    xfree((void *)curr->val); /* avoid warning */
	    xfree((void *)curr); /* avoid warning */

	    account->attrs = temp;
	}

	if (key != newkey) xfree((void*)newkey);
	return 0;
    }
    
    for (; curr->next; curr=curr->next)
	if (strcasecmp(curr->next->key,newkey)==0)
	    break;

    if (curr->next) /* if key is already in the attr list */
    {
	if (val)
	{
	    char * temp;
	    
	    temp = xstrdup(val);
	    
	    if (strcmp(curr->next->val,temp)!=0)
	    {
		FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are changing an entry */
		curr->next->dirty = 1;
	    }
	    xfree((void *)curr->next->val); /* avoid warning */
	    curr->next->val = temp;
	}
	else
	{
	    t_attribute * temp;
	    
	    temp = curr->next->next;
	    
	    FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are deleting an entry */
	    xfree((void *)curr->next->key); /* avoid warning */
	    xfree((void *)curr->next->val); /* avoid warning */
	    xfree(curr->next);
	    
	    curr->next = temp;
	}

	if (key != newkey) xfree((void*)newkey);
	return 0;
    }
    
    if (key != newkey) xfree((void*)newkey);

    if (val)
    {
	FLAG_SET(&account->flags,ACCOUNT_FLAG_DIRTY); /* we are inserting an entry */
	return account_insert_attr(account,key,val);
    }
    return 0;
}

static int _cb_load_attr(const char *key, const char *val, void *data)
{
    t_account *account = (t_account *)data;

    return account_set_strattr(account, key, val);
}

static int account_load_attrs(t_account * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_load_attrs","got NULL account");
	return -1;
    }

    if (!account->storage)
    {
	eventlog(eventlog_level_error,"account_load_attrs","account has NULL filename");
	return -1;
    }

    
    if (FLAG_ISSET(account->flags,ACCOUNT_FLAG_LOADED)) /* already done */
	return 0;
    if (FLAG_ISSET(account->flags,ACCOUNT_FLAG_DIRTY)) /* if not loaded, how dirty? */
    {
	eventlog(eventlog_level_error,"account_load_attrs","can not load modified account");
	return -1;
    }

    FLAG_SET(&account->flags,ACCOUNT_FLAG_LOADED); /* set now so set_strattr works */
   doing_loadattrs = 1;
   if (storage->read_attrs(account->storage, _cb_load_attr, account)) {
        eventlog(eventlog_level_error, __FUNCTION__, "got error loading attributes");
	return -1;
   }
   doing_loadattrs = 0;

    if (account->name) {
	eventlog(eventlog_level_error,"account_load_attrs","found old username chache, check out!");
	xfree(account->name);
    }
    account->name = NULL;

    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_DIRTY);

    return 0;
    
}

extern void accounts_get_attr(const char * attribute)
{
/* FIXME: do it */
}

static t_account * account_load(t_storage_info *storage)
{
    t_account * account;

    eventlog(eventlog_level_trace, "account_load","<<<< ENTER !");
    if (!(account = account_create(NULL,NULL)))
    {
	eventlog(eventlog_level_error,"account_load","could not load account");
	return NULL;
    }

    account->storage = storage;
    
    return account;
}

extern int accountlist_load_default(void)
{
    if (default_acct)
	account_destroy(default_acct);

    if (!(default_acct = account_load(storage->get_defacct())))
    {
        eventlog(eventlog_level_error,"accountlist_load_default","could not load default account template");
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

static int _cb_read_accounts(t_storage_info *info, void *data)
{
    unsigned int *count = (unsigned int *)data;
    t_account *account;

    if (accountlist_find_account_by_storage(info)) {
	storage->free_info(info);
	return 0;
    }

    if (!(account = account_load(info))) {
        eventlog(eventlog_level_error,"accountlist_reload","could not load account from storage");
        storage->free_info(info);
        return -1;
    }

    if (!accountlist_add_account(account)) {
        eventlog(eventlog_level_error,"accountlist_reload","could not add account to list");
        account_destroy(account);
        return 0;
    }

    /* might as well free up the memory since we probably won't need it */
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_ACCESSED); /* lie */
    account_save(account,0); /* force unload */

    (*count)++;

    return 0;
}

extern int accountlist_reload(void)
{
    unsigned int count;

    int starttime = time(NULL);

  
  force_account_add = 1; /* disable the protection */
  
  count = 0;

  if (storage->read_accounts(_cb_read_accounts, &count)) {
      eventlog(eventlog_level_error, __FUNCTION__, "could not read accounts");
      return -1;
  }

  force_account_add = 0; /* enable the protection */

  if (count)
    eventlog(eventlog_level_info,"accountlist_reload","loaded %u user accounts in %ld seconds",count,time(NULL) - starttime);

  return 0;
}

extern t_account * account_load_new(char const * name, unsigned uid)
{
    t_account *account;
    t_storage_info *info;

    force_account_add = 1; /* disable the protection */
    info = storage->read_account(name,uid);
    if (!info) return NULL;

    if (!(account = account_load(info)))
    {
        eventlog(eventlog_level_error, __FUNCTION__,"could not load account from storage");
        storage->free_info(info);
        return NULL;
    }
	
    if (!accountlist_add_account(account))
    {
        eventlog(eventlog_level_error, __FUNCTION__,"could not add account to list");
        account_destroy(account);
        return NULL;
    }

    /* might as well free up the memory since we probably won't need it */
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_ACCESSED); /* lie */
    account_save(account,0); /* force unload */
    force_account_add = 0;

    return account;
}

static int _cb_read_accounts2(t_storage_info *info, void *data)
{
    unsigned int *count = (unsigned int *)data;
    t_account *account;

    if (!(account = account_load(info)))
    {
        eventlog(eventlog_level_error, __FUNCTION__,"could not load account from storage");
        storage->free_info(info);
        return -1;
    }

    if (!accountlist_add_account(account))
    {
        eventlog(eventlog_level_error, __FUNCTION__,"could not add account to list");
        account_destroy(account);
        return -1;
    }

    /* might as well free up the memory since we probably won't need it */
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_ACCESSED); /* lie */
    account_save(account,0); /* force unload */

    (*count)++;

    return 0;
}

extern int accountlist_create(void)
{
    unsigned int count;

    int starttime = time(NULL);

    eventlog(eventlog_level_info, "accountlist_create", "started creating accountlist");
    
    if (!(accountlist_head = hashtable_create(prefs_get_hashtable_size())))
    {
        eventlog(eventlog_level_error,"accountlist_create","could not create accountlist_head");
	return -1;
    }
    
    if (!(accountlist_uid_head = hashtable_create(prefs_get_hashtable_size())))
    {
        eventlog(eventlog_level_error,"accountlist_create","could not create accountlist_uid_head");
	return -1;
    }
    
    force_account_add = 1; /* disable the protection */
    
    count = 0;
    if (storage->read_accounts(_cb_read_accounts2, &count))
    {
        eventlog(eventlog_level_error,"accountlist_create","got error reading users");
        return -1;
    }

    force_account_add = 0; /* enable the protection */

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
    
    HASHTABLE_TRAVERSE(accountlist_uid_head,curr)
    {
	    hashtable_remove_entry(accountlist_head,curr);
    }

    if (hashtable_destroy(accountlist_head)<0)
	return -1;
    accountlist_head = NULL;
    if (hashtable_destroy(accountlist_uid_head)<0)
	return -1;
    accountlist_uid_head = NULL;
    return 0;
}


extern t_hashtable * accountlist(void)
{
    return accountlist_head;
}

extern t_hashtable * accountlist_uid(void)
{
    return accountlist_uid_head;
}


extern void accountlist_unload_default(void)
{
    account_destroy(default_acct);
}


extern unsigned int accountlist_get_length(void)
{
    return hashtable_get_length(accountlist_head);
}


extern int accountlist_save(unsigned int delta, int *syncdeltap)
{
    static t_entry *    curr = NULL;
    t_account *  account;
    unsigned int scount;
    unsigned int tcount;
    
    scount=tcount = 0;
    if (!curr || !syncdeltap || *syncdeltap > 0)
	curr = hashtable_get_first(accountlist_head);
    for(;curr; curr=entry_get_next(curr))
    {
	if (syncdeltap && tcount >= prefs_get_user_step()) break;
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
    
    if (scount>0)
	eventlog(eventlog_level_debug,"accountlist_save","saved %u of %u user accounts",scount,tcount);

    if (syncdeltap) {
	if (curr) *syncdeltap = -1;
	else if (*syncdeltap < 0) *syncdeltap = prefs_get_user_sync_timer();
    }

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
    
    if (username[0]=='#') {
        if (str_to_uint(&username[1],&userid)<0)
            userid = 0;
    } else if (!(prefs_get_savebyname()))
	if (str_to_uint(username,&userid)<0)
	    userid = 0;

    /* all accounts in list must be hashed already, no need to check */
    
    if (userid)
    {
        account=accountlist_find_account_by_uid(userid);
        if(account!=NULL)
            return account;
    }
    if ((!(userid)) || (userid && ((username[0]=='#') || (isdigit((int)username[0])))))
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
    
    return prefs_get_load_new_account() ? account_load_new(username,0) : NULL;
}


extern t_account * accountlist_find_account_by_uid(unsigned int uid)
{
    t_entry *    curr;
    t_account *  account;
    
    if (uid) {
	HASHTABLE_TRAVERSE_MATCHING(accountlist_uid_head,curr,uid)
	{
	    account = entry_get_data(curr);
	    if (account->uid==uid) {
		hashtable_entry_release(curr);
		return account;
	    }
	}
    }
    return prefs_get_load_new_account() ? account_load_new(NULL,uid) : NULL;
}


extern t_account * accountlist_find_account_by_storage(t_storage_info *info)
{
    t_entry *    curr;
    t_account *  account;

    if (!info) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL storage info");
	return NULL;
    }
    
    /* all accounts in list must be hashed already, no need to check */
    
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
	account = (t_account *) entry_get_data(curr);
	if (storage->cmp_info(info, account->storage) == 0)
	    return account;
    }
    
    return NULL;
}


extern int accountlist_allow_add(void)
{
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
	eventlog(eventlog_level_error,"accountlist_add_account","got bad account (bad uid), fix it!");
	uid = maxuserid + 1;
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

	if(uid <= maxuserid)
	HASHTABLE_TRAVERSE_MATCHING(accountlist_uid_head,curr,uid)
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
        }

	HASHTABLE_TRAVERSE_MATCHING(accountlist_head,curr,account->namehash)
	{
	    curraccount = entry_get_data(curr);
	    if ((tname = account_get_name(curraccount)))
	    {
		    if (strcasecmp(tname,username)==0)
		    {
		        eventlog(eventlog_level_info,"accountlist_add_account","user \"%s\":"UID_FORMAT" already has an account (\"%s\":"UID_FORMAT")",username,uid,tname,curraccount->uid);
		        account_unget_name(tname);
		        hashtable_entry_release(curr);
		        account_unget_strattr(username);
		        return NULL;
		    }
		    account_unget_name(tname);
	    }
	}
    }
    account_unget_strattr(username);

    if (hashtable_insert_data(accountlist_head,account,account->namehash)<0)
    {
	eventlog(eventlog_level_error,"accountlist_add_account","could not add account to list");
	return NULL;
    }
    
    if (hashtable_insert_data(accountlist_uid_head,account,uid)<0)
    {
	eventlog(eventlog_level_error,"accountlist_add_account","could not add account to list");
	return NULL;
    }
    
/*    hashtable_stats(accountlist_head); */

    if (uid>maxuserid)
        maxuserid = uid;

    return account;
}

// aaron --->
/*
extern int accounts_rank_all(void)
{
    t_entry *    curr;
    t_account *  account;
    unsigned int uid;

    // unload ladders, create new.... !!!
    ladders_destroy();
    ladders_init();
    
    
    HASHTABLE_TRAVERSE(accountlist_head,curr)
    {
      int counter;
  
      account = entry_get_data(curr);
      uid = account_get_uid(account);
      war3_ladder_add(solo_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_soloxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_sololevel(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      war3_ladder_add(team_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_teamxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_teamlevel(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      war3_ladder_add(ffa_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_ffaxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_ffalevel(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      for (counter=1; counter<=account_get_atteamcount(account,CLIENTTAG_WARCRAFT3);counter++)
      {
	    if (account_get_atteammembers(account,counter,CLIENTTAG_WARCRAFT3) != NULL)
          war3_ladder_add(at_ladder(CLIENTTAG_WARCRAFT3),
			  uid,account_get_atteamxp(account,counter,CLIENTTAG_WARCRAFT3),
			  account_get_atteamlevel(account,counter,CLIENTTAG_WARCRAFT3),
			  account,counter,CLIENTTAG_WARCRAFT3);
      }

      war3_ladder_add(solo_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_soloxp(account,CLIENTTAG_WAR3XP),
		      account_get_sololevel(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      war3_ladder_add(team_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_teamxp(account,CLIENTTAG_WAR3XP),
		      account_get_teamlevel(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      war3_ladder_add(ffa_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_ffaxp(account,CLIENTTAG_WAR3XP),
		      account_get_ffalevel(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      for (counter=1; counter<=account_get_atteamcount(account,CLIENTTAG_WAR3XP);counter++)
      {
	    if (account_get_atteammembers(account,counter,CLIENTTAG_WAR3XP) != NULL)
          war3_ladder_add(at_ladder(CLIENTTAG_WAR3XP),
			  uid,account_get_atteamxp(account,counter,CLIENTTAG_WAR3XP),
			  account_get_atteamlevel(account,counter,CLIENTTAG_WAR3XP),
			  account,counter,CLIENTTAG_WAR3XP);
      }

    }
    ladder_update_all_accounts();
    return 0;
}
*/
// <---

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


extern int account_check_name(char const * name)
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
        if (isalnum((int)ch)) continue;
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
       return xstrdup(account->name);
#else
       return account->name;
#endif
    }

    /* we dont have a cached username so lets get it from attributes */
    if (!(temp = account_get_strattr(account,"BNET\\acct\\username")))
	eventlog(eventlog_level_error,"account_get_name","account has no username");
    else
	account->name = xstrdup(temp);
    return account->name;
}


extern int account_check_mutual( t_account * account, int myuserid)
{
    if (account == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    if(!myuserid) {
	eventlog(eventlog_level_error,"account_check_mutual","got NULL userid");
	return -1;
    }

    if(account->friends!=NULL)
    {
        t_friend * fr;
        if((fr=friendlist_find_uid(account->friends, myuserid))!=NULL)
        {
            friend_set_mutual(fr, 1);
            return 0;
        }
    }
    else
    {
	int i;
	int n = account_get_friendcount(account);
	int friend;
	for(i=0; i<n; i++) 
	{
	    friend = account_get_friend(account,i);
	    if(!friend)  {
		eventlog(eventlog_level_error,"account_check_mutual","got NULL friend");
		continue;
	    }

	    if(myuserid==friend)
		return 0;
	}
    }

    // If friend isnt in list return -1 to tell func NO
    return -1;
}

extern t_list * account_get_friends(t_account * account)
{
    if (!account)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return NULL;
    }

    if(!FLAG_ISSET(account->flags,ACCOUNT_FLAG_FLOADED))
	if(account_load_friends(account)<0)
        {
    	    eventlog(eventlog_level_error,__FUNCTION__,"could not load friend list");
            return NULL;
        }

    return account->friends;
}

static int account_load_friends(t_account * account)
{
    int i;
    int n;
    int friend;
    t_account * acc;
    t_friend * fr;

    int newlist=0;
    if (!account)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return -1;
    }

    if(FLAG_ISSET(account->flags,ACCOUNT_FLAG_FLOADED))
        return 0;

    if(account->friends==NULL)
    {
        if((account->friends=friendlist_init())==NULL)
            return -1;
        newlist=1;
    }

    n = account_get_friendcount(account);
    for(i=0; i<n; i++)
    {
	friend = account_get_friend(account,i);
        if(!friend)  {
            account_remove_friend(account, i);
            continue;
        }
        fr=NULL;
        if(newlist || (fr=friendlist_find_uid(account->friends, friend))==NULL)
        {
            if((acc = accountlist_find_account_by_uid(friend))==NULL)
            {
                if(account_remove_friend(account, i) == 0)
		{
		    i--;
		    n--;
		}
                continue;
            }
            if(account_check_mutual(acc, account_get_uid(account))==0)
                friendlist_add_account(account->friends, acc, 1);
            else
                friendlist_add_account(account->friends, acc, 0);
        }
        else {
            if((acc=friend_get_account(fr))==NULL)
            {
                account_remove_friend(account, i);
                continue;
            }
            if(account_check_mutual(acc, account_get_uid(account))==0)
                friend_set_mutual(fr, 1);
            else
                friend_set_mutual(fr, 0);
        }
    }
    if(!newlist)
        friendlist_purge(account->friends);
    FLAG_SET(&account->flags,ACCOUNT_FLAG_FLOADED);
    return 0;
}

static int account_unload_friends(t_account * account)
{
    if(friendlist_unload(account->friends)<0)
        return -1;
    FLAG_CLEAR(&account->flags,ACCOUNT_FLAG_FLOADED);
    return 0;
}

extern int account_set_clanmember(t_account * account, t_clanmember * clanmember)
{
    if(account==NULL)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return -1;
    }

    account->clanmember = clanmember;
    return 0;
}

extern t_clanmember * account_get_clanmember(t_account * account)
{
    if(account==NULL)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return NULL;
    }

    if(account->clanmember && clanmember_get_clan(account->clanmember) && (clan_get_created(clanmember_get_clan(account->clanmember)) > 0))
	return account->clanmember;
    else
	return NULL;
}

extern t_clan * account_get_clan(t_account * account)
{
    if(account==NULL)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return NULL;
    }

    if(account->clanmember && (clanmember_get_clan(account->clanmember) != NULL) && (clan_get_created(clanmember_get_clan(account->clanmember)) > 0))
	return clanmember_get_clan(account->clanmember);
    else
	return NULL;
}

extern t_clan * account_get_creating_clan(t_account * account)
{
    if(account==NULL)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return NULL;
    }

    if(account->clanmember && (clanmember_get_clan(account->clanmember) != NULL) && (clan_get_created(clanmember_get_clan(account->clanmember)) <= 0))
	return clanmember_get_clan(account->clanmember);
    else
	return NULL;
}

int account_set_conn(t_account * account, t_connection * conn)
{
  if (!(account))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
    return -1;
  }

  account->conn = conn;
  
  return 0;
}

t_connection * account_get_conn(t_account * account)
{
  if (!(account))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
    return NULL;
  }

  return account->conn;
}
