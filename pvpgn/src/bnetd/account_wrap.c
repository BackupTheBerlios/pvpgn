/*
 * Copyright (C) 1998,1999  Ross Combs (rocombs@cs.nmsu.edu)
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
#define CHARACTER_INTERNAL_ACCESS
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
#include "common/list.h"
#include "common/eventlog.h"
#include "common/util.h"
#include "common/bnettime.h"
#include "ladder.h"
#include "account.h"
#include "character.h"
#include "connection.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "common/setup_after.h"
#include "common/bnet_protocol.h"
#include "common/tag.h"
#include "command.h"
//aaron
#include "war3ladder.h"
#include "prefs.h"
#include "friends.h"
#include "clan.h"
#include "anongame_infos.h"

static unsigned int char_icon_to_uint(char * icon);

#ifdef DEBUG_ACCOUNT
extern unsigned int account_get_numattr_real(t_account * account, char const * key, char const * fn, unsigned int ln)
#else
extern unsigned int account_get_numattr(t_account * account, char const * key)
#endif
{
    char const * temp;
    unsigned int val;
    
    if (!account)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_numattr","got NULL account (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_numattr","got NULL account");
#endif
	return 0;
    }
    if (!key)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_numattr","got NULL key (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_numattr","got NULL key");
#endif
	return 0;
    }
    
    if (!(temp = account_get_strattr(account,key)))
	return 0;
    
    if (str_to_uint(temp,&val)<0)
    {
	eventlog(eventlog_level_error,"account_get_numattr","not a numeric string \"%s\" for key \"%s\"",temp,key);
	account_unget_strattr(temp);
	return 0;
    }
    account_unget_strattr(temp);
    
    return val;
}


extern int account_set_numattr(t_account * account, char const * key, unsigned int val)
{
    char temp[32]; /* should be more than enough room */
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_set_numattr","got NULL account");
	return -1;
    }
    if (!key)
    {
	eventlog(eventlog_level_error,"account_set_numattr","got NULL key");
	return -1;
    }
    
    sprintf(temp,"%u",val);
    return account_set_strattr(account,key,temp);
}


#ifdef DEBUG_ACCOUNT
extern int account_get_boolattr_real(t_account * account, char const * key, char const * fn, unsigned int ln)
#else
extern int account_get_boolattr(t_account * account, char const * key)
#endif
{
    char const * temp;
    
    if (!account)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_boolattr","got NULL account (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_boolattr","got NULL account");
#endif
	return -1;
    }
    if (!key)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_get_boolattr","got NULL key (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_get_boolattr","got NULL key");
#endif
	return -1;
    }
    
    if (!(temp = account_get_strattr(account,key)))
	return -1;
    
    switch (str_get_bool(temp))
    {
    case 1:
	account_unget_strattr(temp);
	return 1;
    case 0:
	account_unget_strattr(temp);
	return 0;
    default:
	account_unget_strattr(temp);
	eventlog(eventlog_level_error,"account_get_boolattr","bad boolean value \"%s\" for key \"%s\"",temp,key);
	return -1;
    }
}


extern int account_set_boolattr(t_account * account, char const * key, int val)
{
    if (!account)
    {
	eventlog(eventlog_level_error,"account_set_boolattr","got NULL account");
	return -1;
    }
    if (!key)
    {
	eventlog(eventlog_level_error,"account_set_boolattr","got NULL key");
	return -1;
    }
    
    return account_set_strattr(account,key,val?"true":"false");
}


/****************************************************************/


#ifdef DEBUG_ACCOUNT
extern int account_unget_name_real(char const * name, char const * fn, unsigned int ln)
#else
extern int account_unget_name(char const * name)
#endif
{
    if (!name)
    {
#ifdef DEBUG_ACCOUNT
	eventlog(eventlog_level_error,"account_unget_name","got NULL name (from %s:%u)",fn,ln);
#else
	eventlog(eventlog_level_error,"account_unget_name","got NULL name");
#endif
	return -1;
    }
    
    return account_unget_strattr(name);
}


extern char const * account_get_pass(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\passhash1");
}

extern int account_set_pass(t_account * account, char const * passhash1)
{
    return account_set_strattr(account,"BNET\\acct\\passhash1",passhash1);
}


extern int account_unget_pass(char const * pass)
{
    return account_unget_strattr(pass);
}


/****************************************************************/


extern int account_get_auth_admin(t_account * account, char const * channelname)
{
    char temp[256];
    
    if (!channelname)
	return account_get_boolattr(account, "BNET\\auth\\admin");
    
    sprintf(temp,"BNET\\auth\\admin\\%.100s",channelname);
    return account_get_boolattr(account, temp);
}


extern int account_set_auth_admin(t_account * account, char const * channelname, int val)
{
    char temp[256];
    
    if (!channelname)
	return account_set_boolattr(account, "BNET\\auth\\admin", val);
    
    sprintf(temp,"BNET\\auth\\admin\\%.100s",channelname);
    return account_set_boolattr(account, temp, val);
}


extern int account_get_auth_announce(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\announce");
}


extern int account_get_auth_botlogin(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\botlogin");
}


extern int account_get_auth_bnetlogin(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\normallogin");
}


extern int account_get_auth_operator(t_account * account, char const * channelname)
{
    char temp[256];
    
    if (!channelname)
	return account_get_boolattr(account,"BNET\\auth\\operator");
    
    sprintf(temp,"BNET\\auth\\operator\\%.100s",channelname);
    return account_get_boolattr(account,temp);
}

extern int account_set_auth_operator(t_account * account, char const * channelname, int val)
{
    char temp[256];
    
    if (!channelname)
	return account_set_boolattr(account, "BNET\\auth\\operator", val);
	
    sprintf(temp,"BNET\\auth\\operator\\%.100s",channelname);
    return account_set_boolattr(account, temp, val);
}

extern int account_get_auth_voice(t_account * account, char const * channelname)
{
    char temp[256];

    sprintf(temp,"BNET\\auth\\voice\\%.100s",channelname);
    return account_get_boolattr(account,temp);	
}

extern int account_set_auth_voice(t_account * account, char const * channelname, int val)
{
    char temp[256];

    sprintf(temp,"BNET\\auth\\voice\\%.100s",channelname);
    return account_set_boolattr(account, temp, val);
}

extern int account_get_auth_changepass(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\changepass");
}


extern int account_get_auth_changeprofile(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\changeprofile");
}


extern int account_get_auth_createnormalgame(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\createnormalgame");
}


extern int account_get_auth_joinnormalgame(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\joinnormalgame");
}


extern int account_get_auth_createladdergame(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\createladdergame");
}


extern int account_get_auth_joinladdergame(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\joinladdergame");
}


extern int account_get_auth_lock(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\lockk");
}


extern int account_set_auth_lock(t_account * account, int val)
{
    return account_set_boolattr(account,"BNET\\auth\\lockk",val);
}




/****************************************************************/


extern char const * account_get_sex(t_account * account)
{
    char const * temp;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_sex","got NULL account");
	return NULL;
    }
    
    if (!(temp = account_get_strattr(account,"profile\\sex")))
	return "";
    return temp;
}


extern int account_unget_sex(char const * sex)
{
    return account_unget_strattr(sex);
}


extern char const * account_get_age(t_account * account)
{
    char const * temp;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_age","got NULL account");
	return NULL;
    }
    
    if (!(temp = account_get_strattr(account,"profile\\age")))
	return "";
    return temp;
}


extern int account_unget_age(char const * age)
{
    return account_unget_strattr(age);
}


extern char const * account_get_loc(t_account * account)
{
    char const * temp;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_loc","got NULL account");
	return NULL;
    }
    
    if (!(temp = account_get_strattr(account,"profile\\location")))
	return "";
    return temp;
}


extern int account_unget_loc(char const * loc)
{
    return account_unget_strattr(loc);
}


extern char const * account_get_desc(t_account * account)
{
    char const * temp;
    
    if (!account)
    {
	eventlog(eventlog_level_error,"account_get_desc","got NULL account");
	return NULL;
    }
    
    if (!(temp = account_get_strattr(account,"profile\\description")))
	return "";
    return temp;
}


extern int account_unget_desc(char const * desc)
{
    return account_unget_strattr(desc);
}


/****************************************************************/


extern unsigned int account_get_fl_time(t_account * account)
{
    return account_get_numattr(account,"BNET\\acct\\firstlogin_time");
}


extern int account_set_fl_time(t_account * account, unsigned int t)
{
    return account_set_numattr(account,"BNET\\acct\\firstlogin_time",t);
}


extern char const * account_get_fl_clientexe(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_clientexe");
}


extern int account_unget_fl_clientexe(char const * clientexe)
{
    return account_unget_strattr(clientexe);
}


extern int account_set_fl_clientexe(t_account * account, char const * exefile)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_clientexe",exefile);
}


extern char const * account_get_fl_clientver(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_clientver");
}


extern int account_unget_fl_clientver(char const * clientver)
{
    return account_unget_strattr(clientver);
}


extern int account_set_fl_clientver(t_account * account, char const * version)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_clientver",version);
}


extern char const * account_get_fl_clienttag(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_clienttag");
}


extern int account_unget_fl_clienttag(char const * clienttag)
{
    return account_unget_strattr(clienttag);
}


extern int account_set_fl_clienttag(t_account * account, char const * clienttag)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_clienttag",clienttag);
}


extern unsigned int account_get_fl_connection(t_account * account)
{
    return account_get_numattr(account,"BNET\\acct\\firstlogin_connection");
}


extern int account_set_fl_connection(t_account * account, unsigned int connection)
{
    return account_set_numattr(account,"BNET\\acct\\firstlogin_connection",connection);
}


extern char const * account_get_fl_host(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_host");
}


