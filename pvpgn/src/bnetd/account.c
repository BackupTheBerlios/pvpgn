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
#include "common/list.h"
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
#include "friends.h"
#include "common/tag.h"
//aaron
#include "war3ladder.h"
#include "clan.h"
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

/*
static unsigned int account_hash(char const * username)
{
    register unsigned int i;
    register unsigned int hash;
    unsigned int pos;
    unsigned int ch;
    unsigned int len = strlen(username);

    hash = (len+1)*120343021;
    for (pos=0,i=0; i<len; i++)
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
*/

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
    
    if (!(account = malloc(sizeof(t_account))))
    {
	eventlog(eventlog_level_error,"account_create","could not allocate memory for account");
	return NULL;
    }

    account->name     = NULL;
    account->storage  = NULL;
    account->clanmember = NULL;
    account->attrs    = NULL;
    account->dirty    = 0;
    account->accessed = 0;
    account->age      = 0;
    account->tmpOP_channel = NULL;
    account->tmpVOICE_channel = NULL;
    account->friends  = NULL;
    account->friend_loaded = 0;
    
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
    friendlist_close(account->friends);
    account_unload_attrs(account);
    if (account->storage)
	storage->free_info(account->storage);

    free(account);
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
   
   if (!account)
     {
	eventlog(eventlog_level_error,"account_save","got NULL account");
	return -1;
     }

   
   /* account aging logic */
   if (account->accessed)
     account->age >>=  1;
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
   
   if (!account->storage)
     {
# ifdef WITH_BITS
	if (!bits_master)
	  return 0; /* It's OK since we don't have the files on the bits clients */
# endif
	eventlog(eventlog_level_error,"account_save","account "UID_FORMAT" has NULL filename",account->uid);
	return -1;
     }

   if (!account->loaded)
     return 0;

   if (!account->dirty) {
	if (account->age>=prefs_get_user_flush_timer()) {
	    account_unload_friends(account);
	    account_unload_attrs(account);
	}
	return 0;
   }
   
   storage->write_attrs(account->storage, account->attrs);

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

    nkey = (char *)storage->escape_key(key);
    if (nkey == key && !(nkey = strdup(key)))
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

    account->accessed = 1;

//    eventlog(eventlog_level_trace, __FUNCTION__, "reading '%s'", key);

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
    else if (strncmp(key,"Star",4)==0)
      {
	char * temp;
	
	/* Starcraft clients query Star instead of STAR on logon screen.
	 */
	if (!(temp = strdup(key)))
	  {
	    eventlog(eventlog_level_error,"account_get_strattr","could not allocate memory for temp");
	    return NULL;
	  }
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
	    free((void*)newkey);
	    newkey = newkey2;
	}
    } else newkey = storage->escape_key(key);

    if (!account->loaded)
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

    if ((curr = (t_attribute *)storage->read_attr(account->storage, newkey)) != NULL) {
	curr->next = account->attrs;
	account->attrs = curr;
	if (newkey!=key) free((void *)newkey); /* avoid warning */
	return curr->val;
    }

    if (newkey!=key) free((void *)newkey); /* avoid warning */

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
    
