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
#include "command.h"
//aaron
#include "war3ladder.h"

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


extern int account_get_auth_admin(t_account * account)
{
    return account_get_boolattr(account,"BNET\\auth\\admin");
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

#ifdef WITH_MYSQL
extern int accounts_remove_verbose_columns(void)
{
  db_drop_column("BNET\\acct\\firstlogin_time");
  db_drop_column("BNET\\acct\\firstlogin_connection");
  db_drop_column("BNET\\acct\\firstlogin_host");
  db_drop_column("BNET\\acct\\firstlogin_user");
  db_drop_column("BNET\\acct\\firstlogin_clientexe");
  db_drop_column("BNET\\acct\\firstlogin_clienttag");
  db_drop_column("BNET\\acct\\firstlogin_clientver");
  db_drop_column("BNET\\acct\\firstlogin_owner");
  db_drop_column("BNET\\acct\\firstlogin_cdkey");
  db_drop_column("BNET\\acct\\lastlogin_connection");
  db_drop_column("BNET\\acct\\lastlogin_user");
  db_drop_column("BNET\\acct\\lastlogin_clientexe");
  db_drop_column("BNET\\acct\\lastlogin_clienttag");
  db_drop_column("BNET\\acct\\lastlogin_clientver");
  db_drop_column("BNET\\acct\\lastlogin_cdkey");
  return 0;

}
#endif

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

	start = next_char + 1;
    }
    
    return 0;
}