extern int account_unget_fl_host(char const * host)
{
    return account_unget_strattr(host);
}


extern int account_set_fl_host(t_account * account, char const * host)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_host",host);
}


extern char const * account_get_fl_user(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_user");
}


extern int account_unget_fl_user(char const * user)
{
    return account_unget_strattr(user);
}


extern int account_set_fl_user(t_account * account, char const * user)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_user",user);
}
extern char const * account_get_fl_owner(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_owner");
}


extern int account_unget_fl_owner(char const * owner)
{
    return account_unget_strattr(owner);
}


extern int account_set_fl_owner(t_account * account, char const * owner)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_owner",owner);
}


extern char const * account_get_fl_cdkey(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\firstlogin_cdkey");
}


extern int account_unget_fl_cdkey(char const * cdkey)
{
    return account_unget_strattr(cdkey);
}


extern int account_set_fl_cdkey(t_account * account, char const * cdkey)
{
    return account_set_strattr(account,"BNET\\acct\\firstlogin_cdkey",cdkey);
}


/****************************************************************/


extern unsigned int account_get_ll_time(t_account * account)
{
    return account_get_numattr(account,"BNET\\acct\\lastlogin_time");
}


extern int account_set_ll_time(t_account * account, unsigned int t)
{
    return account_set_numattr(account,"BNET\\acct\\lastlogin_time",t);
}


extern char const * account_get_ll_clientexe(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_clientexe");
}


extern int account_unget_ll_clientexe(char const * clientexe)
{
    return account_unget_strattr(clientexe);
}


extern int account_set_ll_clientexe(t_account * account, char const * exefile)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_clientexe",exefile);
}


extern char const * account_get_ll_clientver(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_clientver");
}


extern int account_unget_ll_clientver(char const * clientver)
{
    return account_unget_strattr(clientver);
}


extern int account_set_ll_clientver(t_account * account, char const * version)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_clientver",version);
}


extern char const * account_get_ll_clienttag(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_clienttag");
}


extern int account_unget_ll_clienttag(char const * clienttag)
{
    return account_unget_strattr(clienttag);
}


extern int account_set_ll_clienttag(t_account * account, char const * clienttag)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_clienttag",clienttag);
}


extern unsigned int account_get_ll_connection(t_account * account)
{
    return account_get_numattr(account,"BNET\\acct\\lastlogin_connection");
}


extern int account_set_ll_connection(t_account * account, unsigned int connection)
{
    return account_set_numattr(account,"BNET\\acct\\lastlogin_connection",connection);
}


extern char const * account_get_ll_host(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_host");
}


extern int account_unget_ll_host(char const * host)
{
    return account_unget_strattr(host);
}


extern int account_set_ll_host(t_account * account, char const * host)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_host",host);
}


extern char const * account_get_ll_user(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_user");
}


extern int account_unget_ll_user(char const * user)
{
    return account_unget_strattr(user);
}


extern int account_set_ll_user(t_account * account, char const * user)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_user",user);
}


extern char const * account_get_ll_owner(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_owner");
}


extern int account_unget_ll_owner(char const * owner)
{
    return account_unget_strattr(owner);
}


extern int account_set_ll_owner(t_account * account, char const * owner)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_owner",owner);
}


extern char const * account_get_ll_cdkey(t_account * account)
{
    return account_get_strattr(account,"BNET\\acct\\lastlogin_cdkey");
}


extern int account_unget_ll_cdkey(char const * cdkey)
{
    return account_unget_strattr(cdkey);
}


extern int account_set_ll_cdkey(t_account * account, char const * cdkey)
{
    return account_set_strattr(account,"BNET\\acct\\lastlogin_cdkey",cdkey);
}

/***************************************************************/
// aaron:
// cleanup function for those ppl that don't want or need all
// the accounting infos logged in the accounts

extern int account_remove_verbose_accounting(t_account * account)
{
  if (account_get_fl_time(account)!=0)
    {
      account_set_strattr(account,"BNET\\acct\\firstlogin_time",NULL);
      account_set_strattr(account,"BNET\\acct\\firstlogin_connection",NULL);
      account_set_fl_host(account,NULL);
      account_set_fl_user(account,NULL);
      account_set_fl_clientexe(account,NULL);
      account_set_fl_clienttag(account,NULL);
      account_set_fl_clientver(account,NULL);
      account_set_fl_owner(account,NULL);
      account_set_fl_cdkey(account,NULL);
      account_set_strattr(account,"BNET\\acct\\lastlogin_connection",NULL);
      account_set_ll_user(account,NULL);
      account_set_ll_clientexe(account,NULL);
      account_set_ll_clienttag(account,NULL);
      account_set_ll_clientver(account,NULL);
      account_set_ll_cdkey(account,NULL);
    }
    return 0;
}

/****************************************************************/


extern unsigned int account_get_normal_wins(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_wins","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\wins",clienttag);
    return account_get_numattr(account,key);
}


extern int account_inc_normal_wins(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_inc_normal_wins","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\wins",clienttag);
    return account_set_numattr(account,key,account_get_normal_wins(account,clienttag)+1);
}


extern unsigned int account_get_normal_losses(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_losses","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\losses",clienttag);
    return account_get_numattr(account,key);
}


extern int account_inc_normal_losses(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_inc_normal_losses","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\losses",clienttag);
    return account_set_numattr(account,key,account_get_normal_losses(account,clienttag)+1);
}


extern unsigned int account_get_normal_draws(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_draws","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\draws",clienttag);
    return account_get_numattr(account,key);
}


extern int account_inc_normal_draws(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_inc_normal_draws","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\draws",clienttag);
    return account_set_numattr(account,key,account_get_normal_draws(account,clienttag)+1);
}


extern unsigned int account_get_normal_disconnects(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_disconnects","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\disconnects",clienttag);
    return account_get_numattr(account,key);
}


extern int account_inc_normal_disconnects(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_inc_normal_disconnects","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\disconnects",clienttag);
    return account_set_numattr(account,key,account_get_normal_disconnects(account,clienttag)+1);
}


extern int account_set_normal_last_time(t_account * account, char const * clienttag, t_bnettime t)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_last_time","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\last game",clienttag);
    return account_set_strattr(account,key,bnettime_get_str(t));
}


extern int account_set_normal_last_result(t_account * account, char const * clienttag, char const * result)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_last_result","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\last game result",clienttag);
    return account_set_strattr(account,key,result);
}


/****************************************************************/


extern unsigned int account_get_ladder_active_wins(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_wins","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active wins",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_wins(t_account * account, char const * clienttag, t_ladder_id id, unsigned int wins)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_wins","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active wins",clienttag,(int)id);
    return account_set_numattr(account,key,wins);
}


extern unsigned int account_get_ladder_active_losses(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_losses","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active losses",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_losses(t_account * account, char const * clienttag, t_ladder_id id, unsigned int losses)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_losses","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active losses",clienttag,(int)id);
    return account_set_numattr(account,key,losses);
}


extern unsigned int account_get_ladder_active_draws(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_draws","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active draws",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_draws(t_account * account, char const * clienttag, t_ladder_id id, unsigned int draws)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_draws","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active draws",clienttag,(int)id);
    return account_set_numattr(account,key,draws);
}


extern unsigned int account_get_ladder_active_disconnects(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_disconnects","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active disconnects",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_disconnects(t_account * account, char const * clienttag, t_ladder_id id, unsigned int disconnects)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_disconnects","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active disconnects",clienttag,(int)id);
    return account_set_numattr(account,key,disconnects);
}


extern unsigned int account_get_ladder_active_rating(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_rating","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active rating",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_rating(t_account * account, char const * clienttag, t_ladder_id id, unsigned int rating)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_rating","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active rating",clienttag,(int)id);
    return account_set_numattr(account,key,rating);
}


extern unsigned int account_get_ladder_active_rank(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_rank","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\active rank",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_active_rank(t_account * account, char const * clienttag, t_ladder_id id, unsigned int rank)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_rank","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active rank",clienttag,(int)id);
    return account_set_numattr(account,key,rank);
}


extern char const * account_get_ladder_active_last_time(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_active_last_time","got bad clienttag");
	return NULL;
    }
    sprintf(key,"Record\\%s\\%d\\active last game",clienttag,(int)id);
    return account_get_strattr(account,key);
}


extern int account_set_ladder_active_last_time(t_account * account, char const * clienttag, t_ladder_id id, t_bnettime t)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_active_last_time","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\active last game",clienttag,(int)id);
    return account_set_strattr(account,key,bnettime_get_str(t));
}


/****************************************************************/


extern unsigned int account_get_ladder_wins(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_wins","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\wins",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_inc_ladder_wins(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_inc_ladder_wins","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\wins",clienttag,(int)id);
    return account_set_numattr(account,key,account_get_ladder_wins(account,clienttag,id)+1);
}


extern unsigned int account_get_ladder_losses(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_losses","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\losses",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_inc_ladder_losses(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
       eventlog(eventlog_level_error,"account_inc_ladder_losses","got bad clienttag");
       return -1;
    }
    sprintf(key,"Record\\%s\\%d\\losses",clienttag,(int)id);
    return account_set_numattr(account,key,account_get_ladder_losses(account,clienttag,id)+1);
}


extern unsigned int account_get_ladder_draws(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_draws","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\draws",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_inc_ladder_draws(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
       eventlog(eventlog_level_error,"account_inc_ladder_draws","got bad clienttag");
       return -1;
    }
    sprintf(key,"Record\\%s\\%d\\draws",clienttag,(int)id);
    return account_set_numattr(account,key,account_get_ladder_draws(account,clienttag,id)+1);
}


