/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999  Gediminas (gediminas_lt@mailexcite.com)
 * Copyright (C) 1999  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2000  Dizzy 
 * Copyright (C) 2000  Onlyer (onlyer@263.net)
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
#include "compat/strtoul.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include <ctype.h>
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
#ifdef HAVE_SYS_TYPES
# include <sys/types.h>
#endif
#include "compat/strftime.h"
#include "message.h"
#include "common/tag.h"
#include "connection.h"
#include "channel.h"
#include "game.h"
#include "common/util.h"
#include "common/version.h"
#include "account.h"
#include "server.h"
#include "prefs.h"
#include "common/eventlog.h"
#include "ladder.h"
#include "timer.h"
#include "common/bnettime.h"
#include "common/addr.h"
#include "common/packet.h"
#include "gametrans.h"
#include "helpfile.h"
#include "mail.h"
#include "common/bnethash.h"
#include "runprog.h"
#include "common/list.h"
#include "common/proginfo.h"
#include "alias_command.h"
#include "realm.h"
#include "ipban.h"
#ifdef WITH_BITS
# include "bits.h"
# include "bits_query.h"
# include "bits_va.h"
# include "bits_packet.h"
# include "bits_login.h"
# include "bits_chat.h"
# include "bits_game.h"
#endif
#include "common/queue.h"
#include "common/bn_type.h"
#include "command.h"
#include "common/setup_after.h"
// aaron
#include "war3ladder.h"


static char const * bnclass_get_str(unsigned int class);
static void do_whisper(t_connection * user_c, char const * dest, char const * text);
static void do_whois(t_connection * c, char const * dest);
static void user_timer_cb(t_connection * c, time_t now, t_timer_data str);

static char const * bnclass_get_str(unsigned int class)
{
    switch (class)
    {
    case PLAYERINFO_DRTL_CLASS_WARRIOR:
	return "warrior";
    case PLAYERINFO_DRTL_CLASS_ROGUE:
	return "rogue";
    case PLAYERINFO_DRTL_CLASS_SORCERER:
	return "sorcerer";
    default:
	return "unknown";
    }
}


static void do_whisper(t_connection * user_c, char const * dest, char const * text)
{
    t_connection * dest_c;
    char           temp[MAX_MESSAGE_LEN];
    char const *   tname;
    
    if (!(dest_c = connlist_find_connection_by_name(dest,conn_get_realmname(user_c))))
    {
#ifndef WITH_BITS
	message_send_text(user_c,message_type_error,user_c,"That user is not logged on.");
	return;
#else
	bits_chat_user((tname = conn_get_username(user_c)),dest,message_type_whisper,conn_get_latency(user_c),conn_get_flags(user_c),text);
	conn_unget_username(user_c,tname);
	return;
#endif
    }
    
    if (conn_get_dndstr(dest_c))
    {
        sprintf(temp,"%.64s is unavailable (%.128s)",(tname = conn_get_username(dest_c)),conn_get_dndstr(dest_c));
	conn_unget_username(dest_c,tname);
        message_send_text(user_c,message_type_info,user_c,temp);
        return;
    }
    
    message_send_text(user_c,message_type_whisperack,dest_c,text);
    
    if (conn_get_awaystr(dest_c))
    {
        sprintf(temp,"%.64s is away (%.128s)",(tname = conn_get_username(dest_c)),conn_get_awaystr(dest_c));
	conn_unget_username(dest_c,tname);
        message_send_text(user_c,message_type_info,user_c,temp);
    }
    
    message_send_text(dest_c,message_type_whisper,user_c,text);
    
    if ((tname = conn_get_username(user_c)))
    {
        char username[1+USER_NAME_MAX]; /* '*' + username (including NUL) */
	
	if (strlen(tname)<USER_NAME_MAX)
	{
            sprintf(username,"*%s",tname);
	    conn_set_lastsender(dest_c,username);
	}
	conn_unget_username(dest_c,tname);
    }
}


static void do_whois(t_connection * c, char const * dest)
{
    t_connection *    dest_c;
    char              namepart[136]; /* 64 + " (" + 64 + ")" + NUL */
    char              temp[MAX_MESSAGE_LEN];
    char const *      verb;
    t_game const *    game;
    t_channel const * channel;
    
    if (!(dest_c = connlist_find_connection_by_name(dest,conn_get_realmname(c))))
    {
	message_send_text(c,message_type_error,c,"That user is not logged on.");
	return;
    }
    
    if (c==dest_c)
    {
	strcpy(namepart,"You");
	verb = "are";
    }
    else
    {
	char const * tname;
	
	sprintf(namepart,"%.64s",(tname = conn_get_username(dest_c)));
	conn_unget_username(dest_c,tname);
	verb = "is";
    }
    
    if ((game = conn_get_game(dest_c)))
    {
	if (strcmp(game_get_pass(game),"")==0)
	    sprintf(temp,"%s %s logged on from account "UID_FORMAT", and %s currently in game \"%.64s\".",
		    namepart,
		    verb,
		    conn_get_userid(dest_c),
		    verb,
		    game_get_name(game));
	else
	    sprintf(temp,"%s %s logged on from account "UID_FORMAT", and %s currently in private game \"%.64s\".",
		    namepart,
		    verb,
		    conn_get_userid(dest_c),
		    verb,
		    game_get_name(game));
    }
    else if ((channel = conn_get_channel(dest_c)))
    {
	if (channel_get_permanent(channel)==1)
            sprintf(temp,"%s %s logged on from account "UID_FORMAT", and %s currently in channel \"%.64s\".",
		    namepart,
		    verb,
		    conn_get_userid(dest_c),
		    verb,
		    channel_get_name(channel));
	else
            sprintf(temp,"%s %s logged on from account "UID_FORMAT", and %s currently in private channel \"%.64s\".",
		    namepart,
		    verb,
		    conn_get_userid(dest_c),
		    verb,
		    channel_get_name(channel));
    }
    else
	sprintf(temp,"%s %s logged on from account "UID_FORMAT".",
		namepart,
		verb,
		conn_get_userid(dest_c));
    message_send_text(c,message_type_info,c,temp);
    if (game && strcmp(game_get_pass(game),"")!=0)
	message_send_text(c,message_type_info,c,"(This game is password protected.)");
    
    if (conn_get_dndstr(dest_c))
    {
        sprintf(temp,"%s %s refusing messages (%.128s)",
		namepart,
		verb,
		conn_get_dndstr(dest_c));
	message_send_text(c,message_type_info,c,temp);
    }
    else
        if (conn_get_awaystr(dest_c))
        {
            sprintf(temp,"%s away (%.128s)",
		    namepart,
		    conn_get_awaystr(dest_c));
	    message_send_text(c,message_type_info,c,temp);
        }
}


static void user_timer_cb(t_connection * c, time_t now, t_timer_data str)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"user_timer_cb","got NULL connection");
	return;
    }
    if (!str.p)
    {
	eventlog(eventlog_level_error,"user_timer_cb","got NULL str");
	return;
    }
    
    if (now!=(time_t)0) /* zero means user logged out before expiration */
	message_send_text(c,message_type_info,c,str.p);
    free(str.p);
}

typedef int (* t_command)(t_connection * c, char const * text);

typedef struct {
	const char * command_string;
	t_command    command_handler;
} t_command_table_row;


// command handler prototypes
static int _handle_clan_command(t_connection * c, char const * text);
static int _handle_admin_command(t_connection * c, char const * text);
static int _handle_friends_command(t_connection * c, char const * text);
static int _handle_me_command(t_connection * c, char const * text);
static int _handle_whisper_command(t_connection * c, char const * text);
static int _handle_status_command(t_connection * c, char const * text);
static int _handle_who_command(t_connection * c, char const * text);
static int _handle_whois_command(t_connection * c, char const * text);
static int _handle_whoami_command(t_connection * c, char const * text);
static int _handle_announce_command(t_connection * c, char const * text);
static int _handle_beep_command(t_connection * c, char const * text);
static int _handle_nobeep_command(t_connection * c, char const * text);
static int _handle_version_command(t_connection * c, char const * text);
static int _handle_copyright_command(t_connection * c, char const * text);
static int _handle_uptime_command(t_connection * c, char const * text);
static int _handle_stats_command(t_connection * c, char const * text);
static int _handle_time_command(t_connection * c, char const * text);
static int _handle_channel_command(t_connection * c, char const * text);
static int _handle_rejoin_command(t_connection * c, char const * text);
static int _handle_away_command(t_connection * c, char const * text);
static int _handle_dnd_command(t_connection * c, char const * text);
static int _handle_squelch_command(t_connection * c, char const * text);
static int _handle_unsquelch_command(t_connection * c, char const * text);
static int _handle_designate_command(t_connection * c, char const * text);
static int _handle_resign_command(t_connection * c, char const * text);
static int _handle_kick_command(t_connection * c, char const * text);
static int _handle_ban_command(t_connection * c, char const * text);
static int _handle_unban_command(t_connection * c, char const * text);
static int _handle_reply_command(t_connection * c, char const * text);
static int _handle_realmann_command(t_connection * c, char const * text);
static int _handle_watch_command(t_connection * c, char const * text);
static int _handle_unwatch_command(t_connection * c, char const * text);
static int _handle_watchall_command(t_connection * c, char const * text);
static int _handle_unwatchall_command(t_connection * c, char const * text);
static int _handle_lusers_command(t_connection * c, char const * text);
static int _handle_news_command(t_connection * c, char const * text);
static int _handle_games_command(t_connection * c, char const * text);
static int _handle_channels_command(t_connection * c, char const * text);
static int _handle_addacct_command(t_connection * c, char const * text);
static int _handle_chpass_command(t_connection * c, char const * text);
static int _handle_connections_command(t_connection * c, char const * text);
static int _handle_finger_command(t_connection * c, char const * text);
static int _handle_operator_command(t_connection * c, char const * text);
static int _handle_admins_command(t_connection * c, char const * text);
static int _handle_quit_command(t_connection * c, char const * text);
static int _handle_kill_command(t_connection * c, char const * text);
static int _handle_killsession_command(t_connection * c, char const * text);
static int _handle_gameinfo_command(t_connection * c, char const * text);
static int _handle_ladderactivate_command(t_connection * c, char const * text);
static int _handle_remove_accounting_infos_command(t_connection * c, char const * text);
static int _handle_rehash_command(t_connection * c, char const * text);
static int _handle_rank_all_accounts_command(t_connection * c, char const * text);
static int _handle_reload_accounts_all_command(t_connection * c, char const * text);
static int _handle_reload_accounts_new_command(t_connection * c, char const * text);
static int _handle_shutdown_command(t_connection * c, char const * text);
static int _handle_ladderinfo_command(t_connection * c, char const * text);
static int _handle_timer_command(t_connection * c, char const * text);
static int _handle_serverban_command(t_connection * c, char const * text);
static int _handle_netinfo_command(t_connection * c, char const * text);
static int _handle_quota_command(t_connection * c, char const * text);
static int _handle_lockacct_command(t_connection * c, char const * text);
static int _handle_unlockacct_command(t_connection * c, char const * text);
static int _handle_flag_command(t_connection * c, char const * text);
static int _handle_tag_command(t_connection * c, char const * text);
static int _handle_bitsinfo_command(t_connection * c, char const * text);
static int _handle_ipban_command(t_connection * c, char const * text);
static int _handle_set_command(t_connection * c, char const * text);
static int _handle_motd_command(t_connection * c, char const * text);
static int _handle_ping_command(t_connection * c, char const * text);

static t_command_table_row standard_command_table[] = 
{
	{ "/clan"		, _handle_clan_command },
	{ "/admin"		, _handle_admin_command },
	{ "/f"                  , _handle_friends_command },
	{ "/friends"            , _handle_friends_command },
	{ "/me"                 , _handle_me_command },
	{ "/msg"                , _handle_whisper_command },
	{ "/whisper"            , _handle_whisper_command },
	{ "/w"                  , _handle_whisper_command },
	{ "/m"                  , _handle_whisper_command },
	{ "/status"             , _handle_status_command },
	{ "/users"              , _handle_status_command },
	{ "/who"                , _handle_who_command },
	{ "/whois"              , _handle_whois_command },
	{ "/whereis"            , _handle_whois_command },
	{ "/where"              , _handle_whois_command },
	{ "/whoami"             , _handle_whoami_command },
	{ "/announce"           , _handle_announce_command },
	{ "/beep"               , _handle_beep_command },
	{ "/nobeep"             , _handle_nobeep_command },
	{ "/version"            , _handle_version_command },
	{ "/copyright"          , _handle_copyright_command },
	{ "/warrenty"           , _handle_copyright_command },
	{ "/license"            , _handle_copyright_command },
	{ "/uptime"             , _handle_uptime_command },
	{ "/stats"              , _handle_stats_command },
	{ "/astat"              , _handle_stats_command },
	{ "/time"               , _handle_time_command },
        { "/channel"            , _handle_channel_command },
	{ "/join"               , _handle_channel_command },
	{ "/rejoin"             , _handle_rejoin_command },
	{ "/away"               , _handle_away_command },
	{ "/dnd"                , _handle_dnd_command },
	{ "/ignore"             , _handle_squelch_command },
	{ "/squelch"            , _handle_squelch_command },
	{ "/unignore"           , _handle_unsquelch_command },
	{ "/unsquelch"          , _handle_unsquelch_command },
	{ "/designate"          , _handle_designate_command },
	{ "/resign"             , _handle_resign_command },
	{ "/kick"               , _handle_kick_command },
	{ "/ban"                , _handle_ban_command },
	{ "/unban"              , _handle_unban_command },


	{ NULL                  , NULL }
	
};