extern char const * account_get_closed_characterlist(t_account * account, char const * clienttag, char const * realmname)
{
    char realmkey[256];
    int  realmkeylen;
    
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
    realmkeylen=strlen(realmkey);
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

/* ADDED BY UNDYING SOULZZ 4/7/02 */
extern int account_set_w3_race( t_account * account, char const * race )
{
    if ( race == NULL )
    {
	eventlog( eventlog_level_error,"account_set_w3_race","got NULL race. Not setting." );
	return -1;
    }
    eventlog( eventlog_level_debug,"account_set_w3_race","setting race to %s", race );

    return account_set_strattr( account, "Record\\WAR3\\0\\race", race );
}

extern char const * account_get_w3_race( t_account * account )
{
    if ( account_get_strattr( account, "Record\\WAR3\\0\\race") == NULL ) /* doesn't exist, so add it */
    {
   	account_set_w3_race( account, "humans" );	/* humans are the default race */
	if ( account_get_strattr(account, "Record\\WAR3\\0\\race") == NULL )
	{
	    eventlog( eventlog_level_error, "account_get_w3_race", "couldn't get race" );
	    return NULL;
        }	
    }

    return account_get_strattr( account, "Record\\WAR3\\0\\race" );
}

extern int account_set_w3_accountlevel( t_account * account, unsigned int level )
{
    eventlog( eventlog_level_debug,"account_set_w3_race","setting account level to %d", level );
    return account_set_numattr( account, "Record\\WAR3\\0\\accountlevel", level );
}

extern unsigned int account_get_w3_accountlevel( t_account * account )
{
    if ( account_get_strattr( account, "Record\\WAR3\\0\\accountlevel") == NULL ) /* doesn't exist, so add it */
    {
   	account_set_w3_accountlevel( account, 0 );	/* 0 is the default account level */
	if ( account_get_strattr(account, "Record\\WAR3\\0\\accountlevel") == NULL )
	{
	    eventlog( eventlog_level_error, "account_get_w3_accountlevel", "couldn't get accountlevel" );
	    return 0xffff;
        }	
    }

    return account_get_numattr( account, "Record\\WAR3\\0\\accountlevel" );
}
   
/* ADDED BY UNDYING SOULZZ 4/8/02 - Secure password Function SET/GET */
extern int account_set_w3_acctpass( t_account * account, char const * acctsetpass )
{
	if ( acctsetpass == NULL )
	{
		eventlog( eventlog_level_error,"account_set_w3_acctpass","got NULL pass. Not setting." );
		return -1;
    }
	eventlog( eventlog_level_debug,"account_set_w3_acctpass","setting password to %s", acctsetpass );
	return account_set_strattr( account, "Record\\WAR3\\0\\securepass", acctsetpass );
}

extern char const * account_get_w3_acctpass( t_account * account)
{
	return account_get_strattr( account, "Record\\WAR3\\0\\securepass" );
}

/* ADDED BY THE UNDYING SOULZZ 4/10/02 - Clan Name for Profile Setting */
extern int account_set_w3_clanname( t_account * account, char const * acctsetclanname )
{
	if ( acctsetclanname == NULL )
	{
	eventlog( eventlog_level_error,"account_set_w3_acctclanname","got NULL Clan Name. Not setting." );
	return -1;
    }
	eventlog( eventlog_level_debug,"account_set_w3_acctpass","setting password to %s", acctsetclanname );
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


// THEUNDYING 5/15/02 - FRIENDS LISTS GET/ADD/DELETE Functions
// [zap-zero] 20020516
extern int account_set_friend( t_account * account, int friendnum, char const * friendname )
{
	char key[256];
	if ( friendname == NULL || friendnum < 0 || friendnum >= MAX_FRIENDS)
	{
		return -1;
	}
	sprintf(key, "friend\\%d\\name", friendnum);

	return account_set_strattr( account, key, friendname);
}

extern char const * account_get_friend( t_account * account, int friendnum)
{
	char key[256];
	char const * tmp;

	if (friendnum < 0 || friendnum >= MAX_FRIENDS) {
		// bogus name (user himself) instead of NULL, otherwise clients might crash
		return account_get_name(account);  

	}

	sprintf(key, "friend\\%d\\name", friendnum);
	tmp = account_get_strattr(account, key);
	if(!tmp) {
		int n = account_get_friendcount(account);
		eventlog(eventlog_level_warn,"account_get_friend","NULL friend, decrementing friendcount (%d) and sending bogus friend name '%s'", n, account_get_name(account));
		if(--n >= 0)
			account_set_friendcount(account, n);
		tmp = account_get_name(account);
	}
	return tmp;
}

extern int account_set_friendcount( t_account * account, int count)
{
	if (count < 0 || count > MAX_FRIENDS)
	{
		return -1;
	}

	return account_set_numattr( account, "friend\\count", count);
}

extern int account_get_friendcount( t_account * account )
{
    return account_get_numattr( account, "friend\\count" );
}

// THEUNDYING - MUTUAL FRIEND CHECK 
// fixed by zap-zero-tested and working 100% TheUndying
extern int account_check_mutual( t_account * account, char const *myusername)
{
	int i=0;
	int n = account_get_friendcount(account);
	char const *friend;

	if(!myusername) {
		eventlog(eventlog_level_error,"account_check_mutual","got NULL username");
		return -1;
	}

	for(i=0; i<n; i++) 
	{ //Cycle through your friend list and whisper the ones that are online
	  friend = account_get_friend(account,i);
	  if(!friend)  {
		eventlog(eventlog_level_error,"account_check_mutual","got NULL friend");
		return -1;
	  }
 
	  if(!strcasecmp(myusername,friend))
		  return 0;
	}
	// If friend isnt in list return -1 to tell func NO
	return -1;
}
// THEUNDYING 7/27/2002 - MUTUAL FRIENDS WATCHLIST CODE
//Used to let online mutual friends that you have logged on
extern int account_notify_friends_login ( char const *tname )
{
	if(handle_login_whisper(connlist_find_connection_by_accountname(tname),tname)==0)
	{
		return 0;	
	}
	else
	{
		return -1;
	}
}
//Used to let mutual friends know that you have logged off
extern int account_notify_friends_logoff ( char const *tname )
{
	t_account *useraccount;
	t_account *friendaccount;
	char const *username = tname;
	char const *friend;
	char msg[512];
	int i;
	int n;
		
	useraccount = accountlist_find_account(username);
	n = account_get_friendcount(useraccount);
	
	sprintf(msg,"Your friend %s has left the PvPGN Realm",username);

	for(i=0; i<n; i++) 
	{ 
		friend = account_get_friend(useraccount,i);
		friendaccount = accountlist_find_account(friend);

		if(account_check_mutual(friendaccount,tname)==0)
		{
			if(handle_logoff_whisper(connlist_find_connection_by_accountname(friend),username,msg)==0)
			{
				continue;
			}
		}
	}

	return 0;
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
	default:
	    eventlog(eventlog_level_warn,"race_get_str","unknown race: %x", race);
	    return NULL;
	}
}

extern int account_set_racewin( t_account * account, unsigned int intrace )
{
	char table[25];
	unsigned int wins;
	char const * race = race_get_str(intrace);

	if(!race)
	    return -1;

	sprintf(table,"Record\\%s\\wins",race);
	wins = account_get_numattr(account,table);
	wins++;
		
	return account_set_numattr(account,table,wins);
}

extern int account_get_racewin( t_account * account, unsigned int intrace)
{	
	char table[25];
	char const *race = race_get_str(intrace);

	if(!race)
	    return 0;
	
	sprintf(table,"Record\\%s\\wins",race);
	return account_get_numattr(account,table);
}

extern int account_set_raceloss( t_account * account, unsigned int intrace )
{
	char table[25];
	unsigned int losses;
	char const *race = race_get_str(intrace);

	if(!race)
	    return -1;

	sprintf(table,"Record\\%s\\losses",race);

	losses=account_get_numattr(account,table);
	
	losses++;
	
	return account_set_numattr(account,table,losses);
	
}

extern int account_get_raceloss( t_account * account, unsigned int intrace )
{	
	char table[25];
	char const *race = race_get_str(intrace);
	
	if(!race)
	    return 0;
	
	sprintf(table,"Record\\%s\\losses",race);
	
	return account_get_numattr(account,table);
	
}	

extern int account_set_solowin( t_account * account )
{
	unsigned int win;
	win = account_get_numattr(account,"Record\\solo\\wins");
	win++;
	return account_set_numattr(account,"Record\\solo\\wins",win);
}

extern int account_get_solowin( t_account * account )
{
	return account_get_numattr(account,"Record\\solo\\wins");
}

extern int account_set_sololoss( t_account * account)
{
	unsigned int loss;
	loss = account_get_numattr(account,"Record\\solo\\losses");
	loss++;
	return account_set_numattr(account,"Record\\solo\\losses",loss);
}

extern int account_get_sololoss( t_account * account)
{
	return account_get_numattr(account,"Record\\solo\\losses");
}

extern int account_set_soloxp(t_account * account,t_game_result gameresult, unsigned int opponlevel)
{ 
   int xp;
   int mylevel;
   int xpdiff = 0, placeholder;
   
   xp = account_get_soloxp(account); //get current xp
   if (xp < 0) {
      eventlog(eventlog_level_error, "account_set_soloxp", "got negative XP");
      return -1;
   }
   
   mylevel = account_get_sololevel(account); //get accounts level
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
   
   // aaron
   war3_ladder_update(&solo_ladder,account_get_uid(account),xpdiff,account,0);
   
   xp += xpdiff;
   if (xp < 0) xp = 0;
   
   return account_set_numattr(account,"Record\\solo\\xp",xp);
}

extern int account_get_soloxp(t_account * account)
{
	return account_get_numattr(account,"Record\\solo\\xp");
}

extern int account_set_sololevel(t_account * account)
{ 
   int xp, mylevel;
   
   xp = account_get_soloxp(account);
   if (xp < 0) xp = 0;
   
   mylevel = account_get_sololevel(account);
   if (mylevel < 1) mylevel = 1;
   
   if (mylevel > W3_XPCALC_MAXLEVEL) {
      eventlog(eventlog_level_error, "account_set_sololevel", "got invalid level: %d", mylevel);
      return -1;
   }
   
   mylevel = ladder_war3_updatelevel(mylevel, xp);

   return account_set_numattr(account, "Record\\solo\\level", mylevel);
}

extern int account_get_sololevel(t_account * account)
{
	return account_get_numattr(account,"Record\\solo\\level");
}

// 11-20-2002 aaron -->
extern int account_set_solorank(t_account * account)
{
  return account_set_numattr(account,"Record\\solo\\rank" ,war3_ladder_get_rank(&solo_ladder,account_get_uid(account),0));
}

extern int account_get_solorank_ladder(t_account * account)
{
   return war3_ladder_get_rank(&solo_ladder, account_get_uid(account),0);
}

extern int account_set_solorank_ladder(t_account * account, int rank)
{
  return account_set_numattr(account,"Record\\solo\\rank",rank);
}

extern int account_get_solorank(t_account * account)
{
       return account_get_numattr(account,"Record\\solo\\rank");
}

// Team Game - Play Game Funcs
extern int account_set_teamwin(t_account * account)
{
	unsigned int win;
	win = account_get_numattr(account,"Record\\team\\wins");
	win++;
	return account_set_numattr(account,"Record\\team\\wins",win);
}

extern int account_get_teamwin(t_account * account)
{
	return account_get_numattr(account,"Record\\team\\wins");
}

extern int account_set_teamloss(t_account * account)
{
	unsigned int loss;
	loss = account_get_numattr(account,"Record\\team\\losses");
	loss++;
	return account_set_numattr(account,"Record\\team\\losses",loss);
}

extern int account_get_teamloss(t_account * account)
{
	return account_get_numattr(account,"Record\\team\\losses");
}

extern int account_set_teamxp(t_account * account,t_game_result gameresult, unsigned int opponlevel)
{
	int xp;
	int mylevel;
	int lvldiff_2_xpdiff_loss[] = {-16, -22, -28, -37, -48, -60, -100, -140, -152, -163, -172, -178, -184};
	int lvldiff_2_xpdiff_win[] = { 184, 178, 172, 163, 152, 140, 100, 60, 48, 37, 28, 22, 16};
	int lvldiff, xpdiff;
	xp = account_get_teamxp(account); //get current xp
	xpdiff = 0;
	mylevel = account_get_teamlevel(account); //get accounts level

	if(mylevel==0) //if level is 0 then set it to 1
		mylevel=1;

	//I know its ugly, how bout some help putting this into a array? -THEUNDYING
	//made 2 nice arrays and added update of war3ladder(team) - aaron
	

	lvldiff = mylevel-opponlevel;
	if (lvldiff>6) lvldiff=6;
	if (lvldiff<-6) lvldiff=-6;
	
	
	if(gameresult == game_result_loss) //loss
	  {
	    xpdiff = lvldiff_2_xpdiff_loss[lvldiff+6];
	  }
	if(gameresult == game_result_win) //win
	  {
	    xpdiff = lvldiff_2_xpdiff_win[lvldiff+6];
	  }
	
	war3_ladder_update(&team_ladder,account_get_uid(account), xpdiff, account,0);
	
	xp += xpdiff;
	
	//now check see if xp is negative, if it is, just set it to 0
	if(xp<0) xp=0;
	
	
	return account_set_numattr(account,"Record\\team\\xp",xp);
}

extern int account_get_teamxp(t_account * account)
{
	return account_get_numattr(account,"Record\\team\\xp");
}

extern int account_set_teamlevel(t_account * account)
{ 
	int xp;
	unsigned int i;
	static const int xp_to_level[] = {100, 200, 300, 400, 500, 600, 700, 800,
		900, 1000, 1100, 1200, 1300, 1400, 1600, 1900, 2200, 2500, 2800, 3200,
	    3600, 4000, 4500, 5000, 5500, 6000, 6600, 7200, 7800, 8400, 9000}; //Supports up to level 31

	xp = account_get_teamxp(account);
	
	for (i = 0; i < sizeof(xp_to_level) / sizeof(int); i++) {
		if (xp_to_level[i] > xp) break;
	}

	return account_set_numattr(account, "Record\\team\\level", ++i);
}

extern int account_get_teamlevel(t_account * account)
{
	return account_get_numattr(account,"Record\\team\\level");
}

// 11-20-2002 aaron --->
extern int account_set_teamrank(t_account * account)
{
        return account_set_numattr(account,"Record\\team\\rank",war3_ladder_get_rank(&team_ladder,account_get_uid(account),0));
}

extern int account_get_teamrank_ladder(t_account * account)
{
	return war3_ladder_get_rank(&team_ladder,account_get_uid(account),0);
}

extern int account_set_teamrank_ladder(t_account * account, int rank)
{
        return account_set_numattr(account,"Record\\team\\rank",rank);
}

extern int account_get_teamrank(t_account * account)
{
       return account_get_numattr(account,"Record\\team\\rank");
}

// FFA code
extern int account_set_ffawin(t_account * account)
{
	unsigned int win;
	win = account_get_numattr(account,"Record\\ffa\\wins");
	win++;
	return account_set_numattr(account,"Record\\ffa\\wins",win);
}

extern int account_get_ffawin(t_account * account)
{
	return account_get_numattr(account,"Record\\ffa\\wins");
}

extern int account_set_ffaloss(t_account * account)
{
	unsigned int loss;
	loss = account_get_numattr(account,"Record\\ffa\\losses");
	loss++;
	return account_set_numattr(account,"Record\\ffa\\losses",loss);
}

extern int account_get_ffaloss(t_account * account)
{
	return account_get_numattr(account,"Record\\ffa\\losses");
}

extern int account_set_ffaxp(t_account * account,t_game_result gameresult, unsigned int opponlevel)
{ 
	int xp;
	int mylevel;
	int lvldiff_2_xpdiff_loss[] = {-16, -22, -28, -37, -48, -60, -100, -140, -152, -163, -172, -178, -184};
	int lvldiff_2_xpdiff_win[] = { 184, 178, 172, 163, 152, 140, 100, 60, 48, 37, 28, 22, 16};
	int lvldiff, xpdiff;
	xp = account_get_ffaxp(account); //get current xp
	xpdiff = 0;
	mylevel = account_get_ffalevel(account); //get accounts level

	if(mylevel==0) //if level is 0 then set it to 1
		mylevel=1;

	//I know its ugly, how bout some help putting this into a array? -THEUNDYING
	//aaron: done the array stuff
	
	lvldiff = mylevel-opponlevel;
	if (lvldiff>6) lvldiff=6;
	if (lvldiff<-6) lvldiff=-6;
	
	if(gameresult == game_result_loss) //loss
	{
	  xpdiff=lvldiff_2_xpdiff_loss[lvldiff+6];
	}
	if(gameresult == game_result_win) //win
	{
	  xpdiff=lvldiff_2_xpdiff_win[lvldiff+6];
	}

	war3_ladder_update(&ffa_ladder, account_get_uid(account), xpdiff, account, 0);

	xp += xpdiff;

	if (xp<0) xp=0;

	return account_set_numattr(account,"Record\\ffa\\xp",xp);
}

extern int account_get_ffaxp(t_account * account)
{
	return account_get_numattr(account,"Record\\ffa\\xp");
}

extern int account_set_ffalevel(t_account * account)
{ 
	int xp;
	unsigned int i;
	static const int xp_to_level[] = {100, 200, 300, 400, 500, 600, 700, 800,
		900, 1000, 1100, 1200, 1300, 1400, 1600, 1900, 2200, 2500, 2800, 3200,
	    3600, 4000, 4500, 5000, 5500, 6000, 6600, 7200, 7800, 8400, 9000}; //Supports up to level 31

	xp = account_get_ffaxp(account);
	
	for (i = 0; i < sizeof(xp_to_level) / sizeof(int); i++) {
		if (xp_to_level[i] > xp) break;
	}

	return account_set_numattr(account, "Record\\ffa\\level", ++i);
}

extern int account_get_ffalevel(t_account * account)
{
	return account_get_numattr(account,"Record\\ffa\\level");
}

extern int account_get_ffarank(t_account * account)
{
	return account_get_numattr(account,"Record\\ffa\\rank");
}

// aaron --->
extern int account_set_ffarank(t_account * account)
{
	return account_set_numattr(account,"Record\\ffa\\rank",war3_ladder_get_rank(&ffa_ladder,account_get_uid(account),0));
}

extern int account_get_ffarank_ladder(t_account * account)
{
	return war3_ladder_get_rank(&ffa_ladder,account_get_uid(account),0);
}

extern int account_set_ffarank_ladder(t_account * account, int rank)
{
	return account_set_numattr(account,"Record\\ffa\\rank",rank);
}
// <---


//Other funcs used in profiles and PG saving
extern void account_get_raceicon(t_account * account, char * raceicon, unsigned int * raceiconnumber, unsigned int * wins) //Based of wins for each race, Race with most wins, gets shown in chat channel
{
	unsigned int humans;
	unsigned int orcs;
	unsigned int undead;
	unsigned int nightelf;
	unsigned int random;
	int icons_limits[] = {25, 250, 500, 1500, -1};
	unsigned int i;

	random = account_get_racewin(account,W3_RACE_RANDOM);
	humans = account_get_racewin(account,W3_RACE_HUMANS); 
	orcs = account_get_racewin(account,W3_RACE_ORCS); 
	undead = account_get_racewin(account,W3_RACE_UNDEAD);
	nightelf = account_get_racewin(account,W3_RACE_NIGHTELVES);
	if(humans>=orcs && humans>=undead && humans>=nightelf && humans>=random) {
	    *raceicon = 'H';
	    *wins = humans;
	}
	else if(orcs>=humans && orcs>=undead && orcs>=nightelf && orcs>=random) {
	    *raceicon = 'O';
	    *wins = orcs;
	}
	else if(undead>=humans && undead>=orcs && undead>=nightelf && undead>=random) {
	    *raceicon = 'U';
	    *wins = undead;
	}
	else if(nightelf>=humans && nightelf>=orcs && nightelf>=undead && nightelf>=random) {
	    *raceicon = 'N';
	    *wins = nightelf;
	}
	else {
	    *raceicon = 'R';
	    *wins = random;
	}
	i = 0;
	while((signed)*wins >= icons_limits[i] && icons_limits[i] > 0) i++;
	*raceiconnumber = i + 1;
}

extern int account_get_profile_calcs(t_account * account, int xp, unsigned int j)
{

	static const int xp_min[] = {-1, 0, 100, 200, 400, 600, 900, 1200, 1600, 2000, 2500, 
	3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500, 9000};
	static const int xp_max[] = {-1, 100, 200, 400, 600, 900, 1200, 1600, 2000, 2500, 
	3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500};

	unsigned int i;
	int  t;

	for (i = 0; i < sizeof(xp_min) / sizeof(int); i++) {
		if (xp >= xp_min[i] && xp < xp_max[i]) {
			t = (int)((((double)xp - (double)xp_min[i]) 
					/ ((double)xp_max[i] - (double)xp_min[i])) * 128);
			if (i < j) {
				return 128 + t;
			} else {
				return t;
			}
		}
	}
	return 0;
}

extern int account_set_saveladderstats(t_account * account,unsigned int gametype, t_game_result result, unsigned int opponlevel)
{
	unsigned int intrace;

	if(!account) {
		eventlog(eventlog_level_error, "account_set_saveladderstats", "got NULL account");
		return -1;
	}

	intrace = account_get_w3pgrace(account);
	
	if(gametype==ANONGAME_TYPE_1V1) //1v1
	{

		if(result == game_result_win) //win
		{
			account_set_solowin(account);
			account_set_racewin(account,intrace);
			account_inc_ladder_wins(account,"WAR3",ladder_id_w3_solo);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_solo,"WIN");
			
		}
		if(result == game_result_loss) //loss
		{
			account_set_sololoss(account);
			account_set_raceloss(account,intrace);
			account_inc_ladder_losses(account,"WAR3",ladder_id_w3_solo);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_solo,"LOSS");
		}
			
		account_set_soloxp(account,result,opponlevel);
		account_set_sololevel(account);
	}
	else if(gametype==ANONGAME_TYPE_2V2)
	{
		if(result == game_result_win) //win
		{
			account_set_teamwin(account);
			account_set_racewin(account,intrace);
			account_inc_ladder_wins(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"WIN");
		}
		if(result == game_result_loss) //loss
		{
			account_set_teamloss(account);
			account_set_raceloss(account,intrace);
			account_inc_ladder_losses(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"LOSS");
		}

		account_set_teamxp(account,result,opponlevel); //Not done yet
		account_set_teamlevel(account);
	}
	else if(gametype==ANONGAME_TYPE_3V3)
	{
		if(result == game_result_win) //win
		{
			account_set_teamwin(account);
			account_set_racewin(account,intrace);
			account_inc_ladder_wins(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"WIN");
		}
		if(result == game_result_loss) //loss
		{
			account_set_teamloss(account);
			account_set_raceloss(account,intrace);
			account_inc_ladder_losses(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"LOSS");
		}

		account_set_teamxp(account,result,opponlevel); //Not done yet
		account_set_teamlevel(account);
	}
	else if(gametype==ANONGAME_TYPE_4V4)
	{
		if(result == game_result_win) //win
		{
			account_set_teamwin(account);
			account_set_racewin(account,intrace);
			account_inc_ladder_wins(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"WIN");
		}
		if(result == game_result_loss) //loss
		{
			account_set_teamloss(account);
			account_set_raceloss(account,intrace);
			account_inc_ladder_losses(account,"WAR3",ladder_id_w3_team);
			account_set_ladder_last_result(account,"WAR3",ladder_id_w3_team,"LOSS");
		}

		account_set_teamxp(account,result,opponlevel); //Not done yet
		account_set_teamlevel(account);
	}

	else if(gametype==ANONGAME_TYPE_SMALL_FFA)
	{
		if(result == game_result_win) //win
		{
			account_set_ffawin(account);
			account_set_racewin(account,intrace);
		}
		if(result == game_result_loss) //loss
		{
			account_set_ffaloss(account);
			account_set_raceloss(account,intrace);
		}

		account_set_ffaxp(account,result,opponlevel);
		account_set_ffalevel(account);
	}
	else
		eventlog(eventlog_level_error,"account_set_saveladderstats","Invalid Gametype? %u",gametype);

	return 0;
}