extern unsigned int account_get_ladder_disconnects(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_disconnects","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\disconnects",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_inc_ladder_disconnects(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
       eventlog(eventlog_level_error,"account_inc_ladder_disconnects","got bad clienttag");
       return -1;
    }
    sprintf(key,"Record\\%s\\%d\\disconnects",clienttag,(int)id);
    return account_set_numattr(account,key,account_get_ladder_disconnects(account,clienttag,id)+1);
}


extern unsigned int account_get_ladder_rating(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_rating","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\rating",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_adjust_ladder_rating(t_account * account, char const * clienttag, t_ladder_id id, int delta)
{
    char         key[256];
    unsigned int oldrating;
    unsigned int newrating;
    int          retval=0;
    
    if (!clienttag || strlen(clienttag)!=4)
    {
       eventlog(eventlog_level_error,"account_adjust_ladder_rating","got bad clienttag");
       return -1;
    }
    sprintf(key,"Record\\%s\\%d\\rating",clienttag,(int)id);
    /* don't allow rating to go below 1 */
    oldrating = account_get_ladder_rating(account,clienttag,id);
    if (delta<0 && oldrating<=(unsigned int)-delta)
        newrating = 1;
    else
        newrating = oldrating+delta;
    if (account_set_numattr(account,key,newrating)<0)
	retval = -1;
    
    if (newrating>account_get_ladder_high_rating(account,clienttag,id))
    {
	sprintf(key,"Record\\%s\\%d\\high rating",clienttag,(int)id);
	if (account_set_numattr(account,key,newrating)<0)
	    retval = -1;
    }
    
    return retval;
}


extern unsigned int account_get_ladder_rank(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_rank","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\rank",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_rank(t_account * account, char const * clienttag, t_ladder_id id, unsigned int rank)
{
    char         key[256];
    unsigned int oldrank;
    int          retval=0;
    
    if (!clienttag || strlen(clienttag)!=4)
    {
       eventlog(eventlog_level_error,"account_set_ladder_rank","got bad clienttag");
       return -1;
    }
    if (rank==0)
        eventlog(eventlog_level_warn,"account_set_ladder_rank","setting rank to zero?");
    sprintf(key,"Record\\%s\\%d\\rank",clienttag,(int)id);
    if (account_set_numattr(account,key,rank)<0)
	retval = -1;
    
    oldrank = account_get_ladder_high_rank(account,clienttag,id);
    if (oldrank==0 || rank<oldrank)
    {
	sprintf(key,"Record\\%s\\%d\\high rank",clienttag,(int)id);
	if (account_set_numattr(account,key,rank)<0)
	    retval = -1;
    }
    
    return retval;
}


extern unsigned int account_get_ladder_high_rating(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_high_rating","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\high rating",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern unsigned int account_get_ladder_high_rank(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_high_rank","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\%d\\high_rank",clienttag,(int)id);
    return account_get_numattr(account,key);
}


extern int account_set_ladder_last_time(t_account * account, char const * clienttag, t_ladder_id id, t_bnettime t)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_last_time","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\last game",clienttag,(int)id);
    return account_set_strattr(account,key,bnettime_get_str(t));
}


extern char const * account_get_ladder_last_time(t_account * account, char const * clienttag, t_ladder_id id)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_ladder_last_time","got bad clienttag");
	return NULL;
    }
    sprintf(key,"Record\\%s\\%d\\last game",clienttag,(int)id);
    return account_get_strattr(account,key);
}


extern int account_set_ladder_last_result(t_account * account, char const * clienttag, t_ladder_id id, char const * result)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_ladder_last_result","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\%d\\last game result",clienttag,(int)id);
    return account_set_strattr(account,key,result);
}


/****************************************************************/


extern unsigned int account_get_normal_level(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_level","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\level",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_level(t_account * account, char const * clienttag, unsigned int level)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_level","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\level",clienttag);
    return account_set_numattr(account,key,level);
}


extern unsigned int account_get_normal_class(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_class","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\class",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_class(t_account * account, char const * clienttag, unsigned int class)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_class","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\class",clienttag);
    return account_set_numattr(account,key,class);
}


extern unsigned int account_get_normal_diablo_kills(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_diablo_kills","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\diablo kills",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_diablo_kills(t_account * account, char const * clienttag, unsigned int diablo_kills)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_diablo_kills","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\diablo kills",clienttag);
    return account_set_numattr(account,key,diablo_kills);
}


extern unsigned int account_get_normal_strength(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_strength","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\strength",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_strength(t_account * account, char const * clienttag, unsigned int strength)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_strength","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\strength",clienttag);
    return account_set_numattr(account,key,strength);
}


extern unsigned int account_get_normal_magic(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_magic","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\magic",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_magic(t_account * account, char const * clienttag, unsigned int magic)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_magic","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\magic",clienttag);
    return account_set_numattr(account,key,magic);
}


extern unsigned int account_get_normal_dexterity(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_dexterity","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\dexterity",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_dexterity(t_account * account, char const * clienttag, unsigned int dexterity)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_dexterity","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\dexterity",clienttag);
    return account_set_numattr(account,key,dexterity);
}


extern unsigned int account_get_normal_vitality(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_vitality","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\vitality",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_vitality(t_account * account, char const * clienttag, unsigned int vitality)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_vitality","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\vitality",clienttag);
    return account_set_numattr(account,key,vitality);
}


extern unsigned int account_get_normal_gold(t_account * account, char const * clienttag)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_normal_gold","got bad clienttag");
	return 0;
    }
    sprintf(key,"Record\\%s\\0\\gold",clienttag);
    return account_get_numattr(account,key);
}


extern int account_set_normal_gold(t_account * account, char const * clienttag, unsigned int gold)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_normal_gold","got bad clienttag");
	return -1;
    }
    sprintf(key,"Record\\%s\\0\\gold",clienttag);
    return account_set_numattr(account,key,gold);
}


/****************************************************************/


extern int account_check_closed_character(t_account * account, char const * clienttag, char const * realmname, char const * charname)
{
    char const * charlist = account_get_closed_characterlist (account, clienttag, realmname);
    char         tempname[32];

    if (charlist == NULL)
    {
        eventlog(eventlog_level_debug,"account_check_closed_character","no characters in Realm %s",realmname);
    }
    else
    {
        char const * start;
	char const * next_char;
	int    list_len;
	int    name_len;
	int    i;

	eventlog(eventlog_level_debug,"account_check_closed_character","got characterlist \"%s\" for Realm %s",charlist,realmname);

	list_len  = strlen(charlist);
	start     = charlist;
	next_char = start;
	for (i = 0; i < list_len; i++, next_char++)
	{
	    if (',' == *next_char)
	    {
	        name_len = next_char - start;

	        strncpy(tempname, start, name_len);
		tempname[name_len] = '\0';

	        eventlog(eventlog_level_debug,"account_check_closed_character","found character \"%s\"",tempname);

		if (strcmp(tempname, charname) == 0)
		    return 1;

		start = next_char + 1;
	    }
	}

	name_len = next_char - start;

	strncpy(tempname, start, name_len);
	tempname[name_len] = '\0';

	eventlog(eventlog_level_debug,"account_check_closed_character","found tail character \"%s\"",tempname);

	if (strcmp(tempname, charname) == 0)
	    return 1;
    }
    
    return 0;
}


extern char const * account_get_closed_characterlist(t_account * account, char const * clienttag, char const * realmname)
{
    char realmkey[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_get_closed_characterlist","got bad clienttag");
	return NULL;
    }

    if (!realmname)
    {
        eventlog(eventlog_level_error,"account_get_closed_characterlist","got NULL realmname");
	return NULL;
    }

    if (!account)
    {
        eventlog(eventlog_level_error,"account_get_closed_characterlist","got NULL account");
	return NULL;
    }

    sprintf(realmkey,"BNET\\CharacterList\\%s\\%s\\0",clienttag,realmname);
    eventlog(eventlog_level_debug,"account_get_closed_characterlist","looking for '%s'",realmkey);

    return account_get_strattr(account, realmkey);
}


extern int account_unget_closed_characterlist(t_account * account, char const * charlist)
{
    return account_unget_strattr(charlist);
}


extern int account_set_closed_characterlist(t_account * account, char const * clienttag, char const * charlist)
{
    char key[256];
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_set_closed_characterlist","got bad clienttag");
	return -1;
    }

    eventlog(eventlog_level_debug,"account_set_closed_characterlist, clienttag='%s', charlist='%s'",clienttag,charlist);

    sprintf(key,"BNET\\Characters\\%s\\0",clienttag);
    return account_set_strattr(account,key,charlist);
}

extern int account_add_closed_character(t_account * account, char const * clienttag, t_character * ch)
{
    char key[256];
    char hex_buffer[356];
    char chars_in_realm[256];
    char const * old_list;
    
    if (!clienttag || strlen(clienttag)!=4)
    {
	eventlog(eventlog_level_error,"account_add_closed_character","got bad clienttag");
	return -1;
    }

    if (!ch)
    {
	eventlog(eventlog_level_error,"account_add_closed_character","got NULL character");
	return -1;
    }

    eventlog(eventlog_level_debug,"account_add_closed_character","clienttag=\"%s\", realm=\"%s\", name=\"%s\"",clienttag,ch->realmname,ch->name);

    sprintf(key,"BNET\\CharacterList\\%s\\%s\\0",clienttag,ch->realmname);
    old_list = account_get_strattr(account, key);
    if (old_list)
    {
        sprintf(chars_in_realm, "%s,%s", old_list, ch->name);
    }
    else
    {
        sprintf(chars_in_realm, "%s", ch->name);
    }

    eventlog(eventlog_level_debug,"account_add_closed_character","new character list for realm \"%s\" is \"%s\"", ch->realmname, chars_in_realm);
    account_set_strattr(account, key, chars_in_realm);

    sprintf(key,"BNET\\Characters\\%s\\%s\\%s\\0",clienttag,ch->realmname,ch->name);
    str_to_hex(hex_buffer, ch->data, ch->datalen);
    account_set_strattr(account,key,hex_buffer);

    /*
    eventlog(eventlog_level_debug,"account_add_closed_character","key \"%s\"", key);
    eventlog(eventlog_level_debug,"account_add_closed_character","value \"%s\"", hex_buffer);
    */

    return 0;
}