static t_command_table_row extended_command_table[] =
{
	{ "/ann"                , _handle_announce_command },
	{ "/r"                  , _handle_reply_command },
	{ "/reply"              , _handle_reply_command },
	{ "/realmann"           , _handle_realmann_command },
	{ "/watch"              , _handle_watch_command },
	{ "/unwatch"            , _handle_unwatch_command },
	{ "/watchall"           , _handle_watchall_command },
	{ "/unwatchall"         , _handle_unwatchall_command },
	{ "/lusers"             , _handle_lusers_command },
	{ "/news"               , _handle_news_command },
	{ "/games"              , _handle_games_command },
	{ "/channels"           , _handle_channels_command },
	{ "/chs"                , _handle_channels_command },
	{ "/addacct"            , _handle_addacct_command },
	{ "/chpass"             , _handle_chpass_command },
	{ "/connections"        , _handle_connections_command },
	{ "/conn"               , _handle_connections_command },
	{ "/finger"             , _handle_finger_command },
	{ "/operator"           , _handle_operator_command },
	{ "/admins"             , _handle_admins_command },
	{ "/logout"             , _handle_quit_command },
	{ "/quit"               , _handle_quit_command },
	{ "/exit"               , _handle_quit_command },
	{ "/kill"               , _handle_kill_command },
	{ "/killsession"        , _handle_killsession_command },
	{ "/gameinfo"           , _handle_gameinfo_command },
	{ "/ladderactivate"     , _handle_ladderactivate_command },
	{ "/remove_accounting_infos", _handle_remove_accounting_infos_command },
	{ "/rehash"             , _handle_rehash_command },
	{ "/rank_all_accounts"  , _handle_rank_all_accounts_command },
	{ "/reload_accounts_all", _handle_reload_accounts_all_command },
	{ "/reload_accounts_new", _handle_reload_accounts_new_command },
	{ "/shutdown"           , _handle_shutdown_command },
	{ "/ladderinfo"         , _handle_ladderinfo_command },
	{ "/timer"              , _handle_timer_command },
	{ "/serverban"          , _handle_serverban_command },
	{ "/netinfo"            , _handle_netinfo_command },
	{ "/quota"              , _handle_quota_command },
	{ "/lockacct"           , _handle_lockacct_command },
	{ "/unlockacct"         , _handle_unlockacct_command },
	{ "/flag"               , _handle_flag_command },
	{ "/tag"                , _handle_tag_command },
	{ "/bitsinfo"           , _handle_bitsinfo_command },
	{ "/help"               , handle_help_command },
	{ "/mail"               , handle_mail_command },
	{ "/ipban"              , _handle_ipban_command },
	{ "/set"                , _handle_set_command },
	{ "/motd"               , _handle_motd_command },
	{ "/latency"            , _handle_ping_command },
	{ "/ping"               , _handle_ping_command },
	{ "/p"                  , _handle_ping_command },
        { NULL                  , NULL } 

};

extern int handle_command(t_connection * c,  char const * text)
{ int res;

  t_command_table_row *p;

  for (p = standard_command_table; p->command_string != NULL; p++)
    {
      if (strcmp(p->command_string, text)==0)
	if (p->command_handler != NULL) return ((p->command_handler)(c,text));
    }

       
    if (prefs_get_extra_commands()==0)
    {
	message_send_text(c,message_type_error,c,"Unknown command.");
	eventlog(eventlog_level_debug,"handle_command","got unknown standard command \"%s\"",text);
	return 0;
    }
    
    for (p = extended_command_table; p->command_string != NULL; p++)
      {
      if (strcmp(p->command_string, text)==0)
	if (p->command_handler != NULL) return ((p->command_handler)(c,text));
    }
     
    if (strlen(text)>=2 && strncmp(text,"//",2)==0)
    {
	handle_alias_command(c,text);
	return 0;
    }

    message_send_text(c,message_type_error,c,"Unknown command.");
    eventlog(eventlog_level_debug,"handle_command","got unknown command \"%s\"",text);
    return 0;
}

// +++++++++++++++++++++++++++++++++ command implementations +++++++++++++++++++++++++++++++++++++++

static int _handle_clan_command(t_connection * c, char const * text)
{
  t_channel const * channel;
  unsigned int		  i;
  char const *	    clanname;
  char const * acctgetclanname;
  char const *		oldclanname;
  char         		clansendmessage[200];
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"You are not in a channel. Join a channel to set a account password!");
      return 0;
    }
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if ( (strcmp(&text[i],"help") == 0) || (text[i] == '\0') )
    {
      message_send_text(c,message_type_info,c,"usage: /clan <clanname>");
      message_send_text(c,message_type_info,c,"Using this option will allow you to join a clan which displays in your profile.  ");
      return 0;
    }
  acctgetclanname = account_get_w3_clanname(conn_get_account(c));
  message_send_text(c,message_type_error,c,"You have left your old clan ");
  if(!acctgetclanname || !(*acctgetclanname))
    {
      eventlog( eventlog_level_error, "handle_bnet", "No Clan Name Found. User setting a new clan name" );
      clanname = strdup( &text[i] );
      account_set_w3_clanname(conn_get_account(c),clanname);
      free( (void *) clanname );
      oldclanname = strdup( account_get_w3_clanname( conn_get_account( c ) ) );
      sprintf( clansendmessage, "Joined Clan %s", clanname );
      message_send_text(c,message_type_info,c,clansendmessage);	
      return 0;
    }
  else
    {
      eventlog( eventlog_level_error, "handle_bnet", "User setting a new clan name" );
      clanname = strdup( &text[i] );
      account_set_w3_clanname(conn_get_account(c),clanname);
      free( (void *) clanname );
      oldclanname = strdup( account_get_w3_clanname( conn_get_account( c ) ) );
      sprintf( clansendmessage, "Left Clan %s\r\n and Joined Clan %s\r\n", oldclanname, clanname );
      message_send_text(c,message_type_info,c,clansendmessage);	
      return 0;
    }
}

static int _handle_admin_command(t_connection * c, char const * text)
{
  unsigned int i;
  char msg[255];
  const char *username;
  char command;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if ((!text[i]) || ((text[i] != '+') && (text[i] != '-'))) {
    message_send_text(c, message_type_info, c,
		      "Use /admin +username and /admin -username to set/unset admin status.");
    return -1;
		}
  
  command = text[i++];
  username = &text[i];
  
  if(!*username)
    {
      message_send_text(c, message_type_info, c,
			"You need to supply a username.");
      return -1;
    }
  
  if (!accountlist_find_account(username)) {
    sprintf(msg, "There's no account with %s username.", username);
    message_send_text(c, message_type_info, c, msg);
    return -1;
  }
  
  if(account_get_auth_admin(conn_get_account(c)) > 0)
    {
      if (command == '+') {
	account_set_admin(accountlist_find_account(username));
	sprintf(msg, "%s has been promoted to a Server Admin.", username);			
      } else {
	account_set_demoteadmin(accountlist_find_account(username));
	sprintf(msg, "%s has been demoted from a Server Admin.", username);			
      }
      message_send_text(c, message_type_info, c, msg);
    } else {
      message_send_text(c, message_type_info, c,
			"You must have admin status to execute this command.");
      return -1;
    }
  return 0;
}

static int _handle_friends_command(t_connection * c, char const * text)
{
  int i;
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  text = &text[i];
  
  if(!text[0] || strstart(text,"help")==0 || strstart(text, "h")==0) {
    message_send_text(c,message_type_info,c,"Friends List (Used in Arranged Teams and finding online friends.)");
    message_send_text(c,message_type_info,c,"Type: /f add <username (adds a friend to your list)");
    message_send_text(c,message_type_info,c,"Type: /f del <username> (removes a friend from your list)");
    message_send_text(c,message_type_info,c,"Type: /f list (shows your full friends list)");
    message_send_text(c,message_type_info,c,"Type: /f msg (whispers a message to all your friends at once)");
    return 0;
  }
  if (strstart(text,"add")==0 || strstart(text,"a")==0) {
    char const * newfriend;
    char msgtemp[MAX_MESSAGE_LEN];
    int n;
    t_packet * rpacket=NULL;
    t_connection * dest_c;
    t_connection * friend_c;
    char tmp[7];
    
    for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
    for (; text[i]==' '; i++);
    
    if (!accountlist_find_account(&text[i]))
      {
	message_send_text(c,message_type_info,c,"That user does not exist.");
	return 0;
      }
    
    // [quetzal] 20020822 - we DO care if we add UserName or username to friends list
    newfriend = account_get_strattr(accountlist_find_account(&text[i]), 
				    "BNET\\acct\\username");
    
    if(!strcasecmp(newfriend, conn_get_username(c))) {
      message_send_text(c,message_type_info,c,"You can't add yourself to your friends list.");
      return 0;
    }
    
    n = account_get_friendcount(conn_get_account(c));
    
    if(n >= MAX_FRIENDS) {
      sprintf(msgtemp, "You can only have a maximum of %d friends.", MAX_FRIENDS);
      message_send_text(c,message_type_info,c,msgtemp);
      return 0;
    }
    
    
    // check if the friend was already added
    
    for(i=0; i<n; i++)
      if(!strcasecmp(account_get_friend(conn_get_account(c), i), newfriend)) 
	{
	  sprintf(msgtemp, "%s is already on your friends list!", newfriend);
	  message_send_text(c,message_type_info,c,msgtemp);
	  return 0;
	}
    
    account_set_friendcount(conn_get_account(c), n+1);
    account_set_friend(conn_get_account(c), n, newfriend);
    
    sprintf( msgtemp, "Added %s to your friends list.", newfriend);
    message_send_text(c,message_type_info,c,msgtemp);
    // 7/27/02 - THEUNDYING - Inform friend that you added them to your list
    friend_c = connlist_find_connection_by_accountname(newfriend);
    sprintf(msgtemp,"%s added you to his/her friends list.",conn_get_username(c));
    message_send_text(friend_c,message_type_info,friend_c,msgtemp);
    
    if (!(rpacket = packet_create(packet_class_bnet)))
      return 0;
    
    packet_set_size(rpacket,sizeof(t_server_friendadd_ack));
    packet_set_type(rpacket,SERVER_FRIENDADD_ACK);
    
    packet_append_string(rpacket, newfriend);
    memset(tmp, 0, 7);
    if ((dest_c = connlist_find_connection_by_accountname(newfriend))) {
      if (conn_get_channel(dest_c)) 
	tmp[1] = FRIENDSTATUS_CHAT;
      else
	tmp[1] = FRIENDSTATUS_ONLINE;
    }
    
    packet_append_data(rpacket, tmp, 7);
    
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    
    return 0;
		}
  // THEUNDYING 5/17/02 - WHISPERS ALL ONLINE FRIENDS AT ONCE
  // [zap-zero] 20020518 - some changes
  if (strstart(text,"msg")==0 || strstart(text,"w")==0 || strstart(text,"whisper")==0 || strstart(text,"m")==0) 
    {
      char const *friend;
      char const *msg;
      int i;
      int cnt = 0;
      int n = account_get_friendcount(conn_get_account(c));
      char const *myusername;
      t_connection * dest_c;
      
      for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
      for (; text[i]==' '; i++);
      
      msg = &text[i];
      //if the message test is empty then ignore command
      if (!(*msg))
	{
	  message_send_text(c,message_type_info,c,"Did not message any friends. Type some text next time.");
	  return 0; 
	}
      
      
      for(i=0; i<n; i++) 
	{ //Cycle through your friend list and whisper the ones that are online
	  friend = account_get_friend(conn_get_account(c),i);
	  dest_c = connlist_find_connection_by_accountname(friend);
	  myusername = conn_get_username(c);
	  if (dest_c==NULL) //If friend is offline, go on to next
	    continue;
	  else { 
	    cnt++;	// keep track of successful whispers
	    if(account_check_mutual(conn_get_account(dest_c),myusername)==0)
	      {
		message_send_text(dest_c,message_type_whisper,c,msg);
	      }
	  }
	}
      if(cnt)
	message_send_text(c,message_type_friendwhisperack,c,msg);
      else
	message_send_text(c,message_type_info,c,"All your friends are offline.");
      
      return 0;
    }
  if (strstart(text,"r")==0 || strstart(text,"remove")==0
      || strstart(text,"d")==0 || strstart(text,"del")==0) {
    int n;
    char msgtemp[MAX_MESSAGE_LEN];
    char const * oldfriend;
    t_packet * rpacket=NULL;
    
    for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
    for (; text[i]==' '; i++);
    
    // [quetzal] 20020822 - we DO care if we del UserName or username from friends list
    // [quetzal] 20020907 - dont do anything if oldfriend is NULL
    oldfriend = account_get_strattr(accountlist_find_account(&text[i]),"BNET\\acct\\username");
    if (oldfriend)
      {
	
	n = account_get_friendcount(conn_get_account(c));
	
	/*if (!accountlist_find_account(&text[i])) // But sometimes accounts can disappear in a DB corruption. If that happens the name will stay on the list all the time. heh
	  {
	  message_send_text(c,message_type_info,c,"That user does not exist.");
	  return 0;
	  }*/
	
	for(i=0; i<n; i++)
	  if(!strcasecmp(account_get_friend(conn_get_account(c), i), oldfriend)) {
	    char num = (char)i;
	    /* shift friends after deleted */
	    for(; i<n-1; i++)
	      account_set_friend(conn_get_account(c), i, account_get_friend(conn_get_account(c), i+1));
	    
	    account_set_friend(conn_get_account(c), n-1, "");
	    account_set_friendcount(conn_get_account(c), n-1);
	    
	    sprintf(msgtemp, "Removed %s from your friends list.", oldfriend);
	    message_send_text(c,message_type_info,c,msgtemp);
	    
	    
	    if (!(rpacket = packet_create(packet_class_bnet)))
	      return 0;
	    
	    packet_set_size(rpacket,sizeof(t_server_frienddel_ack));
	    packet_set_type(rpacket,SERVER_FRIENDDEL_ACK);
	    
	    bn_byte_set(&rpacket->u.server_frienddel_ack.friendnum, num);
	    
	    queue_push_packet(conn_get_out_queue(c),rpacket);
	    packet_del_ref(rpacket);
	    
	    return 0;
	  };
      }
    
    sprintf(msgtemp, "%s was not found on your friends list.", oldfriend);
    message_send_text(c,message_type_info,c,msgtemp);
    return 0;
  }
  if (strstart(text,"list")==0 || strstart(text,"l")==0) {
    char const * friend;
    char status[128];
    char software[1000];
    char msgtemp[MAX_MESSAGE_LEN];
    int n;
    char const *myusername;
    char const *clienttag;
    t_connection const * dest_c;
    t_game const * game;
    t_channel const * channel;
    
    software[0]='\0';
    message_send_text(c,message_type_info,c,"Your PvPGN - Friends List");
    message_send_text(c,message_type_info,c,"=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
    
    n = account_get_friendcount(conn_get_account(c));
    myusername = conn_get_username(c);
    
    for(i=0; i<n; i++) 
      {
	friend = account_get_friend(conn_get_account(c), i);
	
	if (!(dest_c = connlist_find_connection_by_accountname(friend)))
	  sprintf(status, ", offline");
	else
	  {
	    clienttag = conn_get_clienttag(dest_c);
	    if(strcasecmp(clienttag,CLIENTTAG_WARCRAFT3)==0)
	      sprintf(software," using Warcraft 3");
	    else if(strcasecmp(clienttag,CLIENTTAG_WARCIIBNE)==0)
	      sprintf(software," using Warcraft 2");
	    else if(strcasecmp(clienttag,CLIENTTAG_STARCRAFT)==0)
	      sprintf(software," using Starcraft");
	    else if(strcasecmp(clienttag,CLIENTTAG_BROODWARS)==0)
	      sprintf(software," using BroodWars");
	    else if(strcasecmp(clienttag,CLIENTTAG_DIABLORTL)==0)
	      sprintf(software," using Diablo 1");
	    else if(strcasecmp(clienttag,CLIENTTAG_DIABLO2XP)==0)
	      sprintf(software," using Diablo 2 Xpansion");
	    else if(strcasecmp(clienttag,CLIENTTAG_BNCHATBOT)==0)
	      sprintf(software," using a BOT");
	    else
	      sprintf(software," using a UNKNOWN Game.");
	    
	    if(account_check_mutual(conn_get_account(dest_c),myusername)==0)
	      {
		if ((game = conn_get_game(dest_c)))
		  {
		    sprintf(status, ", in game \"%.64s\",", game_get_name(game));
		  }
		else if ((channel = conn_get_channel(dest_c))) 
		  {
		    if(strcasecmp(channel_get_name(channel),"Arranged Teams")==0)
		      sprintf(status, ", in game AT Preparation...");
		    else                        			
		      sprintf(status, ", in channel \"%.64s\",", channel_get_name(channel));
		  } 
		else
		  sprintf(status, ", is in AT Preparation");
	      }
	    else
	      {
		if ((game = conn_get_game(dest_c)))
		  sprintf(status, ", is in a game.");
		else if ((channel = conn_get_channel(dest_c))) 
		  sprintf(status, ", is in a chat channel.");
		else
		  sprintf(status, ", is in AT Preparation");
	      }
	  }
	
	if (software[0]) sprintf(msgtemp, "%d: %s%s, %s", i+1, friend, status,software);
	else sprintf(msgtemp, "%d: %s%s", i+1, friend, status);
	message_send_text(c,message_type_info,c,msgtemp);
      }
    
    message_send_text(c,message_type_info,c,"=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
    message_send_text(c,message_type_info,c,"End of Friends List");
    
    return 0;
  } 
}

static int _handle_me_command(t_connection * c, char const * text)
{
  t_channel const * channel;
  unsigned int      i;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"You are not in a channel.");
      return 0;
    }
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++); /* skip spaces */
  
  if (!conn_quota_exceeded(c,&text[i]))
    channel_message_send(channel,message_type_emote,c,&text[i]);
  return 0;
}