extern int account_set_w3pgrace( t_account * account, unsigned int race)
{
	return account_set_numattr( account, "Record\\w3pgrace", race);
}

extern int account_get_w3pgrace( t_account * account )
{
    return account_get_numattr( account, "Record\\w3pgrace" );
}
// Arranged Team Functions
extern int account_check_team(t_account * account, const char * members_usernames)
{
	int teams_cnt = account_get_atteamcount(account);
	int i;
	    
    if(teams_cnt==0) //user has never play at before
		return -1;
	
	for(i=1;i<=teams_cnt;i++)
	{
		char const * members = account_get_atteammembers(account,i);
		if(members && members_usernames && strcasecmp(account_get_atteammembers(account,i),members_usernames)==0)
		{
			eventlog(eventlog_level_debug,"account_check_team","Use have played before! Team num is %d",i);
			return i;
		}
	}
	//if your here this group of people have never played a AT game yet
	return -1;
}

extern int account_create_newteam(t_account * account, const char * members_usernames, unsigned int teamsize)
{
	int teams_cnt = account_get_atteamcount(account);
	
	teams_cnt++; //since we making a new team here add+1 to the count

    account_set_atteamcount(account,teams_cnt);
	account_set_atteammembers(account,teams_cnt,members_usernames);
	account_set_atteamsize(account,teams_cnt,teamsize);
	    	
	return teams_cnt;
}