/* ADDED BY THE UNDYING SOULZZ 4/10/02 - Clan Name for Profile Setting */
extern int account_set_w3_clanname( t_account * account, char const * acctsetclanname )
{
	if ( acctsetclanname == NULL )
	{
	eventlog( eventlog_level_error,"account_set_w3_acctclanname","got NULL Clan Name. Not setting." );
	return -1;
    }
	eventlog( eventlog_level_debug,"account_set_w3_clanname","setting clanname to %s", acctsetclanname );
	return account_set_strattr( account, "profile\\clanname", acctsetclanname );
}


extern char const * account_get_w3_clanname( t_account * account)
{
    if ( account_get_strattr( account, "profile\\clanname") == NULL ) /* doesn't exist, so add it */
    {
   	account_set_w3_clanname( account, "" );	/* add line to account file but set with a NULL/Nothing*/
	if ( account_get_strattr(account, "profile\\clanname") == NULL )
	{
	    eventlog( eventlog_level_error, "account_get_w3_clanname", "User has not defined a clan name for /CLAN option" );
	    return NULL;
        }	
    }

    return account_get_strattr( account, "profile\\clanname" );
}
// THEUNDYING 5/24/02 - PROFILE GET WINS/LOSSES/LEVELS..etc.. 


extern int account_set_friend( t_account * account, int friendnum, unsigned int frienduid )
{
	char key[256];
	if ( frienduid == 0 || friendnum < 0 || friendnum >= prefs_get_max_friends())
	{
		return -1;
	}
	sprintf(key, "friend\\%d\\uid", friendnum);

	return account_set_numattr( account, key, frienduid);
}

extern unsigned int account_get_friend( t_account * account, int friendnum)
{
    char key[256];
    int tmp;
    char const * name;
    t_account * acct;

    if (friendnum < 0 || friendnum >= prefs_get_max_friends()) {
	// bogus name (user himself) instead of NULL, otherwise clients might crash
	eventlog(eventlog_level_error, __FUNCTION__, "invalid friendnum %d (max: %d)", friendnum, prefs_get_max_friends());
	return 0;  
    }

    sprintf(key, "friend\\%d\\uid", friendnum);
    tmp = account_get_numattr(account, key);
    if(!tmp) {
        // ok, looks like we have a problem. Maybe friends still stored in old format?

        sprintf(key,"friend\\%d\\name",friendnum);
        name = account_get_strattr(account,key);

        if (name) 
        {
    	    if ((acct = accountlist_find_account(name)) != NULL)
            {
        	tmp = account_get_uid(acct);
                account_set_friend(account,friendnum,tmp);
                account_set_strattr(account,key,NULL); //remove old username-based friend now                  

                return tmp;
	    }
            account_set_strattr(account,key,NULL); //remove old username-based friend now                  
	    eventlog(eventlog_level_warn, __FUNCTION__, "unexistant friend name ('%s') in old storage format", name);
	    return 0;
        }

	eventlog(eventlog_level_error, __FUNCTION__, "could not find friend (friendno: %d of '%s')", friendnum, account_get_name(account));
	return 0;
    }

    return tmp;
}

extern int account_set_friendcount( t_account * account, int count)
{
	if (count < 0 || count > prefs_get_max_friends())
	{
		return -1;
	}

	return account_set_numattr( account, "friend\\count", count);
}

extern int account_get_friendcount( t_account * account )
{
    return account_get_numattr( account, "friend\\count" );
}