#ifndef WITH_BITS
    if (!account->loaded)
    {
        if (account_load_attrs(account)<0)
	    {
	        eventlog(eventlog_level_error,"account_set_strattr","could not load attributes");
	        return -1;
    	}
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

    newkey = storage->escape_key(key);
    if (strcasecmp(curr->key,newkey)==0) /* if key is already the first in the attr list */
    {
	if (val)
	{
	    char * temp;
	    
	    if (!(temp = strdup(val)))
	    {
		eventlog(eventlog_level_error,"account_set_strattr","could not allocate attribute value");
		if (key != newkey) free((void*)newkey);
		return -1;
	    }
	    
	    if (strcmp(curr->val,temp)!=0)
	    {	
		account->dirty = 1; /* we are changing an entry */
	    }
	    free((void *)curr->val); /* avoid warning */
	    curr->val = temp;
	    curr->dirty = 1;
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

	if (key != newkey) free((void*)newkey);
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
	    
	    if (!(temp = strdup(val)))
	    {
		eventlog(eventlog_level_error,"account_set_strattr","could not allocate attribute value");
		if (key != newkey) free((void*)newkey);
		return -1;
	    }
	    
	    if (strcmp(curr->next->val,temp)!=0)
	    {
		account->dirty = 1; /* we are changing an entry */
		curr->next->dirty = 1;
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

	if (key != newkey) free((void*)newkey);
	return 0;
    }
    
    if (key != newkey) free((void*)newkey);

    if (val)
    {
	account->dirty = 1; /* we are inserting an entry */
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

    
    if (account->loaded) /* already done */
	return 0;
    if (account->dirty) /* if not loaded, how dirty? */
    {
	eventlog(eventlog_level_error,"account_load_attrs","can not load modified account");
	return -1;
    }
    
    account->loaded = 1; /* set now so set_strattr works */
   doing_loadattrs = 1;
   if (storage->read_attrs(account->storage, _cb_load_attr, account)) {
        eventlog(eventlog_level_error, __FUNCTION__, "got error loading attributes");
	return -1;
   }
   doing_loadattrs = 0;

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
    account->accessed = 0; /* lie */
    account_save(account,3600); /* big delta to force unload */

    (*count)++;

    return 0;
}

extern int accountlist_reload(void)
{
    unsigned int count;

    int starttime = time(NULL);

#ifdef WITH_BITS
  if (!bits_master)
     {
       eventlog(eventlog_level_info,"accountlist_reload","running as BITS client -> no accounts loaded");
       return 0;
     }
#endif
  
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

static int _cb_read_accounts2(t_storage_info *info, void *data)
{
    unsigned int *count = (unsigned int *)data;
    t_account *account;

    if (!(account = account_load(info)))
    {
        eventlog(eventlog_level_error,"accountlist_create","could not load account from storage");
        storage->free_info(info);
        return 0;
    }
	
    if (!accountlist_add_account(account))
    {
        eventlog(eventlog_level_error,"accountlist_create","could not add account to list");
        account_destroy(account);
        return 0;
    }

    /* might as well free up the memory since we probably won't need it */
    account->accessed = 0; /* lie */
    account_save(account,1000); /* big delta to force unload */
	
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
    
#ifdef WITH_BITS
    if (!bits_master)
    {
	eventlog(eventlog_level_info,"accountlist_create","running as BITS client -> no accounts loaded");
	return 0;
    }
#endif

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
    if ((!(userid)) || (userid && ((username[0]=='#') || (isdigit(username[0])))))
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
    return NULL;
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
		        eventlog(eventlog_level_error,"accountlist_add_account","user \"%s\":"UID_FORMAT" already has an account (\"%s\":"UID_FORMAT")",username,uid,tname,curraccount->uid);
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
      war3_ladder_add(solo_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_soloxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_sololevel(account,CLIENTTAG_WARCRAFT3),
		      account_get_solorank(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      war3_ladder_add(team_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_teamxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_teamlevel(account,CLIENTTAG_WARCRAFT3),
		      account_get_teamrank(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      war3_ladder_add(ffa_ladder(CLIENTTAG_WARCRAFT3),
		      uid,account_get_ffaxp(account,CLIENTTAG_WARCRAFT3),
		      account_get_ffalevel(account,CLIENTTAG_WARCRAFT3),
		      account_get_ffarank(account,CLIENTTAG_WARCRAFT3),
		      account,0,CLIENTTAG_WARCRAFT3);
      for (counter=1; counter<=account_get_atteamcount(account,CLIENTTAG_WARCRAFT3);counter++)
      {
	    if (account_get_atteammembers(account,counter,CLIENTTAG_WARCRAFT3) != NULL)
          war3_ladder_add(at_ladder(CLIENTTAG_WARCRAFT3),
			  uid,account_get_atteamxp(account,counter,CLIENTTAG_WARCRAFT3),
			  account_get_atteamlevel(account,counter,CLIENTTAG_WARCRAFT3),
			  account_get_atteamrank(account,counter,CLIENTTAG_WARCRAFT3),
			  account,counter,CLIENTTAG_WARCRAFT3);
      }

      war3_ladder_add(solo_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_soloxp(account,CLIENTTAG_WAR3XP),
		      account_get_sololevel(account,CLIENTTAG_WAR3XP),
		      account_get_solorank(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      war3_ladder_add(team_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_teamxp(account,CLIENTTAG_WAR3XP),
		      account_get_teamlevel(account,CLIENTTAG_WAR3XP),
		      account_get_teamrank(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      war3_ladder_add(ffa_ladder(CLIENTTAG_WAR3XP),
		      uid,account_get_ffaxp(account,CLIENTTAG_WAR3XP),
		      account_get_ffalevel(account,CLIENTTAG_WAR3XP),
		      account_get_ffarank(account,CLIENTTAG_WAR3XP),
		      account,0,CLIENTTAG_WAR3XP);
      for (counter=1; counter<=account_get_atteamcount(account,CLIENTTAG_WAR3XP);counter++)
      {
	    if (account_get_atteammembers(account,counter,CLIENTTAG_WAR3XP) != NULL)
          war3_ladder_add(at_ladder(CLIENTTAG_WAR3XP),
			  uid,account_get_atteamxp(account,counter,CLIENTTAG_WAR3XP),
			  account_get_atteamlevel(account,counter,CLIENTTAG_WAR3XP),
			  account_get_atteamrank(account,counter,CLIENTTAG_WAR3XP),
			  account,counter,CLIENTTAG_WAR3XP);
      }

    }
    war3_ladder_update_all_accounts();
    return 0;
}
// <---

#ifdef WITH_BITS

extern int accountlist_remove_account(t_account const * account)
{
    if(hashtable_remove_data(accountlist_head,account,account->namehash)!=0)
        return -1;
    if(hashtable_remove_data(accountlist_uid_head,account,account->uid)!=0)
        return -1;
    return 0;
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

extern int account_set_tmpOP_channel(t_account * account, char const * tmpOP_channel)
{
	char * tmp;

	if (!account)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	  return -1;
	}

	if (account->tmpOP_channel)
	{
	  free((void *)account->tmpOP_channel);
	  account->tmpOP_channel = NULL;
	}

	if (tmpOP_channel)
	{
	  if (!(tmp = strdup(tmpOP_channel)))
	  {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not strdup tmpOP_channel");
	    return -1;
	  }
	  account->tmpOP_channel = tmp;
	}

	return 0;
}

extern char * account_get_tmpOP_channel(t_account * account)
{
	if (!account)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	  return NULL;
	}
	
	return account->tmpOP_channel;
}

extern int account_set_tmpVOICE_channel(t_account * account, char const * tmpVOICE_channel)
{
	char * tmp;

	if (!account)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	  return -1;
	}

	if (account->tmpVOICE_channel)
	{
	  free((void *)account->tmpVOICE_channel);
	  account->tmpVOICE_channel = NULL;
	}

	if (tmpVOICE_channel)
	{
	  if (!(tmp = strdup(tmpVOICE_channel)))
	  {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not strdup tmpVOICE_channel");
	    return -1;
	  }
	  account->tmpVOICE_channel = tmp;
	}

	return 0;
}

extern char * account_get_tmpVOICE_channel(t_account * account)
{
	if (!account)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	  return NULL;
	}
	
	return account->tmpVOICE_channel;
}


// THEUNDYING - MUTUAL FRIEND CHECK 
// fixed by zap-zero-tested and working 100% TheUndying
// modified by Soar to support account->friends list direct access
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

    if(!account->friend_loaded)
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

    if(account->friend_loaded)
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
    account->friend_loaded=1;
    return 0;
}

static int account_unload_friends(t_account * account)
{
    if(friendlist_unload(account->friends)<0)
        return -1;
    account->friend_loaded=0;
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
