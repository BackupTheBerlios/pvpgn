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
#include "command_groups.h"
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
	t_account * dest_a;
	t_bnettime btlogin;
	time_t ulogin;
	struct tm * tmlogin;

	if (!(dest_a = accountlist_find_account(dest))) {
	    message_send_text(c,message_type_error,c,"Unknown user.");
	    return;
	}

	if (conn_get_class(c) == conn_class_bnet) {
	    btlogin = time_to_bnettime((time_t)account_get_ll_time(dest_a),0);
	    btlogin = bnettime_add_tzbias(btlogin, conn_get_tzbias(c));
	    ulogin = bnettime_to_time(btlogin);
	    if (!(tmlogin = gmtime(&ulogin)))
		strcpy(temp, "User was last seen on ?");
	    else
		strftime(temp, sizeof(temp), "User was last seen on : %a %b %d %H:%M:%S",tmlogin);
	} else strcpy(temp, "User is offline");
	message_send_text(c, message_type_info, c, temp);
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

static int command_set_flags(t_connection * c); // [Omega]
// command handler prototypes
static int _handle_clan_command(t_connection * c, char const * text);
static int _handle_admin_command(t_connection * c, char const * text);
static int _handle_aop_command(t_connection * c, char const * text);
static int _handle_op_command(t_connection * c, char const * text);
static int _handle_deop_command(t_connection * c, char const * text);
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
//static int _handle_designate_command(t_connection * c, char const * text); Obsolete function [Omega]
//static int _handle_resign_command(t_connection * c, char const * text); Obsolete function [Omega]
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
//static int _handle_ipban_command(t_connection * c, char const * text); Redirected to handle_ipban_command() in ipban.c [Omega]
static int _handle_set_command(t_connection * c, char const * text);
static int _handle_motd_command(t_connection * c, char const * text);
static int _handle_ping_command(t_connection * c, char const * text);
static int _handle_commandgroups_command(t_connection * c, char const * text);

static const t_command_table_row standard_command_table[] = 
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
//	{ "/designate"          , _handle_designate_command }, Obsotele command [Omega]
//	{ "/resign"             , _handle_resign_command }, Obsolete command [Omega]
	{ "/kick"               , _handle_kick_command },
	{ "/ban"                , _handle_ban_command },
	{ "/unban"              , _handle_unban_command },


	{ NULL                  , NULL }
	
};

static const t_command_table_row extended_command_table[] =
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
	{ "/con"                , _handle_connections_command },
	{ "/finger"             , _handle_finger_command },
	{ "/operator"           , _handle_operator_command },
	{ "/aop"		, _handle_aop_command },
	{ "/op"           	, _handle_op_command },
	{ "/deop"           	, _handle_deop_command },
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
	{ "/ipban"              , handle_ipban_command }, // in ipban.c
	{ "/set"                , _handle_set_command },
	{ "/motd"               , _handle_motd_command },
	{ "/latency"            , _handle_ping_command },
	{ "/ping"               , _handle_ping_command },
	{ "/p"                  , _handle_ping_command },
	{ "/commandgroups"	, _handle_commandgroups_command },
	{ "/cg"			, _handle_commandgroups_command },
        { NULL                  , NULL } 

};

char const * skip_command(char const * org_text)
{
   unsigned int i;
   char * text = (char *)org_text;
   for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
   if (text[i]!='\0') text[i++]='\0';             /* \0-terminate command */
   for (; text[i]==' '; i++);  
   return &text[i];
}

extern int handle_command(t_connection * c,  char const * text)
{
  t_command_table_row const *p;

  for (p = standard_command_table; p->command_string != NULL; p++)
    {
      if (strstart(text, p->command_string)==0)
        {
	  if (!(command_get_group(p->command_string)))
	    {
	      message_send_text(c,message_type_error,c,"This command has been deactivated");
	      return 0;
	    }
	  if (!((command_get_group(p->command_string) & account_get_command_groups(conn_get_account(c)))))
	    {
	      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
	      return 0;
	    }
	if (p->command_handler != NULL) return ((p->command_handler)(c,text));
	}
    }

       
    if (prefs_get_extra_commands()==0)
    {
	message_send_text(c,message_type_error,c,"Unknown command.");
	eventlog(eventlog_level_debug,"handle_command","got unknown standard command \"%s\"",text);
	return 0;
    }
    
    for (p = extended_command_table; p->command_string != NULL; p++)
      {
      if (strstart(text, p->command_string)==0)
        {
	  if (!(command_get_group(p->command_string)))
	    {
	      message_send_text(c,message_type_error,c,"This command has been deactivated");
	      return 0;
	    }
	  if (!((command_get_group(p->command_string) & account_get_command_groups(conn_get_account(c)))))
	    {
	      message_send_text(c,message_type_error,c,"This command is reserved for admins.");
	      return 0;
	    }
	if (p->command_handler != NULL) return ((p->command_handler)(c,text));
	}
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
  char const      * oldclanname;
  char         	    clansendmessage[MAX_MESSAGE_LEN];
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"You are not in a channel. Join a channel to set a account password!");
      return 0;
    }
  
  text = skip_command(text);
  
  if ( text[0] == '\0' )
    {
      message_send_text(c,message_type_info,c,"usage: /clan <clanname>");
      message_send_text(c,message_type_info,c,"Using this option will allow you to join a clan which displays in your profile.  ");
      return 0;
    }

  if (strlen(text) > MAX_CLANNAME_LEN)
  {
    message_send_text(c,message_type_error,c,"the length of your clanname is limited to 64 characters.");
    return 0;
  }
  
  oldclanname = strdup(account_get_w3_clanname(conn_get_account(c)));
  account_set_w3_clanname(conn_get_account(c),text);
  if(!oldclanname || !(*oldclanname))
    {
      sprintf( clansendmessage, "Joined Clan %s", text );
    }
  else
    {
      sprintf( clansendmessage, "Left Clan %s and Joined Clan %s", oldclanname, text );
    }
  message_send_text(c,message_type_info,c,clansendmessage);
  free((void *)oldclanname);
  return 0;
}