extern int account_add_friend( t_account * my_acc, t_account * facc)
{
    unsigned my_uid;
    unsigned fuid;
    int nf;
    t_list *flist;

    if (my_acc == NULL || facc == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    my_uid = account_get_uid(my_acc);
    fuid = account_get_uid(facc);

    if (my_acc == facc) return -2;

    nf = account_get_friendcount(my_acc);
    if (nf >= prefs_get_max_friends()) return -3;

    flist = account_get_friends(my_acc);
    if (flist == NULL) return -1;
    if (friendlist_find_account(flist, facc) != NULL) return -4;

    account_set_friend(my_acc, nf, fuid);
    account_set_friendcount(my_acc, nf + 1);
    if (account_check_mutual(facc, my_uid) == 0)
	friendlist_add_account(flist, facc, 1);
    else
	friendlist_add_account(flist, facc, 0);

    return 0;
}

extern int account_remove_friend( t_account * account, int friendnum )
{
    unsigned i;
    int n = account_get_friendcount(account);

    if (account == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    if (friendnum < 0 || friendnum >= n) {
	eventlog(eventlog_level_error, __FUNCTION__, "got invalid friendnum (friendnum: %d max: %d)", friendnum, n);
	return -1;
    }

    for(i = friendnum ; i < n - 1; i++)
	account_set_friend(account, i, account_get_friend(account, i + 1));

    account_set_friend(account, n-1, 0); /* FIXME: should delete the attribute */
    account_set_friendcount(account, n-1);

    return 0;
}

extern int account_remove_friend2( t_account * account, const char * friend)
{
    t_list *flist;
    t_friend *fr;
    unsigned i, uid;
    int n;

    if (account == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    if (friend == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL friend username");
	return -1;
    }

    if ((flist = account_get_friends(account)) == NULL)
	return -1;

    if ((fr = friendlist_find_username(flist, friend)) == NULL) return -2;

    n = account_get_friendcount(account);
    uid = account_get_uid(friend_get_account(fr));
    for (i = 0; i < n; i++)
	if (account_get_friend(account, i) == uid) {
	    account_remove_friend(account, i);
	    friendlist_remove_friend(flist, fr);
	    return i;
	}

    return -2;
}

// Some Extra Commands for REAL admins to promote other users to Admins
// And Moderators of channels
extern int account_set_admin( t_account * account )
{
	return account_set_strattr( account, "BNET\\auth\\admin", "true");
}
extern int account_set_demoteadmin( t_account * account )
{
	return account_set_strattr( account, "BNET\\auth\\admin", "false" );
}

extern unsigned int account_get_command_groups(t_account * account)
{
    return account_get_numattr(account,"BNET\\auth\\command_groups");
}
extern int account_set_command_groups(t_account * account, unsigned int groups)
{
    return account_set_numattr(account,"BNET\\auth\\command_groups",groups);
}

// WAR3 Play Game & Profile Funcs

extern char const * race_get_str(unsigned int race)
{
	switch(race) {
	case W3_RACE_ORCS:
	    return "orcs";
	case W3_RACE_HUMANS:
	    return "humans";
	case W3_RACE_UNDEAD:
	    return "undead";
	case W3_RACE_NIGHTELVES:
	    return "nightelves";
	case W3_RACE_RANDOM:
	    return "random";
	case W3_ICON_UNDEAD:
		return "undead";
	case W3_ICON_RANDOM:
		return "random";
	case W3_ICON_DEMONS:
		return "demons";
	default:
	    eventlog(eventlog_level_warn,"race_get_str","unknown race: %x", race);
	    return NULL;
	}
}

extern int account_set_racewin( t_account * account, unsigned int intrace, char const * clienttag)
{
	char table[256];
	unsigned int wins;
	char const * race = race_get_str(intrace);

	if(!race)
	    return -1;

	sprintf(table,"Record\\%s\\%s\\wins",clienttag, race);
	wins = account_get_numattr(account,table);
	wins++;
		
	return account_set_numattr(account,table,wins);
}

extern int account_get_racewin( t_account * account, unsigned int intrace, char const * clienttag)
{	
	char table[256];
	char const *race = race_get_str(intrace);

	if(!race)
	    return 0;
	
	sprintf(table,"Record\\%s\\%s\\wins",clienttag, race);
	return account_get_numattr(account,table);
}

extern int account_set_raceloss( t_account * account, unsigned int intrace, char const * clienttag)
{
	char table[256];
	unsigned int losses;
	char const *race = race_get_str(intrace);

	if(!race)
	    return -1;

	sprintf(table,"Record\\%s\\%s\\losses",clienttag,race);

	losses=account_get_numattr(account,table);
	
	losses++;
	
	return account_set_numattr(account,table,losses);
	
}

extern int account_get_raceloss( t_account * account, unsigned int intrace, char const * clienttag)
{	
	char table[256];
	char const *race = race_get_str(intrace);
	
	if(!race)
	    return 0;
	
	sprintf(table,"Record\\%s\\%s\\losses",clienttag,race);
	
	return account_get_numattr(account,table);
	
}	

extern int account_set_solowin( t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int win;

  sprintf(key,"Record\\%s\\solo\\wins",clienttag);

  win = account_get_numattr(account,key);
  win++;
  return account_set_numattr(account,key,win);
}

extern int account_get_solowin( t_account * account, char const * clienttag )
{
  char key[256];

  sprintf(key,"Record\\%s\\solo\\wins",clienttag);

  return account_get_numattr(account,key);
}

extern int account_set_sololoss( t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int loss;

  sprintf(key,"Record\\%s\\solo\\losses",clienttag);
  
  loss = account_get_numattr(account,key);
  loss++;
  return account_set_numattr(account,key,loss);
}

extern int account_get_sololoss( t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\solo\\losses",clienttag);

  return account_get_numattr(account,key);
}

extern int account_set_soloxp(t_account * account, char const * clienttag, t_game_result gameresult, unsigned int opponlevel, int * xp_diff)
{ 
  char key[256];
  int xp;
  int mylevel;
  int xpdiff = 0, placeholder;
  
  xp = account_get_soloxp(account, clienttag); //get current xp
  if (xp < 0) {
    eventlog(eventlog_level_error, "account_set_soloxp", "got negative XP");
    return -1;
  }
   
  mylevel = account_get_sololevel(account,clienttag); //get accounts level
  if (mylevel > W3_XPCALC_MAXLEVEL) {
    eventlog(eventlog_level_error, "account_set_soloxp", "got invalid level: %d", mylevel);
    return -1;
  }
  
  if(mylevel<=0) //if level is 0 then set it to 1
    mylevel=1;
  
  if (opponlevel < 1) opponlevel = 1;
  
  switch (gameresult) 
    {
    case game_result_win:
      ladder_war3_xpdiff(mylevel, opponlevel, &xpdiff, &placeholder); break;
    case game_result_loss:
      ladder_war3_xpdiff(opponlevel, mylevel, &placeholder, &xpdiff); break;
    default:
      eventlog(eventlog_level_error, "account_set_soloxp", "got invalid game result: %d", gameresult);
      return -1;
    }

  *xp_diff = xpdiff;
  xp += xpdiff;
  if (xp < 0) xp = 0;

  sprintf(key,"Record\\%s\\solo\\xp",clienttag);
  
  return account_set_numattr(account,key,xp);
}

extern int account_get_soloxp(t_account * account, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Record\\%s\\solo\\xp",clienttag);
  
  return account_get_numattr(account,key);
}

extern int account_set_sololevel(t_account * account, char const * clienttag)
{ 
  char key[256];

  int xp, mylevel;
  
  xp = account_get_soloxp(account, clienttag);
  if (xp < 0) xp = 0;
   
  mylevel = account_get_sololevel(account,clienttag);
  if (mylevel < 1) mylevel = 1;
   
  if (mylevel > W3_XPCALC_MAXLEVEL) {
    eventlog(eventlog_level_error, "account_set_sololevel", "got invalid level: %d", mylevel);
    return -1;
  }
   
  mylevel = ladder_war3_updatelevel(mylevel, xp);

  sprintf(key,"Record\\%s\\solo\\level",clienttag);

  return account_set_numattr(account, key, mylevel);
}

extern int account_get_sololevel(t_account * account, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Record\\%s\\solo\\level",clienttag);

  return account_get_numattr(account,key);
}

// 11-20-2002 aaron -->
extern int account_set_solorank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\solo\\rank",clienttag);

  return account_set_numattr(account, key ,war3_ladder_get_rank(solo_ladder(clienttag),account_get_uid(account),0,clienttag));
}

extern int account_get_solorank_ladder(t_account * account, char const * clienttag)
{
   return war3_ladder_get_rank(solo_ladder(clienttag), account_get_uid(account),0,clienttag);
}

extern int account_set_solorank_ladder(t_account * account, char const * clienttag, int rank)
{
  char key[256];

  sprintf(key,"Record\\%s\\solo\\rank",clienttag);

  return account_set_numattr(account, key, rank);
}

extern int account_get_solorank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\solo\\rank",clienttag);
  
  return account_get_numattr(account,key);
}

// Team Game - Play Game Funcs
extern int account_set_teamwin(t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int win;

  sprintf(key,"Record\\%s\\team\\wins",clienttag);
  
  win = account_get_numattr(account,key);
  win++;
  return account_set_numattr(account,key,win);
}

extern int account_get_teamwin(t_account * account, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Record\\%s\\team\\wins",clienttag);
  
  return account_get_numattr(account,key);
}

extern int account_set_teamloss(t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int loss;

  sprintf(key,"Record\\%s\\team\\losses",clienttag);

  loss = account_get_numattr(account,key);
  loss++;
  return account_set_numattr(account,key,loss);
}

extern int account_get_teamloss(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\losses",clienttag);

  return account_get_numattr(account,key);
}

extern int account_set_teamxp(t_account * account, char const * clienttag, t_game_result gameresult, unsigned int opponlevel,int * xp_diff)
{
  char key[256];
  int xp;
  int mylevel;
  int xpdiff = 0, placeholder;

  xp = account_get_teamxp(account,clienttag); //get current xp
  if (xp < 0) {
    eventlog(eventlog_level_error, "account_set_teamxp", "got negative XP");
    return -1;
  }

  mylevel = account_get_teamlevel(account,clienttag); //get accounts level
  if (mylevel > W3_XPCALC_MAXLEVEL) {
    eventlog(eventlog_level_error, "account_set_teamxp", "got invalid level: %d", mylevel);
    return -1;
  }

  if(mylevel<=0) //if level is 0 then set it to 1
    mylevel=1;
  
  if (opponlevel < 1) opponlevel = 1;
  
  switch (gameresult) 
    {
    case game_result_win:
      ladder_war3_xpdiff(mylevel, opponlevel, &xpdiff, &placeholder); break;
    case game_result_loss:
      ladder_war3_xpdiff(opponlevel, mylevel, &placeholder, &xpdiff); break;
    default:
      eventlog(eventlog_level_error, "account_set_atteamxp", "got invalid game result: %d", gameresult);
      return -1;
    }  

  *xp_diff = xpdiff;

  xp += xpdiff;
  if(xp<0) xp=0;
  
  sprintf(key,"Record\\%s\\team\\xp",clienttag);

  return account_set_numattr(account, key, xp);
}

extern int account_get_teamxp(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\xp",clienttag);
  
  return account_get_numattr(account, key);
}

extern int account_set_teamlevel(t_account * account, char const * clienttag)
{ 
  char key[256];
  int xp,mylevel;
  
  xp = account_get_teamxp(account,clienttag);
  
  if (xp<0) xp =0;

  mylevel = account_get_teamlevel(account,clienttag);
  if (mylevel < 1) mylevel = 1;
  
  if (mylevel > W3_XPCALC_MAXLEVEL) 
    {
      eventlog(eventlog_level_error, "account_set_teamlevel", "got invalid level: %d", mylevel);
      return -1;
    }
  
  mylevel = ladder_war3_updatelevel(mylevel, xp);
  
  sprintf(key,"Record\\%s\\team\\level",clienttag);

  return account_set_numattr(account, key, mylevel);
}

extern int account_get_teamlevel(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\level",clienttag);
  
  return account_get_numattr(account, key);
}

// 11-20-2002 aaron --->
extern int account_set_teamrank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\rank",clienttag);
  
  return account_set_numattr(account,key,war3_ladder_get_rank(team_ladder(clienttag),account_get_uid(account),0,clienttag));
}

extern int account_get_teamrank_ladder(t_account * account, char const * clienttag)
{
  
  return war3_ladder_get_rank(team_ladder(clienttag),account_get_uid(account),0,clienttag);
}

extern int account_set_teamrank_ladder(t_account * account, char const * clienttag, int rank)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\rank",clienttag);
  
  return account_set_numattr(account,key,rank);
}

extern int account_get_teamrank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\team\\rank",clienttag);
  
  return account_get_numattr(account,key);
}

// FFA code
extern int account_set_ffawin(t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int win;

  sprintf(key,"Record\\%s\\ffa\\wins",clienttag);
  
  win = account_get_numattr(account,key);
  win++;
  return account_set_numattr(account,key,win);
}

extern int account_get_ffawin(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\wins",clienttag);
  
  return account_get_numattr(account,key);
}

extern int account_set_ffaloss(t_account * account, char const * clienttag)
{
  char key[256];
  unsigned int loss;

  sprintf(key,"Record\\%s\\ffa\\losses",clienttag);

  loss = account_get_numattr(account,key);
  loss++;
  return account_set_numattr(account,key,loss);
}

extern int account_get_ffaloss(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\losses",clienttag);
  
  return account_get_numattr(account,key);
}

extern int account_set_ffaxp(t_account * account, char const * clienttag,t_game_result gameresult, unsigned int opponlevel, int * xp_diff)
{ 
  char key[256];
  int xp;
  int mylevel;
  int xpdiff = 0, placeholder;
  xp = account_get_ffaxp(account,clienttag); //get current xp

  xpdiff = 0;
  if (xp < 0) {
    eventlog(eventlog_level_error, "account_set_ffaxp", "got negative XP");
    return -1;
  }
  
  mylevel = account_get_ffalevel(account,clienttag); //get accounts level
  if (mylevel > W3_XPCALC_MAXLEVEL) {
    eventlog(eventlog_level_error, "account_set_ffaxp", "got invalid level: %d", mylevel);
    return -1;
  }

  if(mylevel<=0) //if level is 0 then set it to 1
    mylevel=1;
  
  if (opponlevel < 1) opponlevel = 1;

  switch (gameresult) 
    {
    case game_result_win:
      ladder_war3_xpdiff(mylevel, opponlevel, &xpdiff, &placeholder); break;
    case game_result_loss:
      ladder_war3_xpdiff(opponlevel, mylevel, &placeholder, &xpdiff); break;
    default:
      eventlog(eventlog_level_error, "account_set_atteamxp", "got invalid game result: %d", gameresult);
      return -1;
    }
    
  *xp_diff = xpdiff;
  xp += xpdiff;
  if (xp<0) xp=0;
  
  sprintf(key,"Record\\%s\\ffa\\xp",clienttag);

  return account_set_numattr(account,key,xp);
}

extern int account_get_ffaxp(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\xp",clienttag);

  return account_get_numattr(account,key);
}