extern int account_set_atteamcount(t_account * account, unsigned int teamcount)
{
	return account_set_numattr(account,"Record\\teamcount",teamcount);
}

extern int account_get_atteamcount(t_account * account)
{
	return account_get_numattr(account,"Record\\teamcount");
}

extern int account_set_atteamsize(t_account * account, unsigned int teamcount, unsigned int teamsize)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamsize",teamcount);
	return account_set_numattr(account,key,teamsize);
}
extern int account_get_atteamsize(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamsize",teamcount);
	return account_get_numattr(account,key);
}
extern int account_set_atteamwin(t_account * account, unsigned int teamcount)
{
	char key[120];
	int wins = account_get_atteamwin(account,teamcount);
	wins++;

	sprintf(key,"Team\\%u\\teamwins",teamcount);
	
	return account_set_numattr(account,key,wins);
}
extern int account_get_atteamwin(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamwins",teamcount);
	return account_get_numattr(account,key);
}
extern int account_set_atteamloss(t_account * account, unsigned int teamcount)
{
	char key[120];
	int loss = account_get_atteamloss(account,teamcount);
	loss++;

	sprintf(key,"Team\\%u\\teamlosses",teamcount);
	
	return account_set_numattr(account,key,loss);
}
extern int account_get_atteamloss(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamlosses",teamcount);
	return account_get_numattr(account,key);
}
extern int account_set_atteamxp(t_account * account, t_game_result gameresult, unsigned int opponlevel, unsigned int teamcount)
{ 
   int xp;
   int mylevel;
   int xpdiff = 0, placeholder;
   char key[120];
   
   xp = account_get_atteamxp(account,teamcount); //get current xp
   if (xp < 0) {
      eventlog(eventlog_level_error, "account_set_atteamxp", "got negative XP");
      return -1;
   }
   
   mylevel = account_get_atteamlevel(account,teamcount); //get accounts level
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
   
   // aaron:
   war3_ladder_update(&at_ladder,account_get_uid(account),xpdiff,account, teamcount);
   
   xp += xpdiff;
   if (xp < 0) xp = 0;
   
   sprintf(key,"Team\\%u\\teamxp",teamcount);
   return account_set_numattr(account,key,xp);
}
extern int account_get_atteamxp(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamxp",teamcount);
	return account_get_numattr(account,key);
}
extern int account_set_atteamlevel(t_account * account, unsigned int teamcount)
{
   int xp, mylevel;
   char key[255];
   
   xp = account_get_atteamxp(account,teamcount);
   
   if (xp < 0) xp = 0;
   
   mylevel = account_get_atteamlevel(account,teamcount);
   if (mylevel < 1) mylevel = 1;
   
   if (mylevel > W3_XPCALC_MAXLEVEL) 
   {
      eventlog(eventlog_level_error, "account_set_atteamlevel", "got invalid level: %d", mylevel);
      return -1;
   }
   
   mylevel = ladder_war3_updatelevel(mylevel, xp);
   sprintf(key,"Team\\%u\\teamlevel",teamcount);
   return account_set_numattr(account,key,mylevel);
}
extern int account_get_atteamlevel(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teamlevel",teamcount);
	return account_get_numattr(account,key);
}
//aaron TODO
extern int account_get_atteamrank(t_account * account, unsigned int teamcount)
{
  char key[120];
  sprintf(key,"Team\\%u\\rank",teamcount);
  return account_get_numattr(account,key);
}

