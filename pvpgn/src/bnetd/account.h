/*
 * Copyright (C) 1998,1999  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
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
#ifndef INCLUDED_ACCOUNT_TYPES
#define INCLUDED_ACCOUNT_TYPES

#include "storage.h"
#ifndef JUST_NEED_TYPES
#define JUST_NEED_TYPES
#include "common/list.h"
#include "clan.h"
#undef JUST_NEED_TYPES
#else
#include "common/list.h"
#include "clan.h"
#endif

#ifdef ACCOUNT_INTERNAL_ACCESS
typedef struct attribute_struct
{
    char const *              key;
    char const *              val;
    int			      dirty;  // 1 = needs to be saved, 0 = unchanged
    struct attribute_struct * next;
} t_attribute;
#endif

#ifdef WITH_BITS
typedef enum
{
    account_state_invalid, /* account does not exist */
    account_state_delete,  /* account will be removed from cache */
    account_state_pending, /* account state is still unknown (request sent) */
    account_state_valid,   /* account is valid and locked (server receives notifications/changes for it) */
    account_state_unknown  /* account state is unknown and no request has been sent */
} t_bits_account_state;
#endif

#define ACCOUNT_CLIENTTAG_UNKN 0x00; // used for all non warcraft 3 clients so far
#define ACCOUNT_CLIENTTAG_WAR3 0x01;
#define ACCOUNT_CLIENTTAG_W3XP 0x02;

typedef struct account_struct
#ifdef ACCOUNT_INTERNAL_ACCESS
{
    t_attribute * attrs;
    char	* name;     /* profiling proved 99% of getstrattr its from get_name */
    unsigned int  namehash; /* cached from attrs */
    unsigned int  uid;      /* cached from attrs */
    char        * tmpOP_channel; /* non-permanent channel user is tmpOP in */
    char        * tmpVOICE_channel; /* channel user has been granted tmpVOICE in */
    int           dirty;    /* 1==needs to be saved, 0==clean */
    int           loaded;   /* 1==loaded, 0==only on disk */
    int           accessed; /* 1==yes, 0==no */
    int           friend_loaded;
    unsigned int  age;      /* number of times it has not been accessed */
    t_storage_info * storage;
    t_clanmember * clanmember;
    t_list * friends;
#ifdef WITH_BITS
    t_bits_account_state bits_state;
 /*   int           locked;    0==lock request not yet answered,
    				1==account exists & locked,
    				-1==no lock request sent,
    				2==invalid,
    				3==to be deleted (enum?) */
#endif
}
#endif
t_account;

#endif

/*****/
#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_ACCOUNT_PROTOS
#define INCLUDED_ACCOUNT_PROTOS

#define JUST_NEED_TYPES
#include "common/hashtable.h"
#undef JUST_NEED_TYPES

extern unsigned int maxuserid;

extern int accountlist_reload(void);
extern int account_check_name(char const * name);
extern t_account * account_create(char const * username, char const * passhash1) ;
extern t_account * create_vaccount(const char *username, unsigned int uid);
extern void account_destroy(t_account * account);
extern unsigned int account_get_uid(t_account const * account);
extern int account_match(t_account * account, char const * username);
extern int account_save(t_account * account, unsigned int delta);
#ifdef DEBUG_ACCOUNT
extern char const * account_get_strattr_real(t_account * account, char const * key, char const * fn, unsigned int ln);
#define account_get_strattr(A,K) account_get_strattr_real(A,K,__FILE__,__LINE__)
#else
extern char const * account_get_strattr(t_account * account, char const * key);
#endif
extern int account_unget_strattr(char const * val);
extern int account_set_strattr(t_account * account, char const * key, char const * val);
#ifdef WITH_BITS
extern int account_set_strattr_nobits(t_account * account, char const * key, char const * val);
extern int accountlist_remove_account(t_account const * account);
extern int account_name_is_unknown(char const * name);
extern int account_state_is_pending(t_account const * account);
extern int account_is_ready_for_use(t_account const * account);
extern int account_is_invalid(t_account const * account);
extern t_bits_account_state account_get_bits_state(t_account const * account);
extern int account_set_bits_state(t_account * account, t_bits_account_state state);
extern int account_set_accessed(t_account * account, int accessed);
extern int account_is_loaded(t_account const * account);
extern int account_set_loaded(t_account * account, int loaded);
extern int account_set_uid(t_account * account, int uid);
#endif

extern char const * account_get_first_key(t_account * account);
extern char const * account_get_next_key(t_account * account, char const * key);

extern int accountlist_create(void);
extern int accountlist_destroy(void);
extern t_hashtable * accountlist(void);
extern t_hashtable * accountlist_uid(void);
extern int accountlist_load_default(void);
extern void accountlist_unload_default(void);
extern unsigned int accountlist_get_length(void);
extern int accountlist_save(unsigned int delta);
extern t_account * accountlist_find_account(char const * username);
extern t_account * accountlist_find_account_by_uid(unsigned int uid);
extern t_account * accountlist_find_account_by_storage(t_storage_info *);
extern int accountlist_allow_add(void);
extern t_account * accountlist_add_account(t_account * account);
// aaron
extern int accounts_rank_all(void);
extern void accounts_get_attr(char const *);
/* names and passwords */
#ifdef DEBUG_ACCOUNT
extern char const * account_get_name_real(t_account * account, char const * fn, unsigned int ln);
# define account_get_name(A) account_get_name_real(A,__FILE__,__LINE__)
#else
extern char const * account_get_name(t_account * account);
#endif


extern int    account_set_tmpOP_channel(t_account * account, char const * tmpOP_channel);
extern char * account_get_tmpOP_channel(t_account * account);

extern int    account_set_tmpVOICE_channel(t_account * account, char const * tmpVOICE_channel);
extern char * account_get_tmpVOICE_channel(t_account * account);

// THEUNDYING MUTUAL FRIEND CHECK 7/27/02 UPDATED!
// moved to account.c/account.h by Soar to direct access struct t_account
extern int account_check_mutual( t_account * account,  int myuserid);
extern t_list * account_get_friends(t_account * account);

//clan thingy by DJP & Soar
extern int account_set_clanmember(t_account * account, t_clanmember * clanmember);
extern t_clanmember * account_get_clanmember(t_account * account);
extern t_clan * account_get_clan(t_account * account);
extern t_clan * account_get_creating_clan(t_account * account);

#endif
#endif

#include "account_wrap.h"