extern int account_set_ffalevel(t_account * account, char const * clienttag)
{ 
  char key[256];
  int xp, mylevel;

  xp = account_get_ffaxp(account,clienttag);

  if (xp<0) xp = 0;

  mylevel = account_get_ffalevel(account,clienttag);
  if (mylevel < 1) mylevel = 1;
  
  if (mylevel > W3_XPCALC_MAXLEVEL) 
    {
      eventlog(eventlog_level_error, "account_set_ffalevel", "got invalid level: %d", mylevel);
      return -1;
    }
  
  mylevel = ladder_war3_updatelevel(mylevel, xp);
  
  sprintf(key,"Record\\%s\\ffa\\level",clienttag);

  return account_set_numattr(account, key, mylevel);
}

extern int account_get_ffalevel(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\level",clienttag);
  
  return account_get_numattr(account,key);
}

extern int account_get_ffarank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\rank",clienttag);

  return account_get_numattr(account,key);
}

// aaron --->
extern int account_set_ffarank(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\rank",clienttag);
  
  return account_set_numattr(account,key,war3_ladder_get_rank(ffa_ladder(clienttag),account_get_uid(account),0,clienttag));
}

extern int account_get_ffarank_ladder(t_account * account, char const * clienttag)
{
	return war3_ladder_get_rank(ffa_ladder(clienttag),account_get_uid(account),0,clienttag);
}

extern int account_set_ffarank_ladder(t_account * account, char const * clienttag, int rank)
{
  char key[256];

  sprintf(key,"Record\\%s\\ffa\\rank",clienttag);

  return account_set_numattr(account,key,rank);
}
// <---


//Other funcs used in profiles and PG saving
extern void account_get_raceicon(t_account * account, char * raceicon, unsigned int * raceiconnumber, unsigned int * wins, char const * clienttag) //Based of wins for each race, Race with most wins, gets shown in chat channel
{
	unsigned int humans;
	unsigned int orcs;
	unsigned int undead;
	unsigned int nightelf;
	unsigned int random;
	int icons_limits[] = {25, 250, 500, 1500, -1};
	unsigned int i;

	random = account_get_racewin(account,W3_RACE_RANDOM,clienttag);
	humans = account_get_racewin(account,W3_RACE_HUMANS,clienttag); 
	orcs = account_get_racewin(account,W3_RACE_ORCS,clienttag); 
	undead = account_get_racewin(account,W3_RACE_UNDEAD,clienttag);
	nightelf = account_get_racewin(account,W3_RACE_NIGHTELVES,clienttag);
	if(orcs>=humans && orcs>=undead && orcs>=nightelf && orcs>=random) {
	  *raceicon = 'O';
	  *wins = orcs;
	}
	else if(humans>=orcs && humans>=undead && humans>=nightelf && humans>=random) {
	    *raceicon = 'H';
	    *wins = humans;
	}
	else if(nightelf>=humans && nightelf>=orcs && nightelf>=undead && nightelf>=random) {
	    *raceicon = 'N';
	    *wins = nightelf;
	}
	else if(undead>=humans && undead>=orcs && undead>=nightelf && undead>=random) {
	    *raceicon = 'U';
	    *wins = undead;
	}
	else {
	    *raceicon = 'R';
	    *wins = random;
	}
	i = 0;
	while((signed)*wins >= icons_limits[i] && icons_limits[i] > 0) i++;
	*raceiconnumber = i + 1;
}

extern int account_get_profile_calcs(t_account * account, int xp, unsigned int Level)
{
        int xp_min;
	int xp_max;
	unsigned int i;
	int  t;
	unsigned int startlvl;
	
	if (Level==1) startlvl = 1;
	else startlvl = Level-1;
	for (i = startlvl; i < W3_XPCALC_MAXLEVEL; i++) {
		xp_min = ladder_war3_get_min_xp(i);
		xp_max = ladder_war3_get_min_xp(i+1);
		if ((xp >= xp_min) && (xp < xp_max)) {
			t = (int)((((double)xp - (double)xp_min) 
					/ ((double)xp_max - (double)xp_min)) * 128);
			if (i < Level) {
				return 128 + t;
			} else {
				return t;
			}
		}
	}
	return 0;
}

extern int account_set_saveladderstats(t_account * account,unsigned int gametype, t_game_result result, unsigned int opponlevel, char const * clienttag)
{
	unsigned int intrace;
        int xpdiff,uid,level;

	if(!account) {
		eventlog(eventlog_level_error, "account_set_saveladderstats", "got NULL account");
		return -1;
	}


	//added for better tracking down of problems with gameresults
	eventlog(eventlog_level_trace,__FUNCTION__,"parsing game result for player: %s result: %s",account_get_name(account),(result==game_result_win)?"WIN":"LOSS");

	intrace = account_get_w3pgrace(account, clienttag);
	uid = account_get_uid(account);
	
	switch (gametype)
	{
	  case ANONGAME_TYPE_1V1: //1v1
	  {

		if(result == game_result_win) //win
		{
			account_set_solowin(account, clienttag);
			account_set_racewin(account, intrace, clienttag);
			
		}
		if(result == game_result_loss) //loss
		{
			account_set_sololoss(account, clienttag);
			account_set_raceloss(account,intrace, clienttag);
		}
			
		account_set_soloxp(account,clienttag,result,opponlevel,&xpdiff);
		account_set_sololevel(account,clienttag);
		level = account_get_sololevel(account,clienttag);
		if (war3_ladder_update(solo_ladder(clienttag),uid,xpdiff,level,account,0)!=0)
		  war3_ladder_add(solo_ladder(clienttag),uid,account_get_soloxp(account,clienttag),level,0,account,0,clienttag);
		break;
	  }
	  case ANONGAME_TYPE_2V2:
	  case ANONGAME_TYPE_3V3:
	  case ANONGAME_TYPE_4V4:
	  case ANONGAME_TYPE_5V5:
	  case ANONGAME_TYPE_6V6:
	  case ANONGAME_TYPE_2V2V2:
	  case ANONGAME_TYPE_3V3V3:
	  case ANONGAME_TYPE_4V4V4:
	  case ANONGAME_TYPE_2V2V2V2:
	  case ANONGAME_TYPE_3V3V3V3:
	  {
		if(result == game_result_win) //win
		{
			account_set_teamwin(account,clienttag);
			account_set_racewin(account,intrace,clienttag);
		}
		if(result == game_result_loss) //loss
		{
			account_set_teamloss(account,clienttag);
			account_set_raceloss(account,intrace,clienttag);
		}

		account_set_teamxp(account,clienttag,result,opponlevel,&xpdiff);
		account_set_teamlevel(account,clienttag);
		level = account_get_teamlevel(account,clienttag);
		if (war3_ladder_update(team_ladder(clienttag),uid,xpdiff,level,account,0)!=0)
		  war3_ladder_add(team_ladder(clienttag),uid,account_get_teamxp(account,clienttag),level,0,account,0,clienttag);
		break;
	  }

	  case ANONGAME_TYPE_SMALL_FFA:
	  {
		if(result == game_result_win) //win
		{
			account_set_ffawin(account,clienttag);
			account_set_racewin(account,intrace,clienttag);
		}
		if(result == game_result_loss) //loss
		{
			account_set_ffaloss(account,clienttag);
			account_set_raceloss(account,intrace,clienttag);
		}

		account_set_ffaxp(account,clienttag,result,opponlevel,&xpdiff);
		account_set_ffalevel(account,clienttag);
		level = account_get_ffalevel(account,clienttag);
		if (war3_ladder_update(ffa_ladder(clienttag),uid,xpdiff,level,account,0)!=0)
		  war3_ladder_add(ffa_ladder(clienttag),uid,account_get_ffaxp(account,clienttag),level,0,account,0,clienttag);
                break;
	  }
	  default:
		eventlog(eventlog_level_error,"account_set_saveladderstats","Invalid Gametype? %u",gametype);
	}

	return 0;
}

extern int account_set_w3pgrace( t_account * account, char const * clienttag, unsigned int race)
{
  char key[256];
  sprintf(key,"Record\\%s\\w3pgrace",clienttag);
  return account_set_numattr( account, key, race);
}

extern int account_get_w3pgrace( t_account * account, char const * clienttag )
{
  char key[256];
  sprintf(key,"Record\\%s\\w3pgrace",clienttag);
  return account_get_numattr( account, key);
}
// Arranged Team Functions
extern int account_check_team(t_account * account, const char * members_usernames, char const * clienttag)
{
  int teams_cnt = account_get_atteamcount(account,clienttag);
  int i;
	    
  if(teams_cnt==0) //user has never play at before
    return -1;
	
  for(i=1;i<=teams_cnt;i++)
    {
      char const * members = account_get_atteammembers(account,i,clienttag);
      if(members && members_usernames && strcasecmp(members,members_usernames)==0)
	{
	  eventlog(eventlog_level_debug,"account_check_team","Use have played before! Team num is %d",i);
	  return i;
	}
    }
  //if your here this group of people have never played a AT game yet
  return -1;
}

extern int account_create_newteam(t_account * account, const char * members_usernames, unsigned int teamsize, char const * clienttag)
{
    int teams_cnt = account_get_atteamcount(account,clienttag);

    if (teams_cnt <= 0 ) teams_cnt = 1;
    else teams_cnt++; /* since we making a new team here add+1 to the count */

    account_set_atteamcount(account,clienttag,teams_cnt);
    account_set_atteammembers(account,teams_cnt,clienttag,members_usernames);
    account_set_atteamsize(account,teams_cnt,clienttag,teamsize);

    return teams_cnt;
}

extern int account_set_atteamcount(t_account * account, char const * clienttag, unsigned int teamcount)
{
  char key[256];

  sprintf(key,"Record\\%s\\teamcount",clienttag);
  
  return account_set_numattr(account,key,teamcount);
}

extern int account_get_atteamcount(t_account * account, char const * clienttag)
{
  char key[256];

  sprintf(key,"Record\\%s\\teamcount",clienttag);

  return account_get_numattr(account,key);
}