static int command_set_flags(t_connection * c)
{
    unsigned int	currflags;
    unsigned int	newflags;
    char const *	channel;
    t_account  *	acc;
    
    currflags = conn_get_flags(c);
    acc = conn_get_account(c);
    
    if (!(channel = channel_get_name(conn_get_channel(c))))
	return -1;
    
    if (account_get_auth_admin(acc,channel) == 1 || account_get_auth_admin(acc,NULL) == 1)
	newflags = MF_BLIZZARD;
    else if (account_get_auth_operator(acc,channel) == 1 || 
	     account_get_auth_operator(acc,NULL) == 1 ||
	     channel_account_is_tmpOP(conn_get_channel(c),acc) )
        newflags = MF_BNET;
    else
        newflags = 0;
        
    if (conn_get_flags(c) != newflags) {
	conn_set_flags(c, newflags);
	channel_update_flags(c);
    }
    
    return 0;
}

static int _handle_admin_command(t_connection * c, char const * text)
{
    char		msg[MAX_MESSAGE_LEN];
    char const *	username;
    char		command;
    t_account *		acc;
    
    text = skip_command(text);
    
    if ((text[0]=='\0') || ((text[0] != '+') && (text[0] != '-'))) {
	message_send_text(c,message_type_info,c,"usage: /admin +username to promote user to Server Admin.");
	message_send_text(c,message_type_info,c,"       /admin -username to demote user from Aerver Admin.");
	return -1;
    }
    
    command = text[0];
    username = &text[1];
    
    if(!*username) {
	message_send_text(c,message_type_info,c,"You need to supply a username.");
      return -1;
    }
    
    if(!(acc = accountlist_find_account(username))) {
	sprintf(msg, "There's no account with username %.64s.", username);
	message_send_text(c, message_type_info, c, msg);
	return -1;
    }
    
    if (command == '+') {
	if (account_get_auth_admin(acc,NULL) == 1) {
	    sprintf(msg,"%s is allready a Server Admin",username);
	} else {
	    account_set_auth_admin(acc,NULL,1);
	    sprintf(msg,"%s has been promoted to a Server Admin",username);
	}
    } else {
	if (account_get_auth_admin(acc,NULL) != 1)
    	    sprintf(msg,"%s is no Server Admin, so you can't demote him",username);
	else {
	    account_set_auth_admin(acc,NULL,0);
	    sprintf(msg,"%s has been demoted from a Server Admin",username);
	}
    }
    
    message_send_text(c, message_type_info, c, msg);
    command_set_flags(connlist_find_connection_by_accountname(username));
    return 0;
}

static int _handle_operator_command(t_connection * c, char const * text)
{
    char		msg[MAX_MESSAGE_LEN];
    char const *	username;
    char		command;
    t_account *		acc;
    
    text = skip_command(text);
    
    if ((text[0]=='\0') || ((text[0] != '+') && (text[0] != '-'))) {
	message_send_text(c,message_type_info,c,"usage: /operator +username to promote user to Server Operator.");
	message_send_text(c,message_type_info,c,"       /operator -username to demote user from Server Operator.");
	return -1;
    }
    
    command = text[0];
    username = &text[1];
    
    if(!*username) {
	message_send_text(c,message_type_info,c,"You need to supply a username.");
      return -1;
    }
    
    if(!(acc = accountlist_find_account(username))) {
	sprintf(msg, "There's no account with username %.64s.", username);
	message_send_text(c, message_type_info, c, msg);
	return -1;
    }
    
    if (command == '+') {
	if (account_get_auth_operator(acc,NULL) == 1)
	    sprintf(msg,"%s is allready a Server Operator",username);
	else {
	    account_set_auth_operator(acc,NULL,1);
	    sprintf(msg,"%s has been promoted to a Server Operator",username);
	}
    } else {
	if (account_get_auth_operator(acc,NULL) != 1)
    	    sprintf(msg,"%s is no Server Operator, so you can't demote him",username);
	else {
	    account_set_auth_operator(acc,NULL,0);
	    sprintf(msg,"%s has been demoted from a Server Operator",username);
	}
    }
    
    message_send_text(c, message_type_info, c, msg);
    command_set_flags(connlist_find_connection_by_accountname(username));
    return 0;
}

static int _handle_aop_command(t_connection * c, char const * text)
{
    char		msg[MAX_MESSAGE_LEN];
    char const *	username;
    char const *	channel;
    t_account *		acc;
    
    if (!(channel = channel_get_name(conn_get_channel(c)))) {
	message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
	return -1;
    }
    
    if (account_get_auth_admin(conn_get_account(c),NULL)!=1 && account_get_auth_admin(conn_get_account(c),channel)!=1) {
	message_send_text(c,message_type_error,c,"You must be at least a Channel Admin to use this command.");
	return -1;
    }
    
    text = skip_command(text);
    
    if (!(username = &text[0])) {
	message_send_text(c, message_type_info, c, "You need to supply a username.");
	return -1;
    }
    
    if(!(acc = accountlist_find_account(username))) {
	sprintf(msg, "There's no account with username %.64s.", username);
	message_send_text(c, message_type_info, c, msg);
	return -1;
    }
    
    if (account_get_auth_admin(acc,channel) == 1)
	sprintf(msg,"%s is allready a Channel Admin",username);
    else {
	account_set_auth_admin(acc,channel,1);
	sprintf(msg,"%s has been promoted to a Channel Admin",username);
    }
    
    message_send_text(c, message_type_info, c, msg);
    command_set_flags(connlist_find_connection_by_accountname(username));
    return 0;
}