static int _handle_whisper_command(t_connection * c, char const *text)
{
  char         dest[USER_NAME_MAX+REALM_NAME_LEN]; /* both include NUL, so no need to add one for middle @ or * */
  unsigned int i,j;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_error,c,"What do you want to say?");
      return 0;
    }
  
  do_whisper(c,dest,&text[i]);
  
  return 0;
}

static int _handle_status_command(t_connection * c, char const *text)
{
  char msgtemp[MAX_MESSAGE_LEN];
  
  sprintf(msgtemp,"There are currently %d users online, in %d games and %d channels.",
	  connlist_login_get_length(),
	  gamelist_get_length(),
	  channellist_get_length());
  message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_who_command(t_connection * c, char const *text)
{
  char                 msgtemp[MAX_MESSAGE_LEN];
#ifndef WITH_BITS
  t_connection const * conn;
#else
  t_bits_channelmember const * conn;
#endif
  t_channel const *    channel;
  unsigned int         i;
#ifndef WITH_BITS
  char const *         tname;
#endif
	
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (!(channel = channellist_find_channel_by_name(&text[i],conn_get_country(c),conn_get_realmname(c))))
    {
      message_send_text(c,message_type_error,c,"That channel does not exist.");
      message_send_text(c,message_type_error,c,"(If you are trying to search for a user, use the /whois command.)");
      return 0;
    }
  if (channel_check_banning(channel,c)==1)
    {
      message_send_text(c,message_type_error,c,"You are banned from that channel.");
      return 0;
    }
  
  sprintf(msgtemp,"Users in channel %.64s:",&text[i]);
  i = strlen(msgtemp);
  for (conn=channel_get_first(channel); conn; conn=channel_get_next())
    {
#ifndef WITH_BITS
      if (i+strlen((tname = conn_get_username(conn)))+2>sizeof(msgtemp)) /* " ", name, '\0' */
	{
	  message_send_text(c,message_type_info,c,msgtemp);
	  i = 0;
	}
      sprintf(&msgtemp[i]," %s",tname);
      conn_unget_username(conn,tname);
      i += strlen(&msgtemp[i]);
#else
      char const * name = bits_loginlist_get_name_bysessionid(conn->sessionid);
      
      if (!name) {
	eventlog(eventlog_level_error,"handle_command","FIXME: user without name");
	continue;
      }
      if (i+strlen(name)+2>sizeof(msgtemp)) /* " ", name, '\0' */
	{
	  message_send_text(c,message_type_info,c,msgtemp);
	  i = 0;
	}
      sprintf(&msgtemp[i]," %s",name);
      i += strlen(&msgtemp[i]);
#endif
    }
  if (i>0)
    message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_whois_command(t_connection * c, char const * text)
{
  unsigned int i;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  do_whois(c,&text[i]);
  
  return 0;
}

static int _handle_whoami_command(t_connection * c, char const *text)
{
  char const * tname;
  
  if (!(tname = conn_get_username(c)))
    {
      message_send_text(c,message_type_error,c,"Unable to obtain your account name.");
      return 0;
    }
  
  do_whois(c,tname);
  conn_unget_username(c,tname);
  
  return 0;
}

static int _handle_announce_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  unsigned int i;
  char const * tname;
  t_message *  message;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      account_get_auth_announce(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_info,c,"You do not have permission to use this command.");
      return 0;
    }
  
  sprintf(msgtemp,"Announcement from %.64s: %.128s",(tname = conn_get_username(c)),&text[i]);
  conn_unget_username(c,tname);
  if (!(message = message_create(message_type_broadcast,c,NULL,msgtemp)))
    message_send_text(c,message_type_info,c,"Could not broadcast message.");
  else
    {
      if (message_send_all(message)<0)
	message_send_text(c,message_type_info,c,"Could not broadcast message.");
      message_destroy(message);
    }
  
  return 0;
}

static int _handle_beep_command(t_connection * c, char const *text)
{
  message_send_text(c,message_type_info,c,"Audible notification on."); /* FIXME: actually do something */
  return 0; /* FIXME: these only affect CHAT clients... I think they prevent ^G from being sent */
}

static int _handle_nobeep_command(t_connection * c, char const *t)
{
  message_send_text(c,message_type_info,c,"Audible notification off."); /* FIXME: actually do something */
  return 0;
}

static int _handle_version_command(t_connection * c, char const *text)
{
  message_send_text(c,message_type_info,c,"PvPGN "PVPGN_VERSION);  
  return 0;
}

static int _handle_copyright_command(t_connection * c, char const *text)
{
  static char const * const info[] =
    {
      " Copyright (C) 2002  See source for details",
      " ",
      " PvPGN is free software; you can redistribute it and/or",
      " modify it under the terms of the GNU General Public License",
      " as published by the Free Software Foundation; either version 2",
      " of the License, or (at your option) any later version.",
      " ",
      " This program is distributed in the hope that it will be useful,",
      " but WITHOUT ANY WARRANTY; without even the implied warranty of",
      " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
      " GNU General Public License for more details.",
      " ",
      " You should have received a copy of the GNU General Public License",
      " along with this program; if not, write to the Free Software",
      " Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.",
      NULL
    };
  unsigned int i;
  
  for (i=0; info[i]; i++)
    message_send_text(c,message_type_info,c,info[i]);
  
  return 0;
}