extern int account_set_atteamsize(t_account * account, unsigned int teamcount, char const * clienttag, unsigned int teamsize)
{
  char key[356];
  
  sprintf(key,"Team\\%s\\%u\\teamsize",clienttag,teamcount);
  
  return account_set_numattr(account,key,teamsize);
}

extern int account_get_atteamsize(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teamsize",clienttag, teamcount);

  return account_get_numattr(account,key);
}

extern int account_set_atteamwin(t_account * account, unsigned int teamcount, char const * clienttag, int wins)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teamwins",clienttag,teamcount);
	
  return account_set_numattr(account,key,wins);
}

extern int account_atteamwin(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  int wins = account_get_atteamwin(account,teamcount,clienttag);
  wins++;
  
  sprintf(key,"Team\\%s\\%u\\teamwins",clienttag,teamcount);
	
  return account_set_numattr(account,key,wins);
}

extern int account_get_atteamwin(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];

  sprintf(key,"Team\\%s\\%u\\teamwins",clienttag,teamcount);
  
  return account_get_numattr(account,key);
}

extern int account_set_atteamloss(t_account * account, unsigned int teamcount, char const * clienttag, int loss)
{
  char key[256];

  sprintf(key,"Team\\%s\\%u\\teamlosses",clienttag,teamcount);
	
  return account_set_numattr(account,key,loss);
}

extern int account_atteamloss(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  int loss = account_get_atteamloss(account,teamcount,clienttag);
  loss++;

  sprintf(key,"Team\\%s\\%u\\teamlosses",clienttag,teamcount);
	
  return account_set_numattr(account,key,loss);
}

extern int account_get_atteamloss(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teamlosses",clienttag,teamcount);

  return account_get_numattr(account,key);
}

extern int account_set_atteamxp(t_account * account, unsigned int teamcount, char const * clienttag, int xp)
{ 
    char key[256];

    sprintf(key,"Team\\%s\\%u\\teamxp",clienttag,teamcount);

    return account_set_numattr(account,key,xp);
}

extern int account_update_atteamxp(t_account * account, t_game_result gameresult, unsigned int opponlevel, unsigned int teamcount, char const * clienttag, int * xp_diff)
{ 
  int xp;
  int mylevel;
  int xpdiff = 0, placeholder;
  char key[256];
   
  xp = account_get_atteamxp(account,teamcount,clienttag); //get current xp
  if (xp < 0) {
    eventlog(eventlog_level_error, "account_set_atteamxp", "got negative XP");
    return -1;
  }
  
  mylevel = account_get_atteamlevel(account,teamcount,clienttag); //get accounts level
  if (mylevel > W3_XPCALC_MAXLEVEL) {
    eventlog(eventlog_level_error, "account_set_atteamxp", "got invalid level: %d", mylevel);
    return -1;
  }
  
  if(mylevel<=0) //if level is 0 then set it to 1
    mylevel=1;
  
  if (opponlevel < 1) opponlevel = 1;
  
  switch (gameresult) 
    {
    case game_result_win:
      ladder_war3_xpdiff(mylevel, opponlevel, &xpdiff, &placeholder); break;
    case game_result_loss:
      ladder_war3_xpdiff(opponlevel, mylevel, &placeholder, &xpdiff); break;
    default:
      eventlog(eventlog_level_error, "account_set_atteamxp", "got invalid game result: %d", gameresult);
      return -1;
    }
  
   *xp_diff = xpdiff;
   
   xp += xpdiff;
   if (xp < 0) xp = 0;
   
   sprintf(key,"Team\\%s\\%u\\teamxp",clienttag,teamcount);

   return account_set_numattr(account,key,xp);
}

extern int account_get_atteamxp(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teamxp",clienttag,teamcount);
  
  return account_get_numattr(account,key);
}

extern int account_set_atteamlevel(t_account * account, unsigned int teamcount, char const * clienttag, int teamlevel)
{
  char key[256];
   
  sprintf(key,"Team\\%s\\%u\\teamlevel",clienttag,teamcount);
  return account_set_numattr(account,key,teamlevel);
}

extern int account_update_atteamlevel(t_account * account, unsigned int teamcount, char const * clienttag)
{
  int xp, mylevel;
  char key[256];
   
  xp = account_get_atteamxp(account,teamcount,clienttag);
   
  if (xp < 0) xp = 0;
   
  mylevel = account_get_atteamlevel(account,teamcount,clienttag);
  if (mylevel < 1) mylevel = 1;
  
  if (mylevel > W3_XPCALC_MAXLEVEL) 
    {
      eventlog(eventlog_level_error, "account_set_atteamlevel", "got invalid level: %d", mylevel);
      return -1;
    }
  
  mylevel = ladder_war3_updatelevel(mylevel, xp);
  sprintf(key,"Team\\%s\\%u\\teamlevel",clienttag,teamcount);
  return account_set_numattr(account,key,mylevel);
}

extern int account_get_atteamlevel(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];

  sprintf(key,"Team\\%s\\%u\\teamlevel",clienttag, teamcount);

  return account_get_numattr(account,key);
}

//aaron 
extern int account_get_atteamrank(t_account * account, unsigned int teamcount,char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\rank",clienttag, teamcount);
  
  return account_get_numattr(account,key);
}

extern int account_set_atteamrank(t_account * account, unsigned int teamcount, char const * clienttag, int teamrank)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\rank",clienttag, teamcount);

  return account_set_numattr(account, key, teamrank);
}

extern int account_update_atteamrank(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\rank",clienttag, teamcount);

  return account_set_numattr(account,key,war3_ladder_get_rank(at_ladder(clienttag),account_get_uid(account),teamcount,clienttag));
}

extern int account_get_atteamrank_ladder(t_account * account, unsigned int teamcount, char const * clienttag)
{
  return war3_ladder_get_rank(at_ladder(clienttag),account_get_uid(account), teamcount,clienttag);
}

extern int account_set_atteamrank_ladder(t_account * account, int rank, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  sprintf(key,"Team\\%s\\%u\\rank",clienttag,teamcount);
  return account_set_numattr(account,key,rank);
}
// <---

extern int account_set_atteammembers(t_account * account, unsigned int teamcount, char const * clienttag, char const *members)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teammembers",clienttag,teamcount);
	
  return account_set_strattr(account,key,members);
}

extern char const * account_get_atteammembers(t_account * account, unsigned int teamcount, char const * clienttag)
{
  char key[256];
  
  sprintf(key,"Team\\%s\\%u\\teammembers",clienttag,teamcount);
  
  return account_get_strattr(account,key);
}

extern int account_set_currentatteam(t_account * account, unsigned int teamcount)
{
	return account_set_numattr(account,"BNET\\current_at_team",teamcount);
}

extern int account_get_currentatteam(t_account * account)
{
	return account_get_numattr(account,"BNET\\current_at_team");
}