static int _handle_op_command(t_connection * c, char const * text)
{
    char		msg[MAX_MESSAGE_LEN];
    char const *	username;
    char const *	channel;
    t_account *		acc;
    int			OP_lvl;
    
    if (!(channel = channel_get_name(conn_get_channel(c)))) {
	message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
	return -1;
    }

    acc = conn_get_account(c);
    OP_lvl = 0;
    
    if (account_is_operator_or_admin(acc,channel))
      OP_lvl = 1;
    else if (channel_account_is_tmpOP(conn_get_channel(c),acc))
      OP_lvl = 2;

    if (OP_lvl==0)
    {
	message_send_text(c,message_type_error,c,"You must be at least a Channel Operator or tempOP to use this command.");
	return -1;
    }
    
    text = skip_command(text);
    
    if (!(username = &text[0])) {
	message_send_text(c, message_type_info, c, "You need to supply a username.");
	return -1;
    }
    
    if(!(acc = accountlist_find_account(username))) {
	sprintf(msg, "There's no account with username %.64s.", username);
	message_send_text(c, message_type_info, c, msg);
	return -1;
    }
    
    
    if (OP_lvl==1) // user is full op so he may fully op others
    {
      if (account_get_auth_operator(acc,channel) == 1)
	  sprintf(msg,"%s is allready a Channel Operator",username);
      else {
	  account_set_auth_operator(acc,channel,1);
	  sprintf(msg,"%s has been promoted to a Channel Operator",username);
      }
    }
    else { // user is only tempOP so he may only tempOP others
         account_set_tmpOP_channel(acc,(char *)channel);
	 sprintf(msg,"%s has been promoted to a tempOP",username);
    }
    
    message_send_text(c, message_type_info, c, msg);
    command_set_flags(connlist_find_connection_by_accountname(username));
    return 0;
}

static int _handle_deop_command(t_connection * c, char const * text)
{
    char		msg[MAX_MESSAGE_LEN];
    char const *	username;
    char const *	channel;
    t_account *		acc;
    int			OP_lvl;
    
    if (!(channel = channel_get_name(conn_get_channel(c)))) {
	message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
	return -1;
    }

    acc = conn_get_account(c);
    OP_lvl = 0;
    
    if (account_is_operator_or_admin(acc,channel))
      OP_lvl = 1;
    else if (channel_account_is_tmpOP(conn_get_channel(c),acc))
      OP_lvl = 2;

    if (OP_lvl==0)
    {
	message_send_text(c,message_type_error,c,"You must be at least a Channel Operator or tempOP to use this command.");
	return -1;
    }
    
    text = skip_command(text);
    
    if (!(username = &text[0])) {
	message_send_text(c, message_type_info, c, "You need to supply a username.");
	return -1;
    }
    
    if(!(acc = accountlist_find_account(username))) {
	sprintf(msg, "There's no account with username %.64s.", username);
	message_send_text(c, message_type_info, c, msg);
	return -1;
    }
  
    if (OP_lvl==1) // user is real OP and allowed to deOP
      {    
	if (account_get_auth_admin(acc,channel) == 1 || account_get_auth_operator(acc,channel) == 1) {
	  if (account_get_auth_admin(acc,channel) == 1) {
	    if (account_get_auth_admin(conn_get_account(c),channel)!=1 && account_get_auth_admin(conn_get_account(c),NULL)!=1)
	      message_send_text(c,message_type_info,c,"You must be at least a Channel Admin to demote another Channel Admin");
	    else {
	      account_set_auth_admin(acc,channel,0);
	      account_set_tmpOP_channel(acc,NULL);
	      sprintf(msg, "%s has been demoted from a Channel Admin.", username);
	      message_send_text(c, message_type_info, c, msg);
	    }
	  }
	  if (account_get_auth_operator(acc,channel) == 1) {
	    account_set_auth_operator(acc,channel,0);
	    account_set_tmpOP_channel(acc,NULL);
	    sprintf(msg,"%s has been demoted from a Channel Operator",username);
	    message_send_text(c, message_type_info, c, msg);
	  }
	} 
	else if (channel_account_is_tmpOP(conn_get_channel(c),acc))
	  {
	    account_set_tmpOP_channel(acc,NULL);
	    sprintf(msg,"%s has been demoted from a tempOP of this channel",username);
	    message_send_text(c, message_type_info, c, msg);
	  }
	else {
	  sprintf(msg,"%s is no Channel Admin or Channel Operator or tempOP, so you can't demote him.",username);
	  message_send_text(c, message_type_info, c, msg);
	}
      }
    else //user is just a tempOP and may only deOP other tempOPs
      {
	if (channel_account_is_tmpOP(conn_get_channel(c),acc))
	  {
	    account_set_tmpOP_channel(acc,NULL);
	    sprintf(msg,"%s has been demoted from a tempOP of this channel",username);
	    message_send_text(c, message_type_info, c, msg);
	  }
	else
	  {
	    sprintf(msg,"%s is no tempOP, so you can't demote him",username);
	    message_send_text(c, message_type_info, c, msg);
	  }
      }
    
    command_set_flags(connlist_find_connection_by_accountname(username));
    return 0;
}