extern int account_set_atteamrank(t_account * account, unsigned int teamcount)
{
  char key[120];
  sprintf(key,"Team\\%u\\rank",teamcount);
  return account_set_numattr(account,key,war3_ladder_get_rank(&at_ladder,account_get_uid(account),teamcount));
}

extern int account_get_atteamrank_ladder(t_account * account, unsigned int teamcount)
{
  return war3_ladder_get_rank(&at_ladder,account_get_uid(account), teamcount);
}

extern int account_set_atteamrank_ladder(t_account * account, int rank, unsigned int teamcount)
{
  char key[120];
  sprintf(key,"Team\\%u\\rank",teamcount);
  return account_set_numattr(account,key,rank);
}
// <---

extern int account_set_atteammembers(t_account * account, unsigned int teamcount, char const *members)
{
	char key[120];
	sprintf(key,"Team\\%u\\teammembers",teamcount);
	return account_set_strattr(account,key,members);
}
extern char const * account_get_atteammembers(t_account * account, unsigned int teamcount)
{
	char key[120];
	sprintf(key,"Team\\%u\\teammembers",teamcount);
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
extern int account_set_saveATladderstats(t_account * account, unsigned int gametype, t_game_result result, unsigned int opponlevel, unsigned int current_teamnum)
{
	unsigned int intrace;
	
	if(!account) 
	{
		eventlog(eventlog_level_error, "account_set_saveATladderstats", "got NULL account");
		return -1;
	}
	
	intrace = account_get_w3pgrace(account);
	
	if(result == game_result_win)
	{
		account_set_atteamwin(account,current_teamnum);
		account_set_racewin(account,intrace);
	}
	if(result == game_result_loss)
	{
		account_set_atteamloss(account,current_teamnum);
		account_set_raceloss(account,intrace);
	}
	account_set_atteamxp(account,result,opponlevel,current_teamnum);
	account_set_atteamlevel(account,current_teamnum);
	return 0;
}
extern int account_get_highestladderlevel(t_account * account)
{
	// [quetzal] 20020827 - AT level part rewritten
	int i;

	unsigned int sololevel = account_get_sololevel(account);
	unsigned int teamlevel = account_get_teamlevel(account);
	unsigned int atlevel = 0;
	unsigned int t; // [quetzal] 20020827 - not really needed, but could speed things up a bit
	
	for (i = 0; i < account_get_atteamcount(account); i++) {
		if ((t = account_get_atteamlevel(account, i+1)) > atlevel) {
			atlevel = t;
		}
	}

	eventlog(eventlog_level_debug,"account_get_highestladderlevel","Checking for highest level in Solo,Team,AT Ladder Stats");
	eventlog(eventlog_level_debug,"account_get_highestladderlevel","Solo Level: %d, Team Level %d, Highest AT Team Level: %d",sololevel,teamlevel,atlevel);
			
	if(sololevel >= teamlevel && sololevel >= atlevel)
		return sololevel;
	if(teamlevel >= sololevel && teamlevel >= atlevel)
        return teamlevel;
	if(atlevel >= sololevel && atlevel >= teamlevel)
        return atlevel;

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

extern int account_get_icon_profile(t_account * account)
{
	unsigned int humans	= account_get_racewin(account,W3_RACE_HUMANS);		//  1;
	unsigned int orcs	= account_get_racewin(account,W3_RACE_ORCS);		//  2;
	unsigned int nightelf	= account_get_racewin(account,W3_RACE_NIGHTELVES);	//  4;
	unsigned int undead	= account_get_racewin(account,W3_RACE_UNDEAD);		//  8;
	unsigned int random	= account_get_racewin(account,W3_RACE_RANDOM);		// 32;
	unsigned int race	= 0; // 0 = Humans, 1 = Orcs, 2 = Night Elves, 3 = Undead, 4 = Ramdom
	unsigned int level	= 0; // 0 = under 25, 1 = 25 to 249, 2 = 250 to 499, 3 = 500 to 1499, 4 = 1500 or more (wins)
	unsigned int wins	= 0;
	unsigned int icons_limits[5] = {25, 250, 500, 1500, -1}; // maybe add option to config to set levels
	unsigned int profileicon[5][5] = {
	    {0x68706561, 0x68666F6F, 0x686B6E69, 0x48616D67, 0x6E6D6564}, // Humans - Peasant, Footman, Knight, Archmage, Medivh
	    {0x6F70656F, 0x6F677275, 0x6F746175, 0x4F666172, 0x4F746872}, // Orcs - Peon, Grunt, Tauren, Far Seer, Thrall
	    {0x65777370, 0x65617263, 0x65646F63, 0x456D6F6F, 0x45667572}, // Night Elves - Wisp, Archer, Druid of the Claw, Priestess of the Moon, Furion Stormrage
	    {0x7561636F, 0x7567686F, 0x7561626F, 0x556C6963, 0x55746963}, // Undead - Acolyle, Ghoul, Abomination, Lich, Tichondrius
	    {0x00000000, 0x6E677264, 0x6E616472, 0x6E726472, 0x6E62776D}  // Ramdom - ????, Grean Dragon Whelp, Blue Dragon, Red Dragon, Deathwing
	};

        if(humans>=orcs && humans>=undead && humans>=nightelf && humans>=random) {
	    wins = humans;
            race = 0;
        }
        else if(orcs>=humans && orcs>=undead && orcs>=nightelf && orcs>=random) {
            wins = orcs;
            race = 1;
        }
        else if(nightelf>=humans && nightelf>=orcs && nightelf>=undead && nightelf>=random) {
            wins = nightelf;
            race = 2;
        }
        else if(undead>=humans && undead>=orcs && undead>=nightelf && undead>=random) {
            wins = undead;
            race = 3;
        }
        else {
            wins = random;
            race = 4;
        }

        while(wins >= icons_limits[level] && icons_limits[level] > 0) level++;

        eventlog(eventlog_level_info,"account_get_icon_profile","race -> %u; level -> %u; wins -> %u; profileicon -> 0x%X", race, level, wins, profileicon[race][level]);

	if (!wins) return 0x6E736865; // Sheep
	
        if (!level) return 0x6E736865; // Sheep - Remove to use race icons ie. Peasant, Peon, Wisp, etc. (Not tested) maybe add option in config
        
	return profileicon[race][level];
}