extern int account_fix_at(t_account *account, const char * ctag)
{
    int n, i, j;
    int teamsize;
    const char *teammembers;

    if (account == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    if (ctag == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL clienttag");
	return -1;
    }

    n = account_get_atteamcount(account, ctag);
    for(i = 1, j = 1; i <= n ; i++) {
	teamsize = account_get_atteamsize(account, i, ctag);
	teammembers = account_get_atteammembers(account, i, ctag);
	if (!teammembers || teamsize < 1 || teamsize > 5) continue;
	/* valid team */
	if (j < i) {
	    account_set_atteamsize(account, j, ctag, teamsize);
	    account_set_atteammembers(account, j, ctag, teammembers);
	    account_set_atteamwin(account, j, ctag, account_get_atteamwin(account, i, ctag));
	    account_set_atteamloss(account, j, ctag, account_get_atteamloss(account, i, ctag));
	    account_set_atteamxp(account, j, ctag, account_get_atteamxp(account, i, ctag));
	    account_set_atteamrank(account, j, ctag, account_get_atteamrank(account, i, ctag));
	    account_set_atteamlevel(account, j, ctag, account_get_atteamlevel(account, i, ctag));
	}
	j++;
    }

    if (j <= n) account_set_atteamcount(account, ctag, j - 1);
    return 0;
}

extern int account_set_saveATladderstats(t_account * account, unsigned int gametype, t_game_result result, unsigned int opponlevel, unsigned int current_teamnum, char const * clienttag)
{
  unsigned int intrace;
  int xpdiff,uid,level;
	
  if(!account) 
    {
      eventlog(eventlog_level_error, "account_set_saveATladderstats", "got NULL account");
      return -1;
    }
  
    //added for better tracking down of problems with gameresults
    eventlog(eventlog_level_trace,__FUNCTION__,"parsing game result for player: %s result: %s",account_get_name(account),(result==game_result_win)?"WIN":"LOSS");

  intrace = account_get_w3pgrace(account,clienttag);
  uid = account_get_uid(account);
  
  if(result == game_result_win)
    {
      account_atteamwin(account,current_teamnum,clienttag);
      account_set_racewin(account,intrace,clienttag);
    }
  if(result == game_result_loss)
    {
      account_atteamloss(account,current_teamnum,clienttag);
      account_set_raceloss(account,intrace,clienttag);
    }
  account_update_atteamxp(account,result,opponlevel,current_teamnum,clienttag,&xpdiff);
  account_update_atteamlevel(account,current_teamnum,clienttag);
  level = account_get_atteamlevel(account,current_teamnum,clienttag);
  if (war3_ladder_update(at_ladder(clienttag),uid,xpdiff,level,account,0)!=0)
    war3_ladder_add(at_ladder(clienttag),uid,account_get_atteamxp(account,current_teamnum,clienttag),level,0,account,0,clienttag);
  return 0;
}

extern int account_get_highestladderlevel(t_account * account,char const * clienttag)
{
	// [quetzal] 20020827 - AT level part rewritten
	int i;

	unsigned int sololevel = account_get_sololevel(account,clienttag);
	unsigned int teamlevel = account_get_teamlevel(account,clienttag);
	unsigned int ffalevel  = account_get_ffalevel(account,clienttag);
	unsigned int atlevel = 0;
	unsigned int t; // [quetzal] 20020827 - not really needed, but could speed things up a bit
	
	for (i = 0; i < account_get_atteamcount(account,clienttag); i++) {
		if ((t = account_get_atteamlevel(account, i+1,clienttag)) > atlevel) {
			atlevel = t;
		}
	}

	eventlog(eventlog_level_debug,"account_get_highestladderlevel","Checking for highest level in Solo,Team,FFA,AT Ladder Stats");
	eventlog(eventlog_level_debug,"account_get_highestladderlevel","Solo Level: %d, Team Level %d, FFA Level %d, Highest AT Team Level: %d",sololevel,teamlevel,ffalevel,atlevel);
			
	if(sololevel >= teamlevel && sololevel >= atlevel && sololevel >= ffalevel)
		return sololevel;
	if(teamlevel >= sololevel && teamlevel >= atlevel && teamlevel >= ffalevel)
        	return teamlevel;
	if(atlevel >= sololevel && atlevel >= teamlevel && atlevel >= ffalevel)
        	return atlevel;
	return ffalevel;

    // we should never get here

	return -1;
}
extern int account_set_new_at_team(t_account * account, unsigned int value)
{
	if(!account)
	{
		eventlog(eventlog_level_error,"account_set_new_at_team","Unable to set account flag to TRUE");
		return -1;
	}

	return account_set_numattr(account,"BNET\\new_at_team_flag",value);
}
extern int account_get_new_at_team(t_account * account)
{
	if(!account)
	{
		eventlog(eventlog_level_error,"account_get_new_at_team","Unable to set account flag to TRUE");
		return -1;
	}

	return account_get_numattr(account,"BNET\\new_at_team_flag");
}


//BlacKDicK 04/20/2003
extern int account_set_user_icon( t_account * account, char const * clienttag,char const * usericon)
{
  char key[256];
  sprintf(key,"Record\\%s\\userselected_icon",clienttag);
  if (usericon)
    return account_set_strattr(account,key,usericon);
  else
    return account_set_strattr(account,key,"NULL");
}

extern char const * account_get_user_icon( t_account * account, char const * clienttag )
{
  char key[256];
  char const * retval;
  sprintf(key,"Record\\%s\\userselected_icon",clienttag);
  retval = account_get_strattr(account,key);

  if ((retval) && ((strcmp(retval,"NULL")!=0)))
    return retval;
  else
    return NULL;
}

// Ramdom - Nothing, Grean Dragon Whelp, Azure Dragon (Blue Dragon), Red Dragon, Deathwing, Nothing
// Humans - Peasant, Footman, Knight, Archmage, Medivh, Nothing
// Orcs - Peon, Grunt, Tauren, Far Seer, Thrall, Nothing
// Undead - Acolyle, Ghoul, Abomination, Lich, Tichondrius, Nothing
// Night Elves - Wisp, Archer, Druid of the Claw, Priestess of the Moon, Furion Stormrage, Nothing
// Demons - Nothing, ???(wich unit is nfgn), Infernal, Doom Guard, Pit Lord/Manaroth, Archimonde
// ADDED TFT ICON BY DJP 07/16/2003
static char * profile_code[12][6] = {
	    {NULL  , "ngrd", "nadr", "nrdr", "nbwm", NULL  },
	    {"hpea", "hfoo", "hkni", "Hamg", "nmed", NULL  },
	    {"opeo", "ogru", "otau", "Ofar", "Othr", NULL  },
	    {"uaco", "ugho", "uabo", "Ulic", "Utic", NULL  },
	    {"ewsp", "earc", "edoc", "Emoo", "Efur", NULL  },
	    {NULL  , "nfng", "ninf", "nbal", "Nplh", "Uwar"}, /* not used by RoC */
	    {NULL  , "nmyr", "nnsw", "nhyc", "Hvsh", "Eevm"},
	    {"hpea", "hrif", "hsor", "hspt", "Hblm", "Hjai"},
	    {"opeo", "ohun", "oshm", "ospw", "Oshd", "Orex"},
	    {"uaco", "ucry", "uban", "uobs", "Ucrl", "Usyl"},
	    {"ewsp", "esen", "edot", "edry", "Ekee", "Ewrd"},
	    {NULL  , "nfgu", "ninf", "nbal", "Nplh", "Uwar"}
	};

extern unsigned int account_get_icon_profile(t_account * account, char const * clienttag)
{
	unsigned int humans	= account_get_racewin(account,W3_RACE_HUMANS,clienttag);		//  1;
	unsigned int orcs	= account_get_racewin(account,W3_RACE_ORCS,clienttag); 		        //  2;
	unsigned int nightelf	= account_get_racewin(account,W3_RACE_NIGHTELVES,clienttag);	        //  4;
	unsigned int undead	= account_get_racewin(account,W3_RACE_UNDEAD,clienttag);		//  8;
	unsigned int random	= account_get_racewin(account,W3_RACE_RANDOM,clienttag);		// 32;
	unsigned int race; 	     // 0 = Humans, 1 = Orcs, 2 = Night Elves, 3 = Undead, 4 = Ramdom
	unsigned int level	= 0; // 0 = under 25, 1 = 25 to 249, 2 = 250 to 499, 3 = 500 to 1499, 4 = 1500 or more (wins)
	unsigned int wins;
	int number_ctag		= 0;

	/* moved the check for orcs in the first place so people with 0 wins get peon */
        if(orcs>=humans && orcs>=undead && orcs>=nightelf && orcs>=random) {
            wins = orcs;
            race = 2;
        }
        else if(humans>=orcs && humans>=undead && humans>=nightelf && humans>=random) {
	    wins = humans;
            race = 1;
        }
        else if(nightelf>=humans && nightelf>=orcs && nightelf>=undead && nightelf>=random) {
            wins = nightelf;
            race = 4;
        }
        else if(undead>=humans && undead>=orcs && undead>=nightelf && undead>=random) {
            wins = undead;
            race = 3;
        }
        else {
            wins = random;
            race = 0;
        }

	if (strcmp(clienttag,CLIENTTAG_WARCRAFT3)==0) 
	{
          while(wins >= anongame_infos_get_ICON_REQ_WAR3(level+1) && anongame_infos_get_ICON_REQ_WAR3(level+1) > 0) level++;
	}
	else
	{
          while(wins >= anongame_infos_get_ICON_REQ_W3XP(level+1) && anongame_infos_get_ICON_REQ_W3XP(level+1) > 0) level++;
	  number_ctag = 6;
	}
        
        eventlog(eventlog_level_info,"account_get_icon_profile","race -> %u; level -> %u; wins -> %u; profileicon -> %s", race, level, wins, profile_code[race+number_ctag][level]);

	return char_icon_to_uint(profile_code[race+number_ctag][level]);
}

extern unsigned int account_icon_to_profile_icon(char const * icon,t_account * account, char const * ctag)
{
	char tmp_icon[4];
	char * result;
	int number_ctag=0;

	if (icon==NULL) return account_get_icon_profile(account,ctag);
	if (sizeof(icon)>=4){
		strncpy(tmp_icon,icon,4);
		tmp_icon[0]=tmp_icon[0]-48;
		if (strcmp(ctag,CLIENTTAG_WAR3XP) == 0) {
			number_ctag = 6;
		}
		if (tmp_icon[0]>=1) {
			if (tmp_icon[1]=='R'){
				result = profile_code[0+number_ctag][tmp_icon[0]-1];
			}else if (tmp_icon[1]=='H'){
				result = profile_code[1+number_ctag][tmp_icon[0]-1];
			}else if (tmp_icon[1]=='O'){
				result = profile_code[2+number_ctag][tmp_icon[0]-1];
			}else if (tmp_icon[1]=='U'){
				result = profile_code[3+number_ctag][tmp_icon[0]-1];
			}else if (tmp_icon[1]=='N'){
				result = profile_code[4+number_ctag][tmp_icon[0]-1];
			}else if (tmp_icon[1]=='D'){
				result = profile_code[5+number_ctag][tmp_icon[0]-1];
			}else{
				eventlog(eventlog_level_warn,"account_icon_to_profile_icon","got unrecognized race on [%s] icon ",icon);
				result = profile_code[2][0];} /* "opeo" */
			}else{
				eventlog(eventlog_level_warn,"account_icon_to_profile_icon","got race_level<1 on [%s] icon ",icon);
				result = NULL;
			}
	}else{
	    eventlog(eventlog_level_error,"account_icon_to_profile_icon","got invalid icon lenght [%s] icon ",icon);
	    result = NULL;
	}
	//eventlog(eventlog_level_debug,"account_icon_to_profile_icon","from [%4.4s] icon returned [0x%X]",icon,char_icon_to_uint(result));
	return char_icon_to_uint(result);
}

extern int account_is_operator_or_admin(t_account * account, char const * channel)
{
   if ((account_get_auth_operator(account,channel)==1) || (account_get_auth_operator(account,NULL)==1) ||
       (account_get_auth_admin(account,channel)==1) || (account_get_auth_admin(account,NULL)==1) )
      return 1;
   else
      return 0;

}

static unsigned int char_icon_to_uint(char * icon)
{
    unsigned int value;
    
    if (!icon) return 0;
    if (strlen(icon)!=4) return 0;

    value  = ((unsigned int)icon[0])<<24;
    value |= ((unsigned int)icon[1])<<16;
    value |= ((unsigned int)icon[2])<< 8;
    value |= ((unsigned int)icon[3])    ;
    
    return value;
}