static int _handle_uptime_command(t_connection * c, char const *text)
{
  char msgtemp[MAX_MESSAGE_LEN];
  
  sprintf(msgtemp,"Uptime: %s",seconds_to_timestr(server_get_uptime()));
  message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_stats_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  char         dest[USER_NAME_MAX];
  unsigned int i,j;
  t_account *  account;
  char const * clienttag;
  char const * tname;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  
#ifdef WITH_BITS
  if (bits_va_command_with_account_name(c,text,dest))
    return 0;
#endif
  // [quetzal] 20020815 - /stats without an argument should work as doing /stats yourself
  if (!dest[0]) {
    account = conn_get_account(c);
  } else if (!(account = accountlist_find_account(dest)))	{
    message_send_text(c,message_type_error,c,"Invalid user.");
    return 0;
  }
#ifdef WITH_BITS
  if (account_is_invalid(account)) {
    message_send_text(c,message_type_error,c,"Invalid user.");
    return 0;
  }
#endif
		
  if (text[i]!='\0')
    clienttag = &text[i];
  else if (!(clienttag = conn_get_clienttag(c)))
    {
      message_send_text(c,message_type_error,c,"Unable to determine client game.");
      return 0;
    }
  
  if (strlen(clienttag)!=4)
    {
      sprintf(msgtemp,"You must supply a user name and a valid program ID. (Program ID \"%.32s\" is invalid.)",clienttag);
      message_send_text(c,message_type_error,c,msgtemp);
      message_send_text(c,message_type_error,c,"Example: /stats joe STAR");
      return 0;
    }
  
  if (strcasecmp(clienttag,CLIENTTAG_BNCHATBOT)==0)
    {
      message_send_text(c,message_type_error,c,"This game does not support win/loss records.");
      message_send_text(c,message_type_error,c,"You must supply a user name and a valid program ID.");
      message_send_text(c,message_type_error,c,"Example: /stats joe STAR");
      return 0;
    }
  else if (strcasecmp(clienttag,CLIENTTAG_DIABLORTL)==0 ||
	   strcasecmp(clienttag,CLIENTTAG_DIABLOSHR)==0)
    {
      sprintf(msgtemp,"%.64s's record:",(tname = account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"level: %u",account_get_normal_level(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"class: %.16s",bnclass_get_str(account_get_normal_class(account,clienttag)));
      
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"stats: %u str  %u mag  %u dex  %u vit  %u gld",
	      account_get_normal_strength(account,clienttag),
	      account_get_normal_magic(account,clienttag),
	      account_get_normal_dexterity(account,clienttag),
	      account_get_normal_vitality(account,clienttag),
	      account_get_normal_gold(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"Diablo kills: %u",account_get_normal_diablo_kills(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
    }
  else if (strcasecmp(clienttag,CLIENTTAG_WARCIIBNE)==0)
    {
      sprintf(msgtemp,"%.64s's record:",(tname = account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"Normal games: %u-%u-%u",
	      account_get_normal_wins(account,clienttag),
	      account_get_normal_losses(account,clienttag),
	      account_get_normal_disconnects(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      
      if (account_get_ladder_rating(account,clienttag,ladder_id_normal)>0)
	sprintf(msgtemp,"Ladder games: %u-%u-%u (rating %d)",
		account_get_ladder_wins(account,clienttag,ladder_id_normal),
		account_get_ladder_losses(account,clienttag,ladder_id_normal),
		account_get_ladder_disconnects(account,clienttag,ladder_id_normal),
		account_get_ladder_rating(account,clienttag,ladder_id_normal));
	    else
	      strcpy(msgtemp,"Ladder games: 0-0-0");
      message_send_text(c,message_type_info,c,msgtemp);
      
      if (account_get_ladder_rating(account,clienttag,ladder_id_ironman)>0)
	sprintf(msgtemp,"IronMan games: %u-%u-%u (rating %d)",
		account_get_ladder_wins(account,clienttag,ladder_id_ironman),
		account_get_ladder_losses(account,clienttag,ladder_id_ironman),
		account_get_ladder_disconnects(account,clienttag,ladder_id_ironman),
		account_get_ladder_rating(account,clienttag,ladder_id_ironman));
      else
	strcpy(msgtemp,"IronMan games: 0-0-0");
      message_send_text(c,message_type_info,c,msgtemp);
    }
  else if (strcasecmp(clienttag,CLIENTTAG_WARCRAFT3)==0) // 7-31-02 THEUNDYING - Display stats for war3
    {
      sprintf(msgtemp,"%.64s's Ladder Record's:",(tname=account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"Users Solo Level: %u, Experience: %u",
	      account_get_sololevel(account),
	      account_get_soloxp(account));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"SOLO Ladder Record: %u-%u-0",
	      account_get_solowin(account),
	      account_get_sololoss(account));
      message_send_text(c,message_type_info,c,msgtemp);
      // aaron -->
      sprintf(msgtemp,"SOLO Rank: %u",
	      account_get_solorank(account));
      message_send_text(c,message_type_info,c,msgtemp);
      // <---
      
      sprintf(msgtemp,"Users Team Level: %u, Experience: %u",
	      account_get_teamlevel(account),
	      account_get_teamxp(account));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"TEAM Ladder Record: %u-%u-0",
	      account_get_teamwin(account),
	      account_get_teamloss(account));
      message_send_text(c,message_type_info,c,msgtemp);
      // aaron -->
      sprintf(msgtemp,"TEAM Rank: %u",
	      account_get_teamrank(account));
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"Users FFA Level: %u, Experience: %u",
	      account_get_ffalevel(account),
	      account_get_ffaxp(account));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"FFA Ladder Record: %u-%u-0",
	      account_get_ffawin(account),
	      account_get_ffaloss(account));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"FFA Rank: %u",
	      account_get_ffarank(account));
      message_send_text(c,message_type_info,c,msgtemp);
      if (account_get_atteamcount(account))
	{
	  int teamcount;
	  for (teamcount=0; teamcount<account_get_atteamcount(account); teamcount++)
	    {
	      sprintf(msgtemp,"Users AT Team No. %u",teamcount);
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"Users AT TEAM Level: %u, Experience: %u",
		      account_get_atteamlevel(account,teamcount),
		      account_get_atteamxp(account,teamcount));
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"AT TEAM Ladder Record: %u-%u-0",
		      account_get_atteamwin(account,teamcount),
		      account_get_atteamloss(account,teamcount));
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"AT TEAM Rank: %u",
		      account_get_atteamrank(account,teamcount));
	      message_send_text(c,message_type_info,c,msgtemp);
	    }
	  
	}
      // <---
    }	
  else
    {
      sprintf(msgtemp,"%.64s's record:",(tname = account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"Normal games: %u-%u-%u",
	      account_get_normal_wins(account,clienttag),
	      account_get_normal_losses(account,clienttag),
	      account_get_normal_disconnects(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      
      if (account_get_ladder_rating(account,clienttag,ladder_id_normal)>0)
	sprintf(msgtemp,"Ladder games: %u-%u-%u (rating %d)",
		account_get_ladder_wins(account,clienttag,ladder_id_normal),
		account_get_ladder_losses(account,clienttag,ladder_id_normal),
		account_get_ladder_disconnects(account,clienttag,ladder_id_normal),
		account_get_ladder_rating(account,clienttag,ladder_id_normal));
      else
	strcpy(msgtemp,"Ladder games: 0-0-0");
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_time_command(t_connection * c, char const *text)
{
  char        msgtemp[MAX_MESSAGE_LEN];
  t_bnettime  btsystem;
  t_bnettime  btlocal;
  time_t      now;
  struct tm * tmnow;
  
  btsystem = bnettime();
  
  /* Battle.net time: Wed Jun 23 15:15:29 */
  btlocal = bnettime_add_tzbias(btsystem,local_tzbias());
  now = bnettime_to_time(btlocal);
  if (!(tmnow = gmtime(&now)))
    strcpy(msgtemp,"PvPGN Server Time: ?");
  else
    strftime(msgtemp,sizeof(msgtemp),"PvPGN Server Time: %a %b %d %H:%M:%S",tmnow);
  message_send_text(c,message_type_info,c,msgtemp);
  if (conn_get_class(c)==conn_class_bnet)
    {
      btlocal = bnettime_add_tzbias(btsystem,conn_get_tzbias(c));
      now = bnettime_to_time(btlocal);
      if (!(tmnow = gmtime(&now)))
	strcpy(msgtemp,"Your local time: ?");
      else
	strftime(msgtemp,sizeof(msgtemp),"Your local time: %a %b %d %H:%M:%S",tmnow);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_channel_command(t_connection * c, char const *text)
 {
   unsigned int i;
   
   for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
   for (; text[i]==' '; i++);
   
   if (text[i]=='\0')
     {
       message_send_text(c,message_type_error,c,"Please specify a channel.");
       return 0;
     }
   
   if(strcasecmp(&text[i],"Arranged Teams")==0)
     {
       if(account_get_auth_admin(conn_get_account(c))>0)
	 {
	   message_send_text(c,message_type_error,c,"Please do not talk in channel Arranged Teams");
	   message_send_text(c,message_type_error,c,"This channel is dedicated for the preparation of");
	   message_send_text(c,message_type_error,c,"Arranged Team Games.");
	 }
       else
	 {
	   message_send_text(c,message_type_error,c,"Channel Arranged Teams is a RESTRICTED Channel!");
	   return 0;
	 }
     }
   
   if (conn_set_channel(c,&text[i])<0)
     conn_set_channel(c,CHANNEL_NAME_BANNED); /* should not fail */
   
   return 0;
 }

static int _handle_rejoin_command(t_connection * c, char const *text)
{
  t_channel const * channel;
  char const *      temp;
  char const *      chname;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"You are not in a channel.");
      return 0;
    }
  
  if (!(temp = channel_get_name(channel)))
    return 0;
  
  /* we need to copy the channel name because we might remove the
     last person (ourself) from the channel and cause the string
     to be freed in destroy_channel() */
  if ((chname = strdup(temp)))
    {
      if (conn_set_channel(c,chname)<0)
	conn_set_channel(c,CHANNEL_NAME_BANNED); /* should not fail */
      free((void *)chname); /* avoid warning */
    }
  
  return 0;
}

static int _handle_away_command(t_connection * c, char const *text)
{
  unsigned int i;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0') /* back to normal */
    {
      message_send_text(c,message_type_info,c,"You are no longer marked as away.");
      conn_set_awaystr(c,NULL);
    }
  else
    {
      message_send_text(c,message_type_info,c,"You are now marked as being away.");
      conn_set_awaystr(c,&text[i]);
    }
  
  return 0;
}

static int _handle_dnd_command(t_connection * c, char const *text)
{
  unsigned int i;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0') /* back to normal */
    {
      message_send_text(c,message_type_info,c,"Do Not Disturb mode cancelled.");
      conn_set_dndstr(c,NULL);
    }
  else
    {
      message_send_text(c,message_type_info,c,"Do Not Disturb mode engaged.");
      conn_set_dndstr(c,&text[i]);
    }
  
  return 0;
}

static int _handle_squelch_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  unsigned int i;
  t_account *  account;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  /* D2 puts * before username - FIXME: the client don't see it until
     the player rejoins the channel */
  if (text[i]=='*')
    i++;
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to ignore?");
      return 0;
    }
  
  if (!(account = accountlist_find_account(&text[i])))
    {
      message_send_text(c,message_type_error,c,"No such user.");
      return 0;
    }
  
  if (conn_get_account(c)==account)
    {
      message_send_text(c,message_type_error,c,"You can't squelch yourself.");
      return 0;
    }
  
  if (conn_add_ignore(c,account)<0)
    message_send_text(c,message_type_error,c,"Could not squelch user.");
  else
    {
      char const * tname;
      
      sprintf(msgtemp,"%-.20s has been squelched.",(tname = account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_unsquelch_command(t_connection * c, char const *text)
{
  unsigned int      i;
  t_account const * account;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  /* D2 puts * before username - FIXME: the client don't see it until
     the player rejoins the channel */
  if (text[i]=='*')
    i++;
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who don't you want to ignore?");
      return 0;
    }
  
  if (!(account = accountlist_find_account(&text[i])))
    {
      message_send_text(c,message_type_info,c,"No such user.");
      return 0;
    }
  
  if (conn_del_ignore(c,account)<0)
    message_send_text(c,message_type_info,c,"User was not being ignored.");
  else
    message_send_text(c,message_type_info,c,"No longer ignoring.");
  
  return 0;
}

static int _handle_designate_command(t_connection * c, char const *text)
{
  char           msgtemp[MAX_MESSAGE_LEN];
  unsigned int   i;
  t_channel *    channel;
  t_connection * noc;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to designate?");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      channel_get_operator(channel)!=c)
    {
      message_send_text(c,message_type_error,c,"You are not a channel operator.");
      return 0;
    }
  if (!(noc = connlist_find_connection_by_accountname(&text[i])))
    {
      message_send_text(c,message_type_error,c,"That user is not logged in.");
      return 0;
    }
  if (conn_get_channel(noc)!=channel)
    {
      message_send_text(c,message_type_error,c,"That user is not in this channel.");
      return 0;
    }
  
  if (channel_set_next_operator(channel,noc)<0)
    message_send_text(c,message_type_error,c,"Unable to designate that user.");
  else
    {
      char const * tname;
      
      sprintf(msgtemp,"%s will be the new operator when you resign.",(tname = conn_get_username(noc)));
      conn_unget_username(noc,tname);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_resign_command(t_connection * c, char const *text)
{
  t_channel *    channel;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (channel_get_operator(channel)!=c)
    {
      message_send_text(c,message_type_error,c,"This command is reserved for channel operators.");
      return 0;
    }
  
  if (channel_choose_operator(channel,NULL)<0)
    message_send_text(c,message_type_error,c,"You are unable to resign.");
  else
    message_send_text(c,message_type_info,c,"You are no longer the operator.");
  
  return 0;
}

static int _handle_kick_command(t_connection * c, char const *text)
{
  char              msgtemp[MAX_MESSAGE_LEN];
  char              dest[USER_NAME_MAX];
  unsigned int      i,j;
  t_channel const * channel;
  t_connection *    kuc;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to kick off the channel?");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      channel_get_operator(channel)!=c)
    {
      message_send_text(c,message_type_error,c,"You are not a channel operator.");
      return 0;
    }
  if (!(kuc = connlist_find_connection_by_accountname(dest)))
    {
      message_send_text(c,message_type_error,c,"That user is not logged in.");
      return 0;
    }
  if (conn_get_channel(kuc)!=channel)
    {
      message_send_text(c,message_type_error,c,"That user is not in this channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(kuc))==1)
    {
      message_send_text(c,message_type_error,c,"You cannot kick administrators.");
      return 0;
    }
  
  {
    char const * tname1;
    char const * tname2;
    
    tname1 = account_get_name(conn_get_account(kuc));
    tname2 = account_get_name(conn_get_account(c));
    if (text[i]!='\0')
      sprintf(msgtemp,"%-.20s has been kicked by %-.20s (%s).",tname1,tname2?tname2:"unknown",&text[i]);
    else
      sprintf(msgtemp,"%-.20s has been kicked by %-.20s.",tname1,tname2?tname2:"unknown");
    if (tname2)
      account_unget_name(tname2);
    if (tname1)
      account_unget_name(tname1);
    channel_message_send(channel,message_type_info,c,msgtemp);
  }
  conn_set_channel(kuc,CHANNEL_NAME_KICKED); /* should not fail */
  
  return 0;
}

static int _handle_ban_command(t_connection * c, char const *text)
{
  char           msgtemp[MAX_MESSAGE_LEN];
  char           dest[USER_NAME_MAX];
  unsigned int   i,j;
  t_channel *    channel;
  t_connection * buc;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to ban from the channel?");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      channel_get_operator(channel)!=c)
    {
      message_send_text(c,message_type_error,c,"You are not a channel operator.");
      return 0;
    }
  {
    t_account * account;
    
    if (!(account = accountlist_find_account(dest)))
      message_send_text(c,message_type_info,c,"That account doesn't currently exist, banning anyway.");
    else
      if (account_get_auth_admin(account)==1) /* default to false */
	{
	  message_send_text(c,message_type_error,c,"You cannot ban administrators.");
	  return 0;
	}
  }
  
  if (channel_ban_user(channel,dest)<0)
    {
      sprintf(msgtemp,"Unable to ban %-.20s.",dest);
      message_send_text(c,message_type_error,c,msgtemp);
    }
  else
    {
      char const * tname;
      
      tname = account_get_name(conn_get_account(c));
      if (text[i]!='\0')
	sprintf(msgtemp,"%-.20s has been banned by %-.20s (%s).",dest,tname?tname:"unknown",&text[i]);
      else
	sprintf(msgtemp,"%-.20s has been banned by %-.20s.",dest,tname?tname:"unknown");
      if (tname)
	account_unget_name(tname);
      channel_message_send(channel,message_type_info,c,msgtemp);
    }
  if ((buc = connlist_find_connection_by_accountname(dest)) &&
      conn_get_channel(buc)==channel)
    conn_set_channel(buc,CHANNEL_NAME_BANNED);
  
  return 0;
}

static int _handle_unban_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  t_channel *  channel;
  unsigned int i;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to unban from the channel?");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      channel_get_operator(channel)!=c)
    {
      message_send_text(c,message_type_error,c,"You are not a channel operator.");
      return 0;
    }
  
  if (channel_unban_user(channel,&text[i])<0)
    message_send_text(c,message_type_error,c,"That user is not banned.");
  else
    {
      sprintf(msgtemp,"%s is no longer banned from this channel.",&text[i]);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_reply_command(t_connection * c, char const *text)
{
  unsigned int i;
  char const * dest;
  
  if (!(dest = conn_get_lastsender(c)))
    {
      message_send_text(c,message_type_error,c,"No one messaged you, use /m instead");
      return 0;
    }
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_error,c,"What do you want to reply?");
      return 0;
    }
  do_whisper(c,dest,&text[i]);
  return 0;
}

static int _handle_realmann_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  unsigned int i;
  char const * realmname;
  char const * tname;
  t_connection * tc;
  t_elem const * curr;
  t_message    * message;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (account_get_auth_admin(conn_get_account(c))!=1 && /* default to false */
      account_get_auth_announce(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_info,c,"You do not have permission to use this command.");
      return 0;
    }
  if (!(realmname=conn_get_realmname(c))) {
    message_send_text(c,message_type_info,c,"You must join a realm first");
  }
  sprintf(msgtemp,"Announcement from %.32s@%.32s: %.128s",(tname = conn_get_username(c)),realmname,&text[i]);
  conn_unget_username(c,tname);
  if (!(message = message_create(message_type_broadcast,c,NULL,msgtemp)))
    {
      message_send_text(c,message_type_info,c,"Could not broadcast message.");
    }
  else
    {
      LIST_TRAVERSE_CONST(connlist(),curr)
	{
	  tc = elem_get_data(curr);
	  if (!tc)
	    continue;
	  if ((conn_get_realmname(tc))&&(strcasecmp(conn_get_realmname(tc),realmname)==0))
	    {
	      message_send(message,tc);
	    }
	}
    }
  return 0;
}

static int _handle_watch_command(t_connection * c, char const *text)
{
  unsigned int i;
  char         msgtemp[MAX_MESSAGE_LEN];
  t_account *  account;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_info,c,"Who do you want to watch?");
      return 0;
    }
  if (!(account = accountlist_find_account(&text[i])))
    {
      message_send_text(c,message_type_info,c,"That user does not exist.");
      return 0;
    }
  
  if (conn_add_watch(c,account)<0) /* FIXME: adds all events for now */
    message_send_text(c,message_type_error,c,"Add to watch list failed.");
  else
    {
      sprintf(msgtemp,"User %.64s added to your watch list.",&text[i]);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_unwatch_command(t_connection * c, char const *text)
 {
   unsigned int i;
   char         msgtemp[MAX_MESSAGE_LEN];
   t_account *  account;
   
   for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
   for (; text[i]==' '; i++);
   
   if (text[i]=='\0')
     {
       message_send_text(c,message_type_info,c,"Who do you want to unwatch?");
       return 0;
     }
   if (!(account = accountlist_find_account(&text[i])))
     {
       message_send_text(c,message_type_info,c,"That user does not exist.");
       return 0;
     }
   
   if (conn_del_watch(c,account)<0) /* FIXME: deletes all events for now */
     message_send_text(c,message_type_error,c,"Removal from watch list failed.");
   else
     {
       sprintf(msgtemp,"User %.64s removed from your watch list.",&text[i]);
       message_send_text(c,message_type_info,c,msgtemp);
     }
   
   return 0;
 }

static int _handle_watchall_command(t_connection * c, char const *text)
{
  if (conn_add_watch(c,NULL)<0) /* FIXME: adds all events for now */
    message_send_text(c,message_type_error,c,"Add to watch list failed.");
  else
    message_send_text(c,message_type_info,c,"All users added to your watch list.");
  return 0;
}

static int _handle_unwatchall_command(t_connection * c, char const *text)
{
  if (conn_del_watch(c,NULL)<0) /* FIXME: deletes all events for now */
    message_send_text(c,message_type_error,c,"Removal from watch list failed.");
  else
    message_send_text(c,message_type_info,c,"All users removed from your watch list.");
  return 0;
}

static int _handle_lusers_command(t_connection * c, char const *text)
{
  char           msgtemp[MAX_MESSAGE_LEN];
  t_channel *    channel;
  t_elem const * curr;
  char const *   banned;
  unsigned int   i;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  
  strcpy(msgtemp,"Banned users:");
  i = strlen(msgtemp);
  LIST_TRAVERSE_CONST(channel_get_banlist(channel),curr)
    {
      banned = elem_get_data(curr);
      if (i+strlen(banned)+2>sizeof(msgtemp)) /* " ", name, '\0' */
	{
	  message_send_text(c,message_type_info,c,msgtemp);
	  i = 0;
	}
      sprintf(&msgtemp[i]," %s",banned);
      i += strlen(&msgtemp[i]);
    }
  if (i>0)
    message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_news_command(t_connection * c, char const *text)
{
  char const * filename;
  FILE *       fp;
  
  if ((filename = prefs_get_newsfile()))
    if ((fp = fopen(filename,"r")))
      {
	message_send_file(c,fp);
	if (fclose(fp)<0)
	  eventlog(eventlog_level_error,"handle_command","could not close news file \"%s\" after reading (fclose: %s)",filename,strerror(errno));
      }
    else
      {
	eventlog(eventlog_level_error,"handle_command","could not open news file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	message_send_text(c,message_type_info,c,"No news today.");
      }
  else
    message_send_text(c,message_type_info,c,"No news today.");
  
  return 0;
}

static int _handle_games_command(t_connection * c, char const *text)
{
  unsigned int   i;
  char           msgtemp[MAX_MESSAGE_LEN];
  t_elem const * curr;
#ifndef WITH_BITS
  t_game const * game;
#else
  t_bits_game const * bgame;
#endif
  char const *   tag;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      tag = conn_get_clienttag(c);
      message_send_text(c,message_type_info,c,"Currently accessable games:");
    }
  else if (strcmp(&text[i],"all")==0)
    {
      tag = NULL;
      message_send_text(c,message_type_info,c,"All current games:");
    }
  else
    {
      tag = &text[i];
      message_send_text(c,message_type_info,c,"Current games of that type:");
    }
  
  if (prefs_get_hide_addr() && account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    sprintf(msgtemp," ------name------ p -status- --------type--------- count");
  else
    sprintf(msgtemp," ------name------ p -status- --------type--------- count --------addr--------");
  message_send_text(c,message_type_info,c,msgtemp);
#ifndef WITH_BITS
  LIST_TRAVERSE_CONST(gamelist(),curr)
#else
    LIST_TRAVERSE_CONST(bits_gamelist(),curr)
#endif
    {
#ifdef WITH_BITS
      t_game * game;
      
      bgame = elem_get_data(curr);
      game = bits_game_create_temp(bits_game_get_id(bgame));
#else
      game = elem_get_data(curr);
#endif
      if ((!tag || !prefs_get_hide_pass_games() || strcmp(game_get_pass(game),"")==0) &&
	  (!tag || strcasecmp(game_get_clienttag(game),tag)==0))
	{
	  if (prefs_get_hide_addr() && account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
	    sprintf(msgtemp," %-16.16s %1.1s %-8.8s %-21.21s %5u",
		    game_get_name(game),
		    strcmp(game_get_pass(game),"")==0 ? "n":"y",
		    game_status_get_str(game_get_status(game)),
		    game_type_get_str(game_get_type(game)),
		    game_get_ref(game));
	  else
	    sprintf(msgtemp," %-16.16s %1.1s %-8.8s %-21.21s %5u %s",
		    game_get_name(game),
		    strcmp(game_get_pass(game),"")==0 ? "n":"y",
		    game_status_get_str(game_get_status(game)),
		    game_type_get_str(game_get_type(game)),
		    game_get_ref(game),
		    addr_num_to_addr_str(game_get_addr(game),game_get_port(game)));
	  message_send_text(c,message_type_info,c,msgtemp);
	}
#ifdef WITH_BITS
      bits_game_destroy_temp(game);
#endif
    }
  
  return 0;
}

static int _handle_channels_command(t_connection * c, char const *text)
{
  unsigned int      i;
  char              msgtemp[MAX_MESSAGE_LEN];
  t_elem const *    curr;
  t_channel const * channel;
  t_connection *    opr;
  char const *      oprname;
  char const *      tag;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      tag = conn_get_clienttag(c);
      message_send_text(c,message_type_info,c,"Currently accessable channels:");
    }
  else if (strcmp(&text[i],"all")==0)
    {
      tag = NULL;
      message_send_text(c,message_type_info,c,"All current channels:");
    }
  else
    {
      tag = &text[i];
      message_send_text(c,message_type_info,c,"Current channels of that type:");
    }
  
  sprintf(msgtemp," ----------name---------- users ----operator----");
  message_send_text(c,message_type_info,c,msgtemp);
  LIST_TRAVERSE_CONST(channellist(),curr)
    {
      channel = elem_get_data(curr);
      if ((!tag || !prefs_get_hide_temp_channels() || channel_get_permanent(channel)) &&
	  (!tag || !channel_get_clienttag(channel) ||
	   strcasecmp(channel_get_clienttag(channel),tag)==0))
	{
	  if ((opr = channel_get_operator(channel)))
	    oprname = conn_get_username(opr);
	  else
	    oprname = NULL;
	  sprintf(msgtemp," %-24.24s %5u %-16.16s",
		  channel_get_name(channel),
		  channel_get_length(channel),
		  oprname?oprname:"");
	  if (oprname)
	    conn_unget_username(opr,oprname);
	  message_send_text(c,message_type_info,c,msgtemp);
	}
    }
  
  return 0;
}

static int _handle_addacct_command(t_connection * c, char const *text)
{
  unsigned int i,j;
  t_account  * temp;
  t_hash       passhash;
  char         username[USER_NAME_MAX];
  char         msgtemp[MAX_MESSAGE_LEN];
  char         pass[256];
  
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is only enabled for admins.");
      return 0;
    }
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++);
  for (; text[i]==' '; i++);
  
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get username */
    if (j<sizeof(username)-1) username[j++] = text[i];
  username[j] = '\0';
  
  for (; text[i]==' '; i++); /* skip spaces */
  for (j=0; text[i]!='\0'; i++) /* get pass (spaces are allowed) */
    if (j<sizeof(pass)-1) pass[j++] = text[i];
  pass[j] = '\0';
  
  if (username[0]=='\0' || pass[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"Command requires USER and PASS as arguments.");
      return 0;
    }
  
  /* FIXME: truncate or err on too long password */
  for (i=0; i<strlen(pass); i++)
    if (isupper((int)pass[i])) pass[i] = tolower((int)pass[i]);
  
  bnet_hash(&passhash,strlen(pass),pass);
  
  sprintf(msgtemp,"Trying to add account \"%s\" with password \"%s\"",username,pass);
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Hash is: %s",hash_get_str(passhash));
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (!(temp = account_create(username,hash_get_str(passhash))))
    {
      message_send_text(c,message_type_error,c,"Failed to create account!");
      eventlog(eventlog_level_info,"handle_command","[%d] account \"%s\" not created by admin (failed)",conn_get_socket(c),username);
      return 0;
    }
  if (!accountlist_add_account(temp))
    {
      account_destroy(temp);
      message_send_text(c,message_type_error,c,"Failed to insert account (already exists?)!");
      eventlog(eventlog_level_info,"handle_command","[%d] account \"%s\" could not be created by admin (insert failed)",conn_get_socket(c),username);
    }
  else
    {
      sprintf(msgtemp,"Account "UID_FORMAT" created.",account_get_uid(temp));
      message_send_text(c,message_type_info,c,msgtemp);
      eventlog(eventlog_level_info,"handle_command","[%d] account \"%s\" created by admin",conn_get_socket(c),username);
    }
  return 0;
}

static int _handle_chpass_command(t_connection * c, char const *text)
{
  unsigned int i,j;
  t_account  * account;
  t_account  * temp;
  t_hash       passhash;
  char         msgtemp[MAX_MESSAGE_LEN];
  char         arg1[256];
  char         arg2[256];
  char const * username;
  char *       pass;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++);
  for (; text[i]==' '; i++);
  
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get username/pass */
    if (j<sizeof(arg1)-1) arg1[j++] = text[i];
  arg1[j] = '\0';
  
  for (; text[i]==' '; i++); /* skip spaces */
  for (j=0; text[i]!='\0'; i++) /* get pass (spaces are allowed) */
    if (j<sizeof(arg2)-1) arg2[j++] = text[i];
  arg2[j] = '\0';
  
  if (arg2[0]=='\0')
    {
      username = conn_get_username(c);
      pass     = arg1;
    }
  else
    {
      username = arg1;
      pass     = arg2;
    }
  
  if (pass[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"Command requires PASS argument.");
      return 0;
    }
  
  /* FIXME: truncate or err on too long password */
  for (i=0; i<strlen(pass); i++)
    if (isupper((int)pass[i])) pass[i] = tolower((int)pass[i]);
  
  bnet_hash(&passhash,strlen(pass),pass);
  
  sprintf(msgtemp,"Trying to change password for account \"%s\" to \"%s\"",username,pass);
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Hash is: %s",hash_get_str(passhash));
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (!(temp = accountlist_find_account(username)))
    {
      message_send_text(c,message_type_error,c,"Account does not exist.");
      if (username!=arg1)
	conn_unget_username(c,username);
      return 0;
    }
  
  account = conn_get_account(c);
  
  if ((temp==account && account_get_auth_changepass(account)==0) || /* default to true */
      (temp!=account && account_get_auth_admin(account)!=1)) /* default to false */
    {
      eventlog(eventlog_level_info,"handle_command","[%d] password change for \"%s\" refused (no change access)",conn_get_socket(c),username);
      if (username!=arg1)
	conn_unget_username(c,username);
      message_send_text(c,message_type_error,c,"Only admins may change passwords for other accounts.");
      return 0;
    }
  
  if (username!=arg1)
    conn_unget_username(c,username);
  
  if (account_set_pass(temp,hash_get_str(passhash))<0)
    {
      message_send_text(c,message_type_error,c,"Unable to set password.");
      return 0;
    }
  
  sprintf(msgtemp,"Password for account "UID_FORMAT" updated.",account_get_uid(temp));
  message_send_text(c,message_type_info,c,msgtemp);
  return 0;
}

static int _handle_connections_command(t_connection *c, char const *text)
{
  char           msgtemp[MAX_MESSAGE_LEN];
  t_elem const * curr;
  t_connection * conn;
  char           name[19];
  unsigned int   i; /* for loop */
  char const *   channel_name;
  char const *   game_name;
  
  if (!prefs_get_enable_conn_all() && account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is only enabled for admins.");
      return 0;
    }
  
  message_send_text(c,message_type_info,c,"Current connections:");
  /* addon */
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++); 
  
  if (text[i]=='\0')
    {
      sprintf(msgtemp," -class -tag -----name------ -lat(ms)- ----channel---- --game--");
      message_send_text(c,message_type_info,c,msgtemp);
    }
  else
    if (strcmp(&text[i],"all")==0) /* print extended info */
      {
	if (prefs_get_hide_addr() && account_get_auth_admin(conn_get_account(c))!=1)
	  sprintf(msgtemp," -#- -class ----state--- -tag -----name------ -session-- -flag- -lat(ms)- ----channel---- --game--");
	else
	  sprintf(msgtemp," -#- -class ----state--- -tag -----name------ -session-- -flag- -lat(ms)- ----channel---- --game-- ---------addr--------");
	message_send_text(c,message_type_info,c,msgtemp);
      }
    else
      {
	message_send_text(c,message_type_error,c,"Unknown option.");
	return 0;
      }
  
  LIST_TRAVERSE_CONST(connlist(),curr)
    {
      conn = elem_get_data(curr);
      if (conn_get_account(conn))
	{
	  char const * tname;
	  
	  sprintf(name,"\"%.16s\"",(tname = conn_get_username(conn)));
	  conn_unget_username(conn,tname);
	}
      else
	strcpy(name,"(none)");
      
      if (conn_get_channel(conn)!=NULL)
	channel_name = channel_get_name(conn_get_channel(conn));
      else channel_name = "none";
      if (conn_get_game(conn)!=NULL)
	game_name = game_get_name(conn_get_game(conn));
      else game_name = "none";
      
      if (text[i]=='\0')
	sprintf(msgtemp," %-6.6s %4.4s %-15.15s %9u %-16.16s %-8.8s",
		conn_class_get_str(conn_get_class(conn)),
		conn_get_fake_clienttag(conn),
		name, 
		conn_get_latency(conn), 
		channel_name,
		game_name);
      else
	if (prefs_get_hide_addr() && account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
	  sprintf(msgtemp," %3d %-6.6s %-12.12s %4.4s %-15.15s 0x%08x 0x%04x %9u %-16.16s %-8.8s", 
		  conn_get_socket(conn),
		  conn_class_get_str(conn_get_class(conn)), 
		  conn_state_get_str(conn_get_state(conn)),
		  conn_get_fake_clienttag(conn),
		  name,
		  conn_get_sessionkey(conn),
		  conn_get_flags(conn),
		  conn_get_latency(conn),
		  channel_name,
		  game_name);
	else
	  sprintf(msgtemp," %3u %-6.6s %-12.12s %4.4s %-15.15s 0x%08x 0x%04x %9u %-16.16s %-8.8s %s",
		  conn_get_socket(conn),
		  conn_class_get_str(conn_get_class(conn)),
		  conn_state_get_str(conn_get_state(conn)),
		  conn_get_fake_clienttag(conn),
		  name,
		  conn_get_sessionkey(conn),
		  conn_get_flags(conn),
		  conn_get_latency(conn),
		  channel_name,
		  game_name,
		  addr_num_to_addr_str(conn_get_addr(conn),conn_get_port(conn)));
      
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_finger_command(t_connection * c, char const *text)
{
  char           dest[USER_NAME_MAX];
  char           msgtemp[MAX_MESSAGE_LEN];
  unsigned int   i,j;
  t_account *    account;
  t_connection * conn;
  char const *   host;
  char *         tok;
  char const *   tname;
  char const *   tsex;
  char const *   tloc;
  char const *   tage;
  char const *   thost;
  char const *   tdesc;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
#ifdef WITH_BITS
  if (bits_va_command_with_account_name(c,text,dest))
    return 0;
#endif
  
  if (!(account = accountlist_find_account(dest)))
    {
      message_send_text(c,message_type_error,c,"Invalid user.");
      return 0;
    }
#ifdef WITH_BITS
  if (account_is_invalid(account)) {
    message_send_text(c,message_type_error,c,"Invalid user.");
    return 0;
  }
#endif
  sprintf(msgtemp,"Login: %-16.16s "UID_FORMAT" Sex: %.14s",
	  (tname = account_get_name(account)),
	  account_get_uid(account),
	  (tsex = account_get_sex(account)));
  account_unget_name(tname);
  account_unget_sex(tsex);
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Location: %-23.23s Age: %.14s",
	  (tloc = account_get_loc(account)),
	  (tage = account_get_age(account)));
  account_unget_loc(tloc);
  account_unget_age(tage);
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (!(host=thost = account_get_ll_host(account)) ||
      account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    host = "unknown";
  
  {
    time_t      then;
    struct tm * tmthen;
    
    then = account_get_ll_time(account);
    tmthen = localtime(&then); /* FIXME: determine user's timezone */
    if (!(conn = connlist_find_connection_by_accountname(dest)))
      if (tmthen)
	strftime(msgtemp,sizeof(msgtemp),"Last login %a %b %d %H:%M from ",tmthen);
      else
	strcpy(msgtemp,"Last login ? from ");
    else
      if (tmthen)
	strftime(msgtemp,sizeof(msgtemp),"On since %a %b %d %H:%M from ",tmthen);
      else
	strcpy(msgtemp,"On since ? from ");
  }
  strncat(msgtemp,host,32);
  if (thost)
    account_unget_ll_host(thost);
  
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (conn)
    {
      sprintf(msgtemp,"Idle %s",seconds_to_timestr(conn_get_idletime(conn)));
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  strncpy(msgtemp,(tdesc = account_get_desc(account)),sizeof(msgtemp));
  msgtemp[sizeof(msgtemp)-1] = '\0';
  account_unget_desc(tdesc);
  for (tok=strtok(msgtemp,"\r\n"); tok; tok=strtok(NULL,"\r\n"))
    message_send_text(c,message_type_info,c,tok);
  message_send_text(c,message_type_info,c,"");
  
  return 0;
}

static int _handle_operator_command(t_connection * c, char const *text)
{
  char                 msgtemp[MAX_MESSAGE_LEN];
  t_connection const * opr;
  t_channel const *    channel;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  
  if (!(opr = channel_get_operator(channel)))
    strcpy(msgtemp,"There is no operator.");
  else
    {
      char const * tname;
      
      sprintf(msgtemp,"%.64s is the operator.",(tname = conn_get_username(opr)));
      conn_unget_username(opr,tname);
    }
  message_send_text(c,message_type_info,c,msgtemp);
  return 0;
}

static int _handle_admins_command(t_connection * c, char const *text)
{
  char            msgtemp[MAX_MESSAGE_LEN];
  unsigned int    i;
  t_elem const *  curr;
  t_connection *  tc;
  char const *    nick;
  
  strcpy(msgtemp,"Currently logged on Administrators:");
  i = strlen(msgtemp);
  LIST_TRAVERSE_CONST(connlist(),curr)
    {
      tc = elem_get_data(curr);
      if (!tc)
	continue;
      if (account_get_auth_admin(conn_get_account(tc))==1)
	{
	  if ((nick = conn_get_username(tc)))
	    {
	      if (i+strlen(nick)+2>sizeof(msgtemp)) /* " ", name, '\0' */
		{
		  message_send_text(c,message_type_info,c,msgtemp);
		  i = 0;
		}
	      sprintf(&msgtemp[i]," %s", nick);
	      i += strlen(&msgtemp[i]);
	      conn_unget_username(tc,nick);
	    }
	}
    }
  if (i>0)
    message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_quit_command(t_connection * c, char const *text)
{
  message_send_text(c,message_type_info,c,"Connection closed.");
  conn_set_state(c,conn_state_destroy);
  return 0;
}

static int _handle_kill_command(t_connection * c, char const *text)
{
  unsigned int	i,j;
  t_connection *	user;
  char		usrnick[16]; /* max length of nick + \0 */  /* FIXME: Is it somewhere defined? */
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get nick */
    if (j<sizeof(usrnick)-1) usrnick[j++] = text[i];
  usrnick[j]='\0';
  for (; text[i]==' '; i++);
  
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  if (usrnick[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"Which user do you want to kill?");
      return 0;
    }
  if (!(user = connlist_find_connection_by_accountname(usrnick)))
    {
      message_send_text(c,message_type_error,c,"That user is not logged in?");
      return 0;
    }
  if (text[i]!='\0' && ipbanlist_add(c,addr_num_to_ip_str(conn_get_addr(user)),ipbanlist_str_to_time_t(c,&text[i]))==0)
    message_send_text(user,message_type_info,user,"Connection closed by admin and banned your ip.");
  else
    message_send_text(user,message_type_info,user,"Connection closed by admin.");
  conn_set_state(user,conn_state_destroy);
  return 0;
}

static int _handle_killsession_command(t_connection * c, char const *text)
{
  unsigned int	i,j;
  t_connection *	user;
  char		session[16];
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get nick */
    if (j<sizeof(session)-1) session[j++] = text[i];
  session[j]='\0';
  for (; text[i]==' '; i++);
  
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  if (session[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"Which session do you want to kill?");
      return 0;
    }
  if (!isxdigit((int)session[0]))
    {
      message_send_text(c,message_type_error,c,"That is not a valid session.");
      return 0;
    }
  if (!(user = connlist_find_connection_by_sessionkey((unsigned int)strtoul(session,NULL,16))))
    {
      message_send_text(c,message_type_error,c,"That session does not exist.");
      return 0;
    }
  if (text[i]!='\0' && ipbanlist_add(c,addr_num_to_ip_str(conn_get_addr(user)),ipbanlist_str_to_time_t(c,&text[i]))==0)
    message_send_text(user,message_type_info,user,"Connection closed by admin and banned your ip's.");
  else
    message_send_text(user,message_type_info,user,"Connection closed by admin.");
  conn_set_state(user,conn_state_destroy);
  return 0;
}

static int _handle_gameinfo_command(t_connection * c, char const *text)
{
  unsigned int   i;
  char           msgtemp[MAX_MESSAGE_LEN];
  t_game const * game;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      if (!(game = conn_get_game(c)))
	{
	  message_send_text(c,message_type_error,c,"You are not in a game.");
	  return 0;
	}
    }
  else
    if (!(game = gamelist_find_game(&text[i],game_type_all)))
      {
	message_send_text(c,message_type_error,c,"That game does not exist.");
	return 0;
      }
  
  sprintf(msgtemp,"Name: %-20.20s    ID: "GAMEID_FORMAT" (%s)",game_get_name(game),game_get_id(game),strcmp(game_get_pass(game),"")==0?"public":"private");
  message_send_text(c,message_type_info,c,msgtemp);
  
  {
    t_account *  owner;
    char const * tname;
    char const * namestr;
    
    if (!(owner = conn_get_account(game_get_owner(game))))
      {
	tname = NULL;
	namestr = "none";
      }
    else
      if (!(tname = account_get_name(owner)))
	namestr = "unknown";
      else
	namestr = tname;
    
    sprintf(msgtemp,"Owner: %-20.20s",namestr);
    
    if (tname)
      account_unget_name(tname);
  }
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (!prefs_get_hide_addr() || account_get_auth_admin(conn_get_account(c))==1) /* default to false */
    {
      unsigned int   addr;
      unsigned short port;
      unsigned int   taddr;
      unsigned short tport;
      
      taddr=addr = game_get_addr(game);
      tport=port = game_get_port(game);
      gametrans_net(conn_get_addr(c),conn_get_port(c),conn_get_local_addr(c),conn_get_local_port(c),&taddr,&tport);
      
      if (taddr==addr && tport==port)
	sprintf(msgtemp,"Address: %s",
		addr_num_to_addr_str(addr,port));
      else
	sprintf(msgtemp,"Address: %s (trans %s)",
		addr_num_to_addr_str(addr,port),
		addr_num_to_addr_str(taddr,tport));
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  sprintf(msgtemp,"Client: %4s (version %s, startver %u)",game_get_clienttag(game),vernum_to_verstr(game_get_version(game)),game_get_startver(game));
  message_send_text(c,message_type_info,c,msgtemp);
  
  {
    time_t      gametime;
    struct tm * gmgametime;
    
    gametime = game_get_create_time(game);
    if (!(gmgametime = localtime(&gametime)))
      strcpy(msgtemp,"Created: ?");
    else
      strftime(msgtemp,sizeof(msgtemp),"Created: "GAME_TIME_FORMAT,gmgametime);
    message_send_text(c,message_type_info,c,msgtemp);
    
    gametime = game_get_start_time(game);
    if (gametime!=(time_t)0)
      {
	if (!(gmgametime = localtime(&gametime)))
	  strcpy(msgtemp,"Started: ?");
	else
	  strftime(msgtemp,sizeof(msgtemp),"Started: "GAME_TIME_FORMAT,gmgametime);
      }
    else
      strcpy(msgtemp,"Started: ");
    message_send_text(c,message_type_info,c,msgtemp);
  }
  
  sprintf(msgtemp,"Status: %s",game_status_get_str(game_get_status(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Type: %-20.20s",game_type_get_str(game_get_type(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Speed: %s",game_speed_get_str(game_get_speed(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Difficulty: %s",game_difficulty_get_str(game_get_difficulty(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Option: %s",game_option_get_str(game_get_option(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  {
    char const * mapname;
    
    if (!(mapname = game_get_mapname(game)))
      mapname = "unknown";
    sprintf(msgtemp,"Map: %-20.20s",mapname);
    message_send_text(c,message_type_info,c,msgtemp);
  }
  
  sprintf(msgtemp,"Map Size: %ux%u",game_get_mapsize_x(game),game_get_mapsize_y(game));
  message_send_text(c,message_type_info,c,msgtemp);
  sprintf(msgtemp,"Map Tileset: %s",game_tileset_get_str(game_get_tileset(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  sprintf(msgtemp,"Map Type: %s",game_maptype_get_str(game_get_maptype(game)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  sprintf(msgtemp,"Players: %u current, %u total, %u max",game_get_ref(game),game_get_count(game),game_get_maxplayers(game));
  message_send_text(c,message_type_info,c,msgtemp);
  
  {
    char const * description;
    
    if (!(description = game_get_description(game)))
      description = "";
    sprintf(msgtemp,"Description: %-20.20s",description);
  }
  
  return 0;
}

static int _handle_ladderactivate_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  ladderlist_make_all_active();
  message_send_text(c,message_type_info,c,"Copied current scores to active scores on all ladders.");
  return 0;
}

static int _handle_remove_accounting_infos_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  if (prefs_get_reduced_accounting()==0)
    {
      message_send_text(c,message_type_error,c,"Command Aborted. Server is currently in full accounting mode.");
      return 0;
    }
  // remove accounting here
  message_send_text(c,message_type_info,c,"starting to remove accounting infos... this may take a while...");
  accounts_remove_accounting_infos();
  message_send_text(c,message_type_info,c,"done removing accounting infos.. remember... next account saving may take a while...");
  return 0;
}

static int _handle_rehash_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  server_restart_wraper();  
  return 0;
}

static int _handle_rank_all_accounts_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  // rank all accounts here
  accounts_rank_all();  
  return 0;
}

static int _handle_reload_accounts_all_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  accountlist_reload(RELOAD_UPDATE_ALL);  
  return 0;
}

static int _handle_reload_accounts_new_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  accountlist_reload(RELOAD_ADD_ONLY_NEW);  
  return 0;
}

static int _handle_shutdown_command(t_connection * c, char const *text)
{
  char         dest[32];
  unsigned int i,j;
  unsigned int delay;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    delay = prefs_get_shutdown_delay();
  else
    if (clockstr_to_seconds(dest,&delay)<0)
      {
	message_send_text(c,message_type_error,c,"Invalid delay.");
	return 0;
      }
  
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  server_quit_delay(delay);
  
  if (delay)
    message_send_text(c,message_type_info,c,"You initialized the shutdown sequence.");
  else
    message_send_text(c,message_type_info,c,"You canceled the shutdown sequence.");
  
  return 0;
}

static int _handle_ladderinfo_command(t_connection * c, char const *text)
{
  char         dest[32];
  char         msgtemp[MAX_MESSAGE_LEN];
  unsigned int rank;
  unsigned int i,j;
  t_account *  account;
  char const * clienttag;
  char const * tname;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"Which rank do you want ladder info for?");
      return 0;
    }
  if (str_to_uint(dest,&rank)<0 || rank<1)
    {
      message_send_text(c,message_type_error,c,"Invalid rank.");
      return 0;
    }
  
  if (text[i]!='\0')
    clienttag = &text[i];
  else if (!(clienttag = conn_get_clienttag(c)))
    {
      message_send_text(c,message_type_error,c,"Unable to determine client game.");
      return 0;
    }
  
  if (strlen(clienttag)!=4)
    {
      sprintf(msgtemp,"You must supply a rank and a valid program ID. (Program ID \"%.32s\" is invalid.)",clienttag);
      message_send_text(c,message_type_error,c,msgtemp);
      message_send_text(c,message_type_error,c,"Example: /ladderinfo 1 STAR");
      return 0;
    }
  
  if (strcasecmp(clienttag,CLIENTTAG_STARCRAFT)==0)
    {
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_active,CLIENTTAG_STARCRAFT,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Starcraft active  %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_active_wins(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_active_losses(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_active_disconnects(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_active_rating(account,CLIENTTAG_STARCRAFT,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Starcraft active  %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_current,CLIENTTAG_STARCRAFT,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Starcraft current %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_wins(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_losses(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_disconnects(account,CLIENTTAG_STARCRAFT,ladder_id_normal),
		  account_get_ladder_rating(account,CLIENTTAG_STARCRAFT,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Starcraft current %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  else if (strcasecmp(clienttag,CLIENTTAG_BROODWARS)==0)
    {
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_active,CLIENTTAG_BROODWARS,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Brood War active  %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_active_wins(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_active_losses(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_active_disconnects(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_active_rating(account,CLIENTTAG_BROODWARS,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Brood War active  %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_current,CLIENTTAG_BROODWARS,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Brood War current %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_wins(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_losses(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_disconnects(account,CLIENTTAG_BROODWARS,ladder_id_normal),
		  account_get_ladder_rating(account,CLIENTTAG_BROODWARS,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Brood War current %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  else if (strcasecmp(clienttag,CLIENTTAG_WARCIIBNE)==0)
    {
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_active,CLIENTTAG_WARCIIBNE,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Warcraft II standard active  %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_active_wins(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_active_losses(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_active_disconnects(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_active_rating(account,CLIENTTAG_WARCIIBNE,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Warcraft II standard active  %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_active,CLIENTTAG_WARCIIBNE,ladder_id_ironman)))
	{
	  sprintf(msgtemp,"Warcraft II IronMan active   %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_active_wins(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_active_losses(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_active_disconnects(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_active_rating(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Warcraft II IronMan active   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_current,CLIENTTAG_WARCIIBNE,ladder_id_normal)))
	{
	  sprintf(msgtemp,"Warcraft II standard current %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_wins(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_losses(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_disconnects(account,CLIENTTAG_WARCIIBNE,ladder_id_normal),
		  account_get_ladder_rating(account,CLIENTTAG_WARCIIBNE,ladder_id_normal));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Warcraft II standard current %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_current,CLIENTTAG_WARCIIBNE,ladder_id_ironman)))
	{
	  sprintf(msgtemp,"Warcraft II IronMan current  %5u: %-20.20s %u/%u/%u rating %u",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ladder_wins(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_losses(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_disconnects(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman),
		  account_get_ladder_rating(account,CLIENTTAG_WARCIIBNE,ladder_id_ironman));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"Warcraft II IronMan current  %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  // --> aaron
  else if (strcasecmp(clienttag,CLIENTTAG_WARCRAFT3)==0)
    {
      unsigned int teamcount = 0;
      if ((account = war3_ladder_get_account(&solo_ladder,rank,teamcount)))
	{
	  sprintf(msgtemp,"WarCraft3 Solo   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_solowin(account),
		  account_get_sololoss(account));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 Solo   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(&team_ladder,rank,teamcount)))
	{
	  sprintf(msgtemp,"WarCraft3 Team   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_teamwin(account),
		  account_get_teamloss(account));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 Team   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(&ffa_ladder,rank,teamcount)))
	{
	  sprintf(msgtemp,"WarCraft3 FFA   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ffawin(account),
		  account_get_ffaloss(account));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 FFA   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(&at_ladder,rank,teamcount)))
	{
	  if (account_get_atteammembers(account,teamcount))
	    sprintf(msgtemp,"WarCraft3 AT Team   %5u: %-80.80s %u/%u/0",
		    rank,
		    account_get_atteammembers(account,teamcount),
		    account_get_atteamwin(account,teamcount),
		    account_get_atteamloss(account,teamcount));
	  else
	    sprintf(msgtemp,"WarCraft3 AT Team   %5u: <invalid team info>",rank);
	}
      else
	sprintf(msgtemp,"WarCraft3 AT Team  %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  //<---
  else
    {
      message_send_text(c,message_type_error,c,"This game does not support win/loss records.");
      message_send_text(c,message_type_error,c,"You must supply a rank and a valid program ID.");
      message_send_text(c,message_type_error,c,"Example: /ladderinfo 1 STAR");
    }
  
  return 0;
}

static int _handle_timer_command(t_connection * c, char const *text)
{
  unsigned int i,j;
  unsigned int delta;
  char         deltastr[64];
  t_timer_data data;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get comm */
    if (j<sizeof(deltastr)-1) deltastr[j++] = text[i];
  deltastr[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (deltastr[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"How long do you want the timer to last?");
      return 0;
    }
  
  if (clockstr_to_seconds(deltastr,&delta)<0)
    {
      message_send_text(c,message_type_error,c,"Invalid duration.");
      return 0;
    }
  
  if (text[i]=='\0')
    data.p = strdup("Your timer has expired.");
  else
    data.p = strdup(&text[i]);
  
  if (timerlist_add_timer(c,time(NULL)+(time_t)delta,user_timer_cb,data)<0)
    {
      eventlog(eventlog_level_error,"handle_command","could not add timer");
      free(data.p);
      message_send_text(c,message_type_error,c,"Could not set timer.");
    }
  else
    {
      char msgtemp[MAX_MESSAGE_LEN];
      
      sprintf(msgtemp,"Timer set for %s",seconds_to_timestr(delta));
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  return 0;
}

static int _handle_serverban_command(t_connection *c, char const *text)
{
  char dest[USER_NAME_MAX];
  char messagetemp[MAX_MESSAGE_LEN];
  char msg[MAX_MESSAGE_LEN];
  t_connection * dest_c;
  unsigned int i,j;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); // skip command 
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) // get dest
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if(account_get_auth_admin(conn_get_account(c))!=1)
    {
      message_send_text(c,message_type_error,c,"You do not have permissions to use this command.");
      return 0;
    }
  else
    {
      if (!(dest_c = connlist_find_connection_by_accountname(dest)))
	{
	  message_send_text(c,message_type_error,c,"That user is not logged on.");
	  return 0;
	}
      sprintf(messagetemp,"Banning User %s who is using IP %s",conn_get_username(dest_c),addr_num_to_ip_str(conn_get_game_addr(dest_c)));
      message_send_text(c,message_type_info,c,messagetemp);
      message_send_text(c,message_type_info,c,"Users Account is also LOCKED! Only a Admin can Unlock it!");
      sprintf(msg,"/ipban a %s",addr_num_to_ip_str(conn_get_game_addr(dest_c)));
      handle_ipban_command(c,msg);
      account_set_auth_lock(conn_get_account(dest_c),1);
      //now kill the connection
      sprintf(msg,"You have been banned by Admin: %s",conn_get_username(c));
      message_send_text(dest_c,message_type_error,dest_c,msg);
      message_send_text(dest_c,message_type_error,dest_c,"Your account is also LOCKED! Only a admin can UNLOCK it!");
      conn_destroy(dest_c);
      //now save the ipban file
      ipbanlist_save(prefs_get_ipbanfile());
      return 0;
    }
  
  return 0;
}

static int _handle_netinfo_command(t_connection * c, char const *text)
{
  char           dest[USER_NAME_MAX];
  char           msgtemp[MAX_MESSAGE_LEN];
  unsigned int   i,j;
  t_connection * conn;
  char const *   host;
  char const *   thost;
  t_game const * game;
  unsigned int   addr;
  unsigned short port;
  unsigned int   taddr;
  unsigned short tport;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); // skip command 
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) // get dest
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      char const * tname;
      
      strcpy(dest,(tname = conn_get_username(c)));
      conn_unget_username(c,tname);
    }
  
  if (!(conn = connlist_find_connection_by_accountname(dest)))
    {
      message_send_text(c,message_type_error,c,"That user is not logged on.");
      return 0;
    }
  
  if (conn_get_account(conn)!=conn_get_account(c) &&
      prefs_get_hide_addr() && account_get_auth_admin(conn_get_account(c))!=1) // default to false 
    {
      message_send_text(c,message_type_error,c,"Address information for other users is only available to admins.");
      return 0;
    }
  
  sprintf(msgtemp,"Server TCP: %s (bind %s)",addr_num_to_addr_str(conn_get_real_local_addr(conn),conn_get_real_local_port(conn)),addr_num_to_addr_str(conn_get_local_addr(conn),conn_get_local_port(conn)));
  message_send_text(c,message_type_info,c,msgtemp);
  
  if (!(host=thost = account_get_ll_host(conn_get_account(conn))))
    host = "unknown";
  sprintf(msgtemp,"Client TCP: %s (%.32s)",addr_num_to_addr_str(conn_get_addr(conn),conn_get_port(conn)),host);
  if (thost)
    account_unget_ll_host(thost);
  message_send_text(c,message_type_info,c,msgtemp);
  
  taddr=addr = conn_get_game_addr(conn);
  tport=port = conn_get_game_port(conn);
  gametrans_net(conn_get_addr(c),conn_get_port(c),addr,port,&taddr,&tport);
  
  if (taddr==addr && tport==port)
    sprintf(msgtemp,"Client UDP: %s",
	    addr_num_to_addr_str(addr,port));
  else
    sprintf(msgtemp,"Client UDP: %s (trans %s)",
	    addr_num_to_addr_str(addr,port),
	    addr_num_to_addr_str(taddr,tport));
  message_send_text(c,message_type_info,c,msgtemp);
  
  if ((game = conn_get_game(conn)))
    {
      taddr=addr = game_get_addr(game);
      tport=port = game_get_port(game);
      gametrans_net(conn_get_addr(c),conn_get_port(c),addr,port,&taddr,&tport);
      
      if (taddr==addr && tport==port)
	sprintf(msgtemp,"Game UDP:  %s",
		addr_num_to_addr_str(addr,port));
      else
	sprintf(msgtemp,"Game UDP:  %s (trans %s)",
		addr_num_to_addr_str(addr,port),
		addr_num_to_addr_str(taddr,tport));
    }
  else
    strcpy(msgtemp,"Game UDP:  none");
  message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_quota_command(t_connection * c, char const * text)
{
  char msgtemp[MAX_MESSAGE_LEN];
  
  sprintf(msgtemp,"Your quota allows you to write %u lines per %u seconds.",prefs_get_quota_lines(),prefs_get_quota_time());
  message_send_text(c,message_type_info,c,msgtemp);
  sprintf(msgtemp,"Long lines will be considered to wrap every %u characters.",prefs_get_quota_wrapline());
  message_send_text(c,message_type_info,c,msgtemp);
  sprintf(msgtemp,"You are not allowed to send lines with more than %u characters.",prefs_get_quota_maxline());
  message_send_text(c,message_type_info,c,msgtemp);
  
  return 0;
}

static int _handle_lockacct_command(t_connection * c, char const *text)
{
  unsigned int   i;
  t_connection * user;
  t_account *    account;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_error,c,"Which user do you want to lock?");
      return 0;
    }
#ifdef WITH_BITS
  if (bits_va_command_with_account_name(c,text,&text[i]))
    return 0;
#endif
  
  if (!(account = accountlist_find_account(&text[i])))
    {
      message_send_text(c,message_type_error,c,"Invalid user.");
      return 0;
    }
#ifdef WITH_BITS
  if (account_is_invalid(account)) {
    message_send_text(c,message_type_error,c,"Invalid user.");
    return 0;
  }
#endif
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  if ((user = connlist_find_connection_by_accountname(&text[i])))
    message_send_text(user,message_type_info,user,"Your account has just been locked by admin.");
  
  /* FIXME: this account attribute changing should be advertised on BITS right ?  (Yes.) */
  /* Since this wrapper function uses account_set_attr the attribute change should be advertised automatically on BITS ... */
  account_set_auth_lock(account,1);
  message_send_text(c,message_type_error,c,"That user account is now locked.");
  return 0;
}

static int _handle_unlockacct_command(t_connection * c, char const *text)
{
  unsigned int   i;
  t_connection * user;
  t_account *    account;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      message_send_text(c,message_type_error,c,"Which user do you want to unlock?");
      return 0;
    }
#ifdef WITH_BITS
  if (bits_va_command_with_account_name(c,text,&text[i]))
    return 0;
#endif
  if (!(account = accountlist_find_account(&text[i])))
    {
      message_send_text(c,message_type_error,c,"Invalid user.");
      return 0;
    }
#ifdef WITH_BITS
  if (account_is_invalid(account)) {
    message_send_text(c,message_type_error,c,"Invalid user.");
    return 0;
  }
#endif
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  if ((user = connlist_find_connection_by_accountname(&text[i])))
    message_send_text(user,message_type_info,user,"Your account has just been unlocked by admin.");
  
  /* FIXME: this account attribute changing should be advertised on BITS right ?*/
  
  account_set_auth_lock(account,0);
  message_send_text(c,message_type_error,c,"That user account is now unlocked.");
  return 0;
}     

static int _handle_flag_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  char         dest[32];
  unsigned int i,j;
  unsigned int newflag;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      message_send_text(c,message_type_error,c,"What flags do you want to set?");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1)
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  
  newflag = strtoul(dest,NULL,0);
  conn_set_flags(c,newflag);
  
  sprintf(msgtemp,"Flags set to 0x%08x.",newflag);
  message_send_text(c,message_type_info,c,msgtemp);
  return 0;
}

static int _handle_tag_command(t_connection * c, char const *text)
{
  char         msgtemp[MAX_MESSAGE_LEN];
  char         dest[8];
  unsigned int i,j;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (strlen(dest)!=4)
    {
      message_send_text(c,message_type_error,c,"Client tag should be four characters long.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c))!=1)
    {
      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
      return 0;
    }
  
  conn_set_clienttag(c,dest);
  sprintf(msgtemp,"Client tag set to %s.",dest);
  message_send_text(c,message_type_info,c,msgtemp);
  return 0;
}

static int _handle_bitsinfo_command(t_connection * c, char const *text)
{
#ifndef WITH_BITS
  message_send_text(c,message_type_info,c,"This PvPGN Server was compiled WITHOUT BITS support.");
#else
  char temp[MAX_MESSAGE_LEN];
  t_elem const * curr;
  int count;
  
  message_send_text(c,message_type_info,c,"This PvPGN Server was compiled WITH BITS support.");
  sprintf(temp,"Server address: 0x%04x",bits_get_myaddr());
  message_send_text(c,message_type_info,c,temp);
  if (bits_uplink_connection) {
    message_send_text(c,message_type_info,c,"Server type: client/slave");
  } else {
    message_send_text(c,message_type_info,c,"Server type: master");
  }
  message_send_text(c,message_type_info,c,"BITS routing table:");
  count = 0;
  LIST_TRAVERSE_CONST(bits_routing_table,curr) {
    t_bits_routing_table_entry *e = elem_get_data(curr);
    count++;
    
    sprintf(temp,"Route %d: [%d] -> 0x%04x",count,conn_get_socket(e->conn),e->bits_addr);
    message_send_text(c,message_type_info,c,temp);
  }
  if (bits_master) {
    message_send_text(c,message_type_info,c,"BITS host list:");
    LIST_TRAVERSE_CONST(bits_hostlist,curr) {
      t_bits_hostlist_entry *e = elem_get_data(curr);
      
      if (e->name)
	if (strlen(e->name)>128)
	  sprintf(temp,"[0x%04x] name=(too long)",e->address);
	else
	  sprintf(temp,"[0x%04x] name=\"%s\"",e->address,e->name);
      else {
	eventlog(eventlog_level_error,"handle_command","corruption in bits_hostlist detected");
	sprintf(temp,"[0x%04x] name=(null)",e->address);
      }
      message_send_text(c,message_type_info,c,temp);
    }
  }
#endif
  return 0;
}

static int _handle_set_command(t_connection * c, char const *text)
{
  t_account * account;
  char *accname;
  char *key;
  char *value;
  char t[MAX_MESSAGE_LEN];
  char msgtemp[MAX_MESSAGE_LEN];
  unsigned int i,j;
  char         arg1[256];
  char         arg2[256];
  char         arg3[256];
  
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"This command is only enabled for admins.");
      return 0;
    }
  
  strncpy(t, text, MAX_MESSAGE_LEN - 1);
  for (i=0; t[i]!=' ' && t[i]!='\0'; i++); /* skip command /set */
  
  for (; t[i]==' '; i++); /* skip spaces */
  for (j=0; t[i]!=' ' && t[i]!='\0'; i++) /* get username */
    if (j<sizeof(arg1)-1) arg1[j++] = t[i];
  arg1[j] = '\0';
  
  for (; t[i]==' '; i++); /* skip spaces */
  for (j=0; t[i]!=' ' && t[i]!='\0'; i++) /* get key */
    if (j<sizeof(arg2)-1) arg2[j++] = t[i];
  arg2[j] = '\0';
  
  for (; t[i]==' '; i++); /* skip spaces */
  for (j=0; t[i]!='\0'; i++) /* get value */
    if (j<sizeof(arg3)-1) arg3[j++] = t[i];
  arg3[j] = '\0';
  
  accname = arg1;
  key     = arg2;
  value   = arg3;
  
#ifdef WITH_BITS
  if (bits_va_command_with_account_name(c,text,accname))
    {
      return 0;
    }
#endif
  
  if (!(account = accountlist_find_account(accname)))
    {
      message_send_text(c,message_type_error,c,"Invalid user.");
      return 0;
    }
  
#ifdef WITH_BITS
  if (account_is_invalid(account))
    {
      message_send_text(c,message_type_error,c,"Invalid user.");
      return 0;
    }
#endif
  
  if (*key == '\0')
    {
      message_send_text(c,message_type_error,c,"Which key do you want to change?");
      message_send_text(c,message_type_error,c,"usage: /set key value");
      message_send_text(c,message_type_error,c,"e.g.: /set Record\\solo\\xp 0");
      return 0;
    }
  
  if (*value == '\0')
    {
      message_send_text(c,message_type_error,c,"What value do you want to set?");
      if (account_get_strattr(account,key))
	{
	  sprintf(msgtemp,"current value us \"%s\"",account_get_strattr(account,key));
	  message_send_text(c,message_type_error,c,msgtemp);
	}
      else
	message_send_text(c,message_type_error,c,"value currently not set");
      return 0;
    }
  
  if (account_set_strattr(account,key,value)<0)
    message_send_text(c,message_type_error,c,"Unable to set key");
  else
    message_send_text(c,message_type_error,c,"Key set succesfully");
  
  return 0;
}

static int _handle_motd_command(t_connection * c, char const *text)
{
  char const * filename;
  FILE *       fp;
  
  if ((filename = prefs_get_motdfile())) {
    if ((fp = fopen(filename,"r")))
      {
	message_send_file(c,fp);
	if (fclose(fp)<0)
	  eventlog(eventlog_level_error,"handle_command","could not close motd file \"%s\" after reading (fopen: %s)",filename,strerror(errno));
      }
    else
      {
	eventlog(eventlog_level_error,"handle_command","could not open motd file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	message_send_text(c,message_type_error,c,"Unable to open motd.");
      }
    return 0;
  } else {
    message_send_text(c,message_type_error,c,"No motd.");
    return 0;
  }
}

static int _handle_ping_command(t_connection * c, char const *text)
{
  unsigned int i;
  t_connection *	user;
  t_game 	*	game;
  char msgtemp[MAX_MESSAGE_LEN];
  char const *   tname;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0')
    {
      if ((game=conn_get_game(c)))
	{
	  for (i=0; i<game_get_count(game); i++)
	    {
	      if ((user = game_get_player_conn(game, i)))
		{
		  sprintf(msgtemp,"%s latency: %9u",(tname = conn_get_username(user)),conn_get_latency(user));
		  conn_unget_username(user,tname);
		  message_send_text(c,message_type_info,c,msgtemp);
		}
	    }
	  return 0;
	}
      sprintf(msgtemp,"Your latency %9u",conn_get_latency(c));
    }
  else if ((user = connlist_find_connection_by_accountname(&text[i])))
    sprintf(msgtemp,"%s latency %9u",&text[i],conn_get_latency(user));
  else
    sprintf(msgtemp,"Invalid user");
  
  message_send_text(c,message_type_info,c,msgtemp);
  return 0;
}

static int _handle_ipban_command(t_connection * c, char const *text)
{
  if (account_get_auth_admin(conn_get_account(c))!=1) /* default to false */
    {
      message_send_text(c,message_type_info,c,"You do not have permission to use this command.");
      return 0;
    }
  
  handle_ipban_command(c,text);
  return 0;
}