static int _handle_friends_command(t_connection * c, char const * text)
{
  int i;
	
  text = skip_command(text);;
  
  if(text[0]=='\0' || strstart(text,"help")==0 || strstart(text, "h")==0) {
    message_send_text(c,message_type_info,c,"Friends List (Used in Arranged Teams and finding online friends.)");
    message_send_text(c,message_type_info,c,"Type: /f add <username> (adds a friend to your list)");
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
    t_account    * my_acc;
    t_account    * friend_acc;
    char tmp[7];
    
    text = skip_command(text);

    if (text[0] == '\0')
    {
	message_send_text(c,message_type_info,c,"usage: /f add <username>");
	return 0;
    }
    
    if (!(friend_acc = accountlist_find_account(text)))
      {
	message_send_text(c,message_type_info,c,"That user does not exist.");
	return 0;
      }
    
    // [quetzal] 20020822 - we DO care if we add UserName or username to friends list
    newfriend = account_get_name(friend_acc);
    
    if(!strcasecmp(newfriend, conn_get_username(c))) {
      message_send_text(c,message_type_info,c,"You can't add yourself to your friends list.");
      account_unget_name(newfriend);
      return 0;
    }
    
    n = account_get_friendcount(my_acc = conn_get_account(c));
    
    if(n >= MAX_FRIENDS) {
      sprintf(msgtemp, "You can only have a maximum of %d friends.", MAX_FRIENDS);
      message_send_text(c,message_type_info,c,msgtemp);
      account_unget_name(newfriend);
      return 0;
    }
    
    
    // check if the friend was already added
    
    for(i=0; i<n; i++)
      if(!strcasecmp(account_get_friend(my_acc, i), newfriend)) 
	{
	  sprintf(msgtemp, "%s is already on your friends list!", newfriend);
	  message_send_text(c,message_type_info,c,msgtemp);
	  account_unget_name(newfriend);
	  return 0;
	}
    
    account_set_friendcount(my_acc, n+1);
    account_set_friend(my_acc, n, newfriend);
    
    sprintf( msgtemp, "Added %s to your friends list.", newfriend);
    message_send_text(c,message_type_info,c,msgtemp);
    // 7/27/02 - THEUNDYING - Inform friend that you added them to your list
    friend_c = connlist_find_connection_by_accountname(newfriend);
    sprintf(msgtemp,"%s added you to his/her friends list.",conn_get_username(c));
    message_send_text(friend_c,message_type_info,friend_c,msgtemp);
    
    if (!(rpacket = packet_create(packet_class_bnet)))
    {
      account_unget_name(newfriend);
      return 0;
    }
    
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

    account_unget_name(newfriend);
    
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
      t_account * my_acc;
      int n = account_get_friendcount(my_acc = conn_get_account(c));
      char const *myusername;
      t_connection * dest_c;
      
      msg = skip_command(text);
      //if the message test is empty then ignore command
      if (msg[0]=='\0')
	{
	  message_send_text(c,message_type_info,c,"Did not message any friends. Type some text next time.");
	  return 0; 
	}
      
      myusername = conn_get_username(c);
      for(i=0; i<n; i++) 
	{ //Cycle through your friend list and whisper the ones that are online
	  friend = account_get_friend(my_acc,i);
	  dest_c = connlist_find_connection_by_accountname(friend);
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
    t_account * my_acc;
    t_packet * rpacket=NULL;
    
    text = skip_command(text);
    
    if (text[0]=='\0')
    {
	message_send_text(c,message_type_info,c,"usage: /f remove <username>");
	return 0;
    }
    
    // [quetzal] 20020822 - we DO care if we del UserName or username from friends list
    // [quetzal] 20020907 - dont do anything if oldfriend is NULL
    oldfriend = account_get_name(accountlist_find_account(text));
    if (oldfriend)
      {
	
	n = account_get_friendcount(my_acc = conn_get_account(c));
	
	for(i=0; i<n; i++)
	  if(!strcasecmp(account_get_friend(my_acc, i), oldfriend)) {
	    char num = (char)i;
	    if (i<n-1) account_set_friend(my_acc, i, account_get_friend(my_acc, n-1));
	    
	    account_set_friend(conn_get_account(c), n-1, "");
	    account_set_friendcount(conn_get_account(c), n-1);
	    
	    sprintf(msgtemp, "Removed %s from your friends list.", oldfriend);
	    message_send_text(c,message_type_info,c,msgtemp);

            account_unget_name(oldfriend);	    
	    
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
    
    sprintf(msgtemp, "%s was not found on your friends list.", text);
    message_send_text(c,message_type_info,c,msgtemp);
    return 0;
  }
  if (strstart(text,"list")==0 || strstart(text,"l")==0) {
    char const * friend;
    char status[128];
    char software[64];
    char msgtemp[MAX_MESSAGE_LEN];
    int n;
    char const *myusername;
    t_connection const * dest_c;
    t_game const * game;
    t_channel const * channel;
    t_account * my_acc;
    
    message_send_text(c,message_type_info,c,"Your PvPGN - Friends List");
    message_send_text(c,message_type_info,c,"=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
    
    n = account_get_friendcount(my_acc = conn_get_account(c));
    myusername = conn_get_username(c);
    
    for(i=0; i<n; i++) 
     {
        software[0]='\0';

	friend = account_get_friend(my_acc, i);
	
	if (!(dest_c = connlist_find_connection_by_accountname(friend)))
	  sprintf(status, ", offline");
	else
	  {
	    sprintf(software," using %s", conn_get_user_game_title(conn_get_clienttag(dest_c)));

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
	
	if (software[0]) sprintf(msgtemp, "%d: %.16s%.128s, %.64s", i+1, friend, status,software);
	else sprintf(msgtemp, "%d: %.16s%.128s", i+1, friend, status);
	message_send_text(c,message_type_info,c,msgtemp);
      }
    
    message_send_text(c,message_type_info,c,"=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
    message_send_text(c,message_type_info,c,"End of Friends List");
    
    return 0;
  } 
  return 0;
}

static int _handle_me_command(t_connection * c, char const * text)
{
  t_channel const * channel;
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"You are not in a channel.");
      return 0;
    }
  
  text = skip_command(text);

  if ((text[0]!='\0') && (!conn_quota_exceeded(c,text)))
    channel_message_send(channel,message_type_emote,c,text);
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
  
  if ((dest[0]=='\0') || (text[i]=='\0'))
    {
      message_send_text(c,message_type_info,c,"usage: /whisper <username> <text to whisper>");
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

  if (text[i]=='\0')
  {
	message_send_text(c,message_type_info,c,"usage: /who <channel>");
	return 0;
  }
  
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
  
  if (text[i]=='\0')
  {
    message_send_text(c,message_type_info,c,"usage: /whois <username>");
    return 0;
  }
  
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
  
  if (text[i]=='\0')
  {
	message_send_text(c,message_type_info,c,"usage: /announce <announcement>");
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

static int _handle_nobeep_command(t_connection * c, char const *text)
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
  else if (strcasecmp(clienttag,CLIENTTAG_WARCRAFT3)==0 || strcasecmp(clienttag,CLIENTTAG_WAR3XP)==0) // 7-31-02 THEUNDYING - Display stats for war3
    {
      sprintf(msgtemp,"%.64s's Ladder Record's:",(tname=account_get_name(account)));
      account_unget_name(tname);
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"Users Solo Level: %u, Experience: %u",
	      account_get_sololevel(account,clienttag),
	      account_get_soloxp(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"SOLO Ladder Record: %u-%u-0",
	      account_get_solowin(account,clienttag),
	      account_get_sololoss(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      // aaron -->
      sprintf(msgtemp,"SOLO Rank: %u",
	      account_get_solorank(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      // <---
      
      sprintf(msgtemp,"Users Team Level: %u, Experience: %u",
	      account_get_teamlevel(account,clienttag),
	      account_get_teamxp(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"TEAM Ladder Record: %u-%u-0",
	      account_get_teamwin(account,clienttag),
	      account_get_teamloss(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      // aaron -->
      sprintf(msgtemp,"TEAM Rank: %u",
	      account_get_teamrank(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      
      sprintf(msgtemp,"Users FFA Level: %u, Experience: %u",
	      account_get_ffalevel(account,clienttag),
	      account_get_ffaxp(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"FFA Ladder Record: %u-%u-0",
	      account_get_ffawin(account,clienttag),
	      account_get_ffaloss(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      sprintf(msgtemp,"FFA Rank: %u",
	      account_get_ffarank(account,clienttag));
      message_send_text(c,message_type_info,c,msgtemp);
      if (account_get_atteamcount(account,clienttag))
	{
	  int teamcount;
	  for (teamcount=1; teamcount<=account_get_atteamcount(account,clienttag); teamcount++)
	    {
	      sprintf(msgtemp,"Users AT Team No. %u",teamcount);
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"Users AT TEAM Level: %u, Experience: %u",
		      account_get_atteamlevel(account,teamcount,clienttag),
		      account_get_atteamxp(account,teamcount,clienttag));
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"AT TEAM Ladder Record: %u-%u-0",
		      account_get_atteamwin(account,teamcount,clienttag),
		      account_get_atteamloss(account,teamcount,clienttag));
	      message_send_text(c,message_type_info,c,msgtemp);
	      sprintf(msgtemp,"AT TEAM Rank: %u",
		      account_get_atteamrank(account,teamcount,clienttag));
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
       message_send_text(c,message_type_info,c,"usage /channel <channel>");
       return 0;
     }
   
   if(strcasecmp(&text[i],"Arranged Teams")==0)
     {
//       if(account_get_auth_admin(conn_get_account(c))>0)
//	 {
//	   message_send_text(c,message_type_error,c,"Please do not talk in channel Arranged Teams");
//	   message_send_text(c,message_type_error,c,"This channel is dedicated for the preparation of");
//	   message_send_text(c,message_type_error,c,"Arranged Team Games.");
//	 }
//       else
//	 {
	   message_send_text(c,message_type_error,c,"Channel Arranged Teams is a RESTRICTED Channel!");
	   return 0;
//	 }
     }
   
   if (conn_set_channel(c,&text[i])<0)
     conn_set_channel(c,CHANNEL_NAME_BANNED); /* should not fail */
   command_set_flags(c);
   
   return 0;
 }

static int _handle_rejoin_command(t_connection * c, char const *text)
{

  if (channel_rejoin(c)!=0)
      message_send_text(c,message_type_error,c,"You are not in a channel.");
  command_set_flags(c);
  
  return 0;
}

static int _handle_away_command(t_connection * c, char const *text)
{
  unsigned int i;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  
  if (text[i]=='\0') /* toggle away mode */
    {
      if (!conn_get_awaystr(c))
      {
	message_send_text(c,message_type_info,c,"You are now marked as being away.");
	conn_set_awaystr(c,"Currently not available");	
      }
      else
      {
        message_send_text(c,message_type_info,c,"You are no longer marked as away.");
        conn_set_awaystr(c,NULL);
      }
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
  
  if (text[i]=='\0') /* toggle dnd mode */
    {
      if (!conn_get_dndstr(c))
      {
	message_send_text(c,message_type_info,c,"Do Not Diturb mode engaged.");
	conn_set_dndstr(c,"Not available");
      }
      else
      {
        message_send_text(c,message_type_info,c,"Do Not Disturb mode cancelled.");
        conn_set_dndstr(c,NULL);
      }
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
      message_send_text(c,message_type_info,c,"usage: /squelch <username>");
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
      message_send_text(c,message_type_info,c,"usage: /unsquelch <username>");
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

static int _handle_kick_command(t_connection * c, char const *text)
{
  char              msgtemp[MAX_MESSAGE_LEN];
  char              dest[USER_NAME_MAX];
  unsigned int      i,j;
  t_channel const * channel;
  t_connection *    kuc;
  t_account *	    acc;
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get dest */
    if (j<sizeof(dest)-1) dest[j++] = text[i];
  dest[j] = '\0';
  for (; text[i]==' '; i++);
  
  if (dest[0]=='\0')
    {
      message_send_text(c,message_type_info,c,"usage: /kick <username>");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }

  acc = conn_get_account(c);
  if (account_get_auth_admin(acc,NULL)!=1 && /* default to false */
      account_get_auth_admin(acc,channel_get_name(channel))!=1 && /* default to false */
      account_get_auth_operator(acc,NULL)!=1 && /* default to false */
      account_get_auth_operator(acc,channel_get_name(channel))!=1 && /* default to false */
      !channel_account_is_tmpOP(channel,acc))
    {
      message_send_text(c,message_type_error,c,"You have to be at least a Channel Operator or tempOP to use this command.");
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
  if (account_get_auth_admin(conn_get_account(kuc),NULL)==1 ||
    account_get_auth_admin(conn_get_account(kuc),channel_get_name(channel))==1)
    {
      message_send_text(c,message_type_error,c,"You cannot kick administrators.");
      return 0;
    }
  else if (account_get_auth_operator(conn_get_account(kuc),NULL)==1 ||
    account_get_auth_operator(conn_get_account(kuc),channel_get_name(channel))==1)
    {
      message_send_text(c,message_type_error,c,"You cannot kick operators.");
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
      message_send_text(c,message_type_info,c,"usage. /ban <username>");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c),NULL)!=1 && /* default to false */
      account_get_auth_admin(conn_get_account(c),channel_get_name(channel))!=1 && /* default to false */
      account_get_auth_operator(conn_get_account(c),NULL)!=1 && /* default to false */
      account_get_auth_operator(conn_get_account(c),channel_get_name(channel))!=1) /* default to false */
    {
      message_send_text(c,message_type_error,c,"You have to be at least a Channel Operator to use this command.");
      return 0;
    }
  {
    t_account * account;
    
    if (!(account = accountlist_find_account(dest)))
      message_send_text(c,message_type_info,c,"That account doesn't currently exist, banning anyway.");
    else if (account_get_auth_admin(account,NULL)==1 || account_get_auth_admin(account,channel_get_name(channel))==1)
      {
        message_send_text(c,message_type_error,c,"You cannot ban administrators.");
        return 0;
      }
    else if (account_get_auth_operator(account,NULL)==1 ||
	account_get_auth_operator(account,channel_get_name(channel))==1)
      {
        message_send_text(c,message_type_error,c,"You cannot ban operators.");
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
      message_send_text(c,message_type_info,c,"usage: /unban <username>");
      return 0;
    }
  
  if (!(channel = conn_get_channel(c)))
    {
      message_send_text(c,message_type_error,c,"This command can only be used inside a channel.");
      return 0;
    }
  if (account_get_auth_admin(conn_get_account(c),NULL)!=1 && /* default to false */
      account_get_auth_admin(conn_get_account(c),channel_get_name(channel))!=1 && /* default to false */
      account_get_auth_operator(conn_get_account(c),NULL)!=1 && /* default to false */
      account_get_auth_operator(conn_get_account(c),channel_get_name(channel))!=1) /* default to false */
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
      message_send_text(c,message_type_info,c,"usage: /reply <replytext>");
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
  
  if (!(realmname=conn_get_realmname(c))) {
    message_send_text(c,message_type_info,c,"You must join a realm first");
  }

  if (text[i]=='\0')
  {
    message_send_text(c,message_type_info,c,"usage: /realmann <announcement text>");
    return 0;
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
      message_send_text(c,message_type_info,c,"usage: /watch <username>");
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
       message_send_text(c,message_type_info,c,"usage: /unwatch <username>");
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
      sprintf(msgtemp,"Current games of type %s",tag);
      message_send_text(c,message_type_info,c,msgtemp);
    }
  
  if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
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
	  if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
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
      sprintf(msgtemp,"Current channels of type %s",tag);
      message_send_text(c,message_type_info,c,msgtemp);
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
	 /* FIXME: display opers correct again (aaron)
	  if ((opr = channel_get_operator(channel)))
	    oprname = conn_get_username(opr);
	  else
	 */
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
      message_send_text(c,message_type_info,c,"usage: /addacct <username> <password>");
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
      message_send_text(c,message_type_info,c,"usage: /chpass [username] <password>");
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
      (temp!=account && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-chpass")))) /* default to false */
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
  
  if (!prefs_get_enable_conn_all() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-con"))) /* default to false */
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
	if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr")))
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
	if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
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

  if (dest[0]=='\0')
  {
    message_send_text(c,message_type_info,c,"usage: /finger <account>");
    return 0;
  }
  
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
      !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
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

/*
 * rewrote command /operator to add and remove operator status [Omega]
 *
 * Fixme: rewrite /operators to show Currently logged on Server and/or Channel operators ...??
 */
/*
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
*/

/* FIXME: do we want to show just Server Admin or Channel Admin Also? [Omega] */
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
      if (account_get_auth_admin(conn_get_account(tc),NULL)==1)
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
  char		usrnick[USER_NAME_MAX]; /* max length of nick + \0 */  /* FIXME: Is it somewhere defined? */
  
  for (i=0; text[i]!=' ' && text[i]!='\0'; i++); /* skip command */
  for (; text[i]==' '; i++);
  for (j=0; text[i]!=' ' && text[i]!='\0'; i++) /* get nick */
    if (j<sizeof(usrnick)-1) usrnick[j++] = text[i];
  usrnick[j]='\0';
  for (; text[i]==' '; i++);
  
  if (usrnick[0]=='\0')
    {
      message_send_text(c,message_type_info,c,"usage: /kill <username>");
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
  
  if (session[0]=='\0')
    {
      message_send_text(c,message_type_info,c,"usage: /killsession <session>");
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
  
  if (!prefs_get_hide_addr() || (account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
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
  ladderlist_make_all_active();
  message_send_text(c,message_type_info,c,"Copied current scores to active scores on all ladders.");
  return 0;
}

static int _handle_remove_accounting_infos_command(t_connection * c, char const *text)
{
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
  server_restart_wraper();  
  return 0;
}

static int _handle_rank_all_accounts_command(t_connection * c, char const *text)
{
  // rank all accounts here
  accounts_rank_all();
  return 0;
}

static int _handle_reload_accounts_all_command(t_connection * c, char const *text)
{
  message_send_text(c,message_type_info,c,"start reloading all accounts (Warning: still EXPERIMENTAL");
  accountlist_reload(RELOAD_UPDATE_ALL);  
  message_send_text(c,message_type_info,c,"done reloading all accounts");
  return 0;
}

static int _handle_reload_accounts_new_command(t_connection * c, char const *text)
{
  message_send_text(c,message_type_info,c,"searching for new accounts to add");
  accountlist_reload(RELOAD_ADD_ONLY_NEW);  
  message_send_text(c,message_type_info,c,"done loading new accounts");
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
      message_send_text(c,message_type_info,c,"usage: /ladderinfo <rank> [clienttag]");
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
  else if (strcasecmp(clienttag,CLIENTTAG_WARCRAFT3)==0 || strcasecmp(clienttag,CLIENTTAG_WAR3XP)==0)
    {
      unsigned int teamcount = 0;
      if ((account = war3_ladder_get_account(solo_ladder(clienttag),rank,&teamcount,clienttag)))
	{
	  sprintf(msgtemp,"WarCraft3 Solo   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_solowin(account,clienttag),
		  account_get_sololoss(account,clienttag));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 Solo   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(team_ladder(clienttag),rank,&teamcount,clienttag)))
	{
	  sprintf(msgtemp,"WarCraft3 Team   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_teamwin(account,clienttag),
		  account_get_teamloss(account,clienttag));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 Team   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(ffa_ladder(clienttag),rank,&teamcount,clienttag)))
	{
	  sprintf(msgtemp,"WarCraft3 FFA   %5u: %-20.20s %u/%u/0",
		  rank,
		  (tname = account_get_name(account)),
		  account_get_ffawin(account,clienttag),
		  account_get_ffaloss(account,clienttag));
	  account_unget_name(tname);
	}
      else
	sprintf(msgtemp,"WarCraft3 FFA   %5u: <none>",rank);
      message_send_text(c,message_type_info,c,msgtemp);
      
      if ((account = war3_ladder_get_account(at_ladder(clienttag),rank,&teamcount,clienttag)))
	{
	  if (account_get_atteammembers(account,teamcount,clienttag))
	    sprintf(msgtemp,"WarCraft3 AT Team   %5u: %-80.80s %u/%u/0",
		    rank,
		    account_get_atteammembers(account,teamcount,clienttag),
		    account_get_atteamwin(account,teamcount,clienttag),
		    account_get_atteamloss(account,teamcount,clienttag));
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
      message_send_text(c,message_type_info,c,"usage: /timer <duration>");
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
  
      if (dest[0]=='\0')
      {
	message_send_text(c,message_type_info,c,"usage: /serverban <account>");
	return 0;
      }
	    
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
      prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) // default to false 
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
      message_send_text(c,message_type_info,c,"usage: /lockacct <username>");
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
      message_send_text(c,message_type_info,c,"usage: /unlockacct <username>");
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
      message_send_text(c,message_type_info,c,"usage: /flag <flag>");
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
 
  if (dest[0]=='\0')
  {
	message_send_text(c,message_type_info,c,"usage: /tag <clienttag>");
	return 0;
  }
  if (strlen(dest)!=4)
    {
      message_send_text(c,message_type_error,c,"Client tag should be four characters long.");
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

  if ((arg1[0] =='\0') || (arg2[0]=='\0'))
  {
	message_send_text(c,message_type_info,c,"usage: /set <username> <key> [value]");
  }
  
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
  
  if (*value == '\0')
    {
      if (account_get_strattr(account,key))
	{
	  sprintf(msgtemp, "current value of %.64s is \"%.128s\"",key,account_get_strattr(account,key));
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

/* Redirected to handle_ipban_command in ipban.c [Omega]
static int _handle_ipban_command(t_connection * c, char const *text)
{
  handle_ipban_command(c,text);
  return 0;
}
*/

static int _handle_commandgroups_command(t_connection * c, char const * text)
{
    t_account *	account;
    char *	command;
    char *	username;
    unsigned int usergroups;	// from user account
    unsigned int groups = 0;	// converted from arg3
    char	tempgroups[8];	// converted from usergroups
    char 	t[MAX_MESSAGE_LEN];
    char 	msgtemp[MAX_MESSAGE_LEN];
    unsigned int i,j;
    char	arg1[256];
    char	arg2[256];
    char	arg3[256];
  
    strncpy(t, text, MAX_MESSAGE_LEN - 1);
    for (i=0; t[i]!=' ' && t[i]!='\0'; i++); /* skip command /groups */
    
    for (; t[i]==' '; i++); /* skip spaces */
    for (j=0; t[i]!=' ' && t[i]!='\0'; i++) /* get command */
	if (j<sizeof(arg1)-1) arg1[j++] = t[i];
    arg1[j] = '\0';
  
    for (; t[i]==' '; i++); /* skip spaces */
    for (j=0; t[i]!=' ' && t[i]!='\0'; i++) /* get username */
	if (j<sizeof(arg2)-1) arg2[j++] = t[i];
    arg2[j] = '\0';
  
    for (; t[i]==' '; i++); /* skip spaces */
    for (j=0; t[i]!='\0'; i++) /* get groups */
	if (j<sizeof(arg3)-1) arg3[j++] = t[i];
    arg3[j] = '\0';
  
    command = arg1;
    username = arg2;

    if (arg1[0] =='\0') {
	message_send_text(c,message_type_info,c,"usage: /cg <command> <username> [<group(s)>]");
	return 0;
    }
  
    if (!strcmp(command,"help") || !strcmp(command,"h")) {
	message_send_text(c,message_type_info,c,"Command Groups (Defines the Groups of Commands a User Can Use.)");
	message_send_text(c,message_type_info,c,"Type: /cg add <username> <group(s)> - adds group(s) to user profile");
	message_send_text(c,message_type_info,c,"Type: /cg del <username> <group(s)> - deletes group(s) from user profile");
	message_send_text(c,message_type_info,c,"Type: /cg list <username> - shows current groups user can use");
	return 0;
    }
    
    if (arg2[0] =='\0') {
	message_send_text(c,message_type_info,c,"usage: /cg <command> <username> [<group(s)>]");
	return 0;
    }

    if (!(account = accountlist_find_account(username))) {
	message_send_text(c,message_type_error,c,"Invalid user.");
	return 0;
    }
    
    usergroups = account_get_command_groups(account);

    if (!strcmp(command,"list") || !strcmp(command,"l")) {
	if (usergroups & 1) tempgroups[0] = '1'; else tempgroups[0] = ' ';
	if (usergroups & 2) tempgroups[1] = '2'; else tempgroups[1] = ' ';
	if (usergroups & 4) tempgroups[2] = '3'; else tempgroups[2] = ' ';
	if (usergroups & 8) tempgroups[3] = '4'; else tempgroups[3] = ' ';
	if (usergroups & 16) tempgroups[4] = '5'; else tempgroups[4] = ' ';
	if (usergroups & 32) tempgroups[5] = '6'; else tempgroups[5] = ' ';
	if (usergroups & 64) tempgroups[6] = '7'; else tempgroups[6] = ' ';
	if (usergroups & 128) tempgroups[7] = '8'; else tempgroups[7] = ' ';
	sprintf(msgtemp, "%s's command group(s): %s", username, tempgroups);
	message_send_text(c,message_type_info,c,msgtemp);
	return 0;
    }
    
    if (arg3[0] =='\0') {
	message_send_text(c,message_type_info,c,"usage: /cg <command> <username> [<group(s)>]");
	return 0;
    }
	
    for (i=0; arg3[i] != '\0'; i++) {
	if (arg3[i] == '1') groups |= 1;
	else if (arg3[i] == '2') groups |= 2;
	else if (arg3[i] == '3') groups |= 4;
	else if (arg3[i] == '4') groups |= 8;
	else if (arg3[i] == '5') groups |= 16;
	else if (arg3[i] == '6') groups |= 32;
	else if (arg3[i] == '7') groups |= 64;
	else if (arg3[i] == '8') groups |= 128;
	else {
	    sprintf(msgtemp, "got bad group: %c", arg3[i]);
	    message_send_text(c,message_type_info,c,msgtemp);
	    return 0;
	}
    }

    if (!strcmp(command,"add") || !strcmp(command,"a")) {
	account_set_command_groups(account, usergroups | groups);
	sprintf(msgtemp, "groups %s has been added to user: %s", arg3, username);
	message_send_text(c,message_type_info,c,msgtemp);
	return 0;
    }
    
    if (!strcmp(command,"del") || !strcmp(command,"d")) {
	account_set_command_groups(account, usergroups & (255 - groups));
	sprintf(msgtemp, "groups %s has been deleted from user: %s", arg3, username);
	message_send_text(c,message_type_info,c,msgtemp);
	return 0;
    }
    
    sprintf(msgtemp, "got unknown command: %s", command);
    message_send_text(c,message_type_info,c,msgtemp);
    return 0;
}
