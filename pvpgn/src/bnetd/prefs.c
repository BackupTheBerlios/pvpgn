/*
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 1999  Mark Baysinger (mbaysing@ucsd.edu)
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
#define PREFS_INTERNAL_ACCESS
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
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include <errno.h>
#include "compat/strerror.h"
#include <ctype.h>
#include "common/util.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/setup_after.h"

static int processDirective(char const * directive, char const * value, unsigned int curLine);

#define NONE 0

static struct {
    /* files and paths */
    char const * filedir;
    char const * storage_path;
    char const * logfile;
    char const * loglevels;
    char const * motdfile;
    char const * newsfile;
    char const * channelfile;
    char const * pidfile;
    char const * adfile;
    char const * topicfile;
    char const * DBlayoutfile;

    unsigned int usersync;
    unsigned int userflush;
    unsigned int userstep;

    char const * servername;

    unsigned int track;
    char const * location;
    char const * description;
    char const * url;
    char const * contact_name;
    char const * contact_email;
    unsigned int latency;
    unsigned int irc_latency;
    unsigned int shutdown_delay;
    unsigned int shutdown_decr;
    unsigned int new_accounts;
    unsigned int max_accounts;
    unsigned int kick_old_login;
    unsigned int ask_new_channel;
    unsigned int hide_pass_games;
    unsigned int hide_started_games;
    unsigned int hide_temp_channels;
    unsigned int hide_addr;
    unsigned int enable_conn_all;
    unsigned int extra_commands;
    char const * reportdir;
    unsigned int report_all_games;
    unsigned int report_diablo_games;
    char const * iconfile;
    char const * war3_iconfile;
    char const * star_iconfile;
    char const * tosfile;
    char const * mpqfile;
    char const * trackaddrs;
    char const * servaddrs;
    char const * w3routeaddr;
    char const * ircaddrs;
    unsigned int use_keepalive;
    unsigned int udptest_port;
    char const * ipbanfile;
    unsigned int disc_is_loss;
    char const * helpfile;
    char const * fortunecmd;
    char const * transfile;
    unsigned int chanlog;
    char const * chanlogdir;
    unsigned int quota;
    unsigned int quota_lines;
    unsigned int quota_time;
    unsigned int quota_wrapline;
    unsigned int quota_maxline;
    unsigned int ladder_init_rating;
    unsigned int quota_dobae;
    char const * realmfile;
    char const * issuefile;
    char const * effective_user;
    char const * effective_group;
    unsigned int nullmsg;
    unsigned int mail_support;
    unsigned int mail_quota;
    char const * maildir;
    char const * log_notice;
    unsigned int savebyname;
    unsigned int skip_versioncheck;
    unsigned int allow_bad_version;
    unsigned int allow_unknown_version;
    char const * versioncheck_file;
    unsigned int d2cs_version;
    unsigned int allow_d2cs_setname;
    unsigned int hashtable_size;
    char const * telnetaddrs;
    unsigned int ipban_check_int;
    char const * version_exeinfo_match;
    unsigned int version_exeinfo_maxdiff;
    unsigned int max_concurrent_logins;
    unsigned int identify_timeout_secs;
    char const * server_info;
    char const * mapsfile;
    char const * xplevelfile;
    char const * xpcalcfile;
    unsigned int initkill_timer;
    unsigned int war3_ladder_update_secs;
    unsigned int output_update_secs;
    char const * ladderdir;
    char const * statusdir;
    unsigned int XML_output_ladder;
    unsigned int XML_status_output;
    char const * account_allowed_symbols;
    char const * command_groups_file;
    char const * tournament_file;
    char const * aliasfile;
    char const * anongame_infos_file;
    char const * w3trans_file;
    unsigned int max_conns_per_IP;
    unsigned int max_friends;
    unsigned int clan_newer_time;
    unsigned int clan_max_members;
    unsigned int clan_channel_default_private;
    unsigned int passfail_count;
    unsigned int passfail_bantime;
    unsigned int maxusers_per_channel;
    unsigned int load_new_account;
    char const * supportfile;
} prefs_runtime_config;

/*    directive                 type               defcharval            defintval                 */
static Bconf_t conf_table[] =
{
    { "filedir",                conf_type_char,    BNETD_FILE_DIR,       NONE                , (void *)&prefs_runtime_config.filedir},
    { "storage_path",           conf_type_char,    BNETD_STORAGE_PATH,   NONE                , (void *)&prefs_runtime_config.storage_path},
    { "logfile",                conf_type_char,    BNETD_LOG_FILE,       NONE                , (void *)&prefs_runtime_config.logfile},
    { "loglevels",              conf_type_char,    BNETD_LOG_LEVELS,     NONE                , (void *)&prefs_runtime_config.loglevels},
    { "motdfile",               conf_type_char,    BNETD_MOTD_FILE,      NONE                , (void *)&prefs_runtime_config.motdfile},
    { "newsfile",               conf_type_char,    BNETD_NEWS_DIR,       NONE                , (void *)&prefs_runtime_config.newsfile},
    { "channelfile",            conf_type_char,    BNETD_CHANNEL_FILE,   NONE                , (void *)&prefs_runtime_config.channelfile},
    { "pidfile",                conf_type_char,    BNETD_PID_FILE,       NONE                , (void *)&prefs_runtime_config.pidfile},
    { "adfile",                 conf_type_char,    BNETD_AD_FILE,        NONE                , (void *)&prefs_runtime_config.adfile},
    { "topicfile",		conf_type_char,	   BNETD_TOPIC_FILE,	 NONE		     , (void *)&prefs_runtime_config.topicfile},
    { "DBlayoutfile",		conf_type_char,    BNETD_DBLAYOUT_FILE,  NONE		     , (void *)&prefs_runtime_config.DBlayoutfile},
    { "supportfile",		conf_type_char,    BNETD_SUPPORT_FILE,   NONE                , (void *)&prefs_runtime_config.supportfile},
    { "usersync",               conf_type_int,     NULL,                 BNETD_USERSYNC      , (void *)&prefs_runtime_config.usersync},
    { "userflush",              conf_type_int,     NULL,                 BNETD_USERFLUSH     , (void *)&prefs_runtime_config.userflush},
    { "userstep",               conf_type_int,     NULL,                 BNETD_USERSTEP      , (void *)&prefs_runtime_config.userstep},
    { "servername",             conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.servername},
    { "track",                  conf_type_int,     NULL,                 BNETD_TRACK_TIME    , (void *)&prefs_runtime_config.track},
    { "location",               conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.location},
    { "description",            conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.description},
    { "url",                    conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.url},
    { "contact_name",           conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.contact_name},
    { "contact_email",          conf_type_char,    "",                   NONE                , (void *)&prefs_runtime_config.contact_email},
    { "latency",                conf_type_int,     NULL,                 BNETD_LATENCY       , (void *)&prefs_runtime_config.latency},
    { "irc_latency",            conf_type_int,     NULL,                 BNETD_IRC_LATENCY   , (void *)&prefs_runtime_config.irc_latency},
    { "shutdown_delay",         conf_type_int,     NULL,                 BNETD_SHUTDELAY     , (void *)&prefs_runtime_config.shutdown_delay},
    { "shutdown_decr",          conf_type_int,     NULL,                 BNETD_SHUTDECR      , (void *)&prefs_runtime_config.shutdown_decr},
    { "new_accounts",           conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.new_accounts},
    { "max_accounts",           conf_type_int,     NULL,                 0                   , (void *)&prefs_runtime_config.max_accounts},
    { "kick_old_login",         conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.kick_old_login},
    { "ask_new_channel",        conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.ask_new_channel},
    { "hide_pass_games",        conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.hide_pass_games},
    { "hide_started_games",     conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.hide_started_games},
    { "hide_temp_channels",     conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.hide_temp_channels},
    { "hide_addr",              conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.hide_addr},
    { "enable_conn_all",        conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.enable_conn_all},
    { "extra_commands",         conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.extra_commands},
    { "reportdir",              conf_type_char,    BNETD_REPORT_DIR,     NONE                , (void *)&prefs_runtime_config.reportdir},
    { "report_all_games",       conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.report_all_games},
    { "report_diablo_games",    conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.report_diablo_games},
    { "iconfile",               conf_type_char,    BNETD_ICON_FILE,      NONE                , (void *)&prefs_runtime_config.iconfile},
    { "war3_iconfile",          conf_type_char,    BNETD_WAR3_ICON_FILE, NONE                , (void *)&prefs_runtime_config.war3_iconfile},
    { "star_iconfile",          conf_type_char,    BNETD_STAR_ICON_FILE, NONE                , (void *)&prefs_runtime_config.star_iconfile},
    { "tosfile",                conf_type_char,    BNETD_TOS_FILE,       NONE                , (void *)&prefs_runtime_config.tosfile},
    { "mpqfile",                conf_type_char,    BNETD_MPQ_FILE,       NONE                , (void *)&prefs_runtime_config.mpqfile},
    { "trackaddrs",             conf_type_char,    BNETD_TRACK_ADDRS,    NONE                , (void *)&prefs_runtime_config.trackaddrs},
    { "servaddrs",              conf_type_char,    BNETD_SERV_ADDRS,     NONE                , (void *)&prefs_runtime_config.servaddrs},
    { "w3routeaddr",            conf_type_char,    BNETD_W3ROUTE_ADDR,   NONE                , (void *)&prefs_runtime_config.w3routeaddr},
    { "ircaddrs",               conf_type_char,    BNETD_IRC_ADDRS,      NONE                , (void *)&prefs_runtime_config.ircaddrs},
    { "use_keepalive",          conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.use_keepalive},
    { "udptest_port",           conf_type_int,     NULL,                 BNETD_DEF_TEST_PORT , (void *)&prefs_runtime_config.udptest_port},
    { "ipbanfile",              conf_type_char,    BNETD_IPBAN_FILE,     NONE                , (void *)&prefs_runtime_config.ipbanfile},
    { "disc_is_loss",           conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.disc_is_loss},
    { "helpfile",               conf_type_char,    BNETD_HELP_FILE,      NONE                , (void *)&prefs_runtime_config.helpfile},
    { "fortunecmd",             conf_type_char,    BNETD_FORTUNECMD,     NONE                , (void *)&prefs_runtime_config.fortunecmd},
    { "transfile",              conf_type_char,    BNETD_TRANS_FILE,     NONE                , (void *)&prefs_runtime_config.transfile},
    { "chanlog",                conf_type_bool,    NULL         ,        BNETD_CHANLOG       , (void *)&prefs_runtime_config.chanlog},
    { "chanlogdir",             conf_type_char,    BNETD_CHANLOG_DIR,    NONE                , (void *)&prefs_runtime_config.chanlogdir},
    { "quota",                  conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.quota},
    { "quota_lines",            conf_type_int,     NULL,                 BNETD_QUOTA_LINES   , (void *)&prefs_runtime_config.quota_lines},
    { "quota_time",             conf_type_int,     NULL,                 BNETD_QUOTA_TIME    , (void *)&prefs_runtime_config.quota_time},
    { "quota_wrapline",	        conf_type_int,     NULL,                 BNETD_QUOTA_WLINE   , (void *)&prefs_runtime_config.quota_wrapline},
    { "quota_maxline",	        conf_type_int,     NULL,                 BNETD_QUOTA_MLINE   , (void *)&prefs_runtime_config.quota_maxline},
    { "ladder_init_rating",     conf_type_int,     NULL,                 BNETD_LADDER_INIT_RAT , (void *)&prefs_runtime_config.ladder_init_rating},
    { "quota_dobae",            conf_type_int,     NULL,                 BNETD_QUOTA_DOBAE   , (void *)&prefs_runtime_config.quota_dobae},
    { "realmfile",              conf_type_char,    BNETD_REALM_FILE,     NONE                , (void *)&prefs_runtime_config.realmfile},
    { "issuefile",              conf_type_char,    BNETD_ISSUE_FILE,     NONE                , (void *)&prefs_runtime_config.issuefile},
    { "effective_user",         conf_type_char,    NULL,                 NONE                , (void *)&prefs_runtime_config.effective_user},
    { "effective_group",        conf_type_char,    NULL,                 NONE                , (void *)&prefs_runtime_config.effective_group},
    { "nullmsg",                conf_type_int,     NULL,                 BNETD_DEF_NULLMSG   , (void *)&prefs_runtime_config.nullmsg},
    { "mail_support",           conf_type_bool,    NULL,                 BNETD_MAIL_SUPPORT  , (void *)&prefs_runtime_config.mail_support},
    { "mail_quota",             conf_type_int,     NULL,                 BNETD_MAIL_QUOTA    , (void *)&prefs_runtime_config.mail_quota},
    { "maildir",                conf_type_char,    BNETD_MAIL_DIR,       NONE                , (void *)&prefs_runtime_config.maildir},
    { "log_notice",             conf_type_char,    BNETD_LOG_NOTICE,     NONE                , (void *)&prefs_runtime_config.log_notice},
    { "savebyname",             conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.savebyname},
    { "skip_versioncheck",      conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.skip_versioncheck},
    { "allow_bad_version",      conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.allow_bad_version},
    { "allow_unknown_version",  conf_type_bool,    NULL,                 0                   , (void *)&prefs_runtime_config.allow_unknown_version},
    { "versioncheck_file",      conf_type_char,    PVPGN_VERSIONCHECK,   NONE                , (void *)&prefs_runtime_config.versioncheck_file},
    { "d2cs_version",           conf_type_int,     NULL,                 0                   , (void *)&prefs_runtime_config.d2cs_version},
    { "allow_d2cs_setname",     conf_type_bool,    NULL,                 1                   , (void *)&prefs_runtime_config.allow_d2cs_setname},
    { "hashtable_size",         conf_type_int,     NULL,                 BNETD_HASHTABLE_SIZE , (void *)&prefs_runtime_config.hashtable_size},
    { "telnetaddrs",            conf_type_char,    BNETD_TELNET_ADDRS,   NONE                , (void *)&prefs_runtime_config.telnetaddrs},
    { "ipban_check_int",	conf_type_int,	   NULL,		 30		     , (void *)&prefs_runtime_config.ipban_check_int},
    { "version_exeinfo_match",  conf_type_char,    BNETD_EXEINFO_MATCH,  NONE                , (void *)&prefs_runtime_config.version_exeinfo_match},
    { "version_exeinfo_maxdiff",conf_type_int,     NULL,                 PVPGN_VERSION_TIMEDIV , (void *)&prefs_runtime_config.version_exeinfo_maxdiff},
    { "max_concurrent_logins",  conf_type_int,     NULL,                 0         	     , (void *)&prefs_runtime_config.max_concurrent_logins},
    { "identify_timeout_secs",  conf_type_int,	   NULL,		 W3_IDENTTIMEOUT     , (void *)&prefs_runtime_config.identify_timeout_secs},
    { "server_info", 		conf_type_char,	   "",	 		 NONE		     , (void *)&prefs_runtime_config.server_info},
    { "mapsfile",		conf_type_char,	   NULL,		 0          	     , (void *)&prefs_runtime_config.mapsfile},
    { "xplevelfile",    	conf_type_char,	   NULL,		 0          	     , (void *)&prefs_runtime_config.xplevelfile},
    { "xpcalcfile",		conf_type_char,	   NULL,		 0          	     , (void *)&prefs_runtime_config.xpcalcfile},
    { "initkill_timer", 	conf_type_int,     NULL,       		 0		     , (void *)&prefs_runtime_config.initkill_timer},
    { "war3_ladder_update_secs",conf_type_int,     NULL,                 0                   , (void *)&prefs_runtime_config.war3_ladder_update_secs},
    { "output_update_secs",	conf_type_int,     NULL,                 0                   , (void *)&prefs_runtime_config.output_update_secs},
    { "ladderdir",              conf_type_char,    BNETD_LADDER_DIR,     NONE                , (void *)&prefs_runtime_config.ladderdir},
    { "statusdir",              conf_type_char,    BNETD_STATUS_DIR,     NONE                , (void *)&prefs_runtime_config.statusdir},
    { "XML_output_ladder",      conf_type_bool,    NULL,                 0	    	     , (void *)&prefs_runtime_config.XML_output_ladder},
    { "XML_status_output",      conf_type_bool,    NULL,                 0	     	     , (void *)&prefs_runtime_config.XML_status_output},
    { "account_allowed_symbols",conf_type_char,    PVPGN_DEFAULT_SYMB,   NONE                , (void *)&prefs_runtime_config.account_allowed_symbols},
    { "command_groups_file",	conf_type_char,    BNETD_COMMAND_GROUPS_FILE,	NONE	     , (void *)&prefs_runtime_config.command_groups_file},
    { "tournament_file",	conf_type_char,    BNETD_TOURNAMENT_FILE,NONE		     , (void *)&prefs_runtime_config.tournament_file},
    { "aliasfile"          ,    conf_type_char,    BNETD_ALIASFILE   ,   NONE                , (void *)&prefs_runtime_config.aliasfile},
    { "w3trans_file",		conf_type_char,	   BNETD_W3TRANS_FILE,	 NONE		     , (void *)&prefs_runtime_config.w3trans_file},
    { "anongame_infos_file",	conf_type_char,	   PVPGN_AINFO_FILE,	 NONE		     , (void *)&prefs_runtime_config.anongame_infos_file},
    { "max_conns_per_IP",	conf_type_int,	   NULL,		 0		     , (void *)&prefs_runtime_config.max_conns_per_IP},
    { "max_friends",		conf_type_int,     NULL,                 MAX_FRIENDS         , (void *)&prefs_runtime_config.max_friends},
    { "clan_newer_time",        conf_type_int,     NULL,                 CLAN_NEWER_TIME     , (void *)&prefs_runtime_config.clan_newer_time},
    { "clan_max_members",       conf_type_int,     NULL,                 CLAN_MAX_MEMBERS    , (void *)&prefs_runtime_config.clan_max_members},
    { "clan_channel_default_private",   conf_type_bool,    NULL,         0                   , (void *)&prefs_runtime_config.clan_channel_default_private},
    { "passfail_count",		conf_type_int,     NULL,                 0                   , (void *)&prefs_runtime_config.passfail_count},
    { "passfail_bantime",	conf_type_int,     NULL,                 300                 , (void *)&prefs_runtime_config.passfail_bantime},
    { "maxusers_per_channel",	conf_type_int,	   NULL,		 0		     , (void *)&prefs_runtime_config.maxusers_per_channel},
    { "load_new_account",	conf_type_bool,	   NULL,		 0		     , (void *)&prefs_runtime_config.load_new_account},
    { NULL,             	conf_type_none,    NULL,                 NONE                , NULL},
};

#define PREFS_STORE_UINT(addr) (*((unsigned int *)(addr)))
#define PREFS_STORE_CHAR(addr) (*((char const **)(addr)))

char const * preffile=NULL;

static int processDirective(char const * directive, char const * value, unsigned int curLine)
{
    unsigned int i;
    
    if (!directive)
    {
	eventlog(eventlog_level_error,"processDirective","got NULL directive");
	return -1;
    }
    if (!value)
    {
	eventlog(eventlog_level_error,"processDirective","got NULL value");
	return -1;
    }
    
    for (i=0; conf_table[i].directive; i++)
        if (strcasecmp(conf_table[i].directive,directive)==0)
	{
            switch (conf_table[i].type)
            {
	    case conf_type_char:
		{
		    char const * temp;
		    
		    if (!(temp = strdup(value)))
		    {
			eventlog(eventlog_level_error,"processDirective","could not allocate memory for value");
			return -1;
		    }
		    if (PREFS_STORE_CHAR(conf_table[i].store))
			free((void *)PREFS_STORE_CHAR(conf_table[i].store)); /* avoid warning */
		    PREFS_STORE_CHAR(conf_table[i].store) = temp;
		}
		break;
		
	    case conf_type_int:
		{
		    unsigned int temp;
		    
		    if (str_to_uint(value,&temp)<0)
			eventlog(eventlog_level_error,"processDirective","invalid integer value \"%s\" for element \"%s\" at line %u",value,directive,curLine);
		    else
                	PREFS_STORE_UINT(conf_table[i].store) = temp;
		}
		break;
		
	    case conf_type_bool:
		switch (str_get_bool(value))
		{
		case 1:
		    PREFS_STORE_UINT(conf_table[i].store) = 1;
		    break;
		case 0:
		    PREFS_STORE_UINT(conf_table[i].store) = 0;
		    break;
		default:
		    eventlog(eventlog_level_error,"processDirective","invalid boolean value for element \"%s\" at line %u",directive,curLine);
		}
		break;
		
	    default:
		eventlog(eventlog_level_error,"processDirective","invalid type %d in table",(int)conf_table[i].type);
	    }
	    return 0;
	}
    
    eventlog(eventlog_level_error,"processDirective","unknown element \"%s\" at line %u",directive,curLine);
    return -1;
}


extern int prefs_load(char const * filename)
{
    /* restore defaults */
    {
	unsigned int i;
	
	for (i=0; conf_table[i].directive; i++)
	    switch (conf_table[i].type)
	    {
	    case conf_type_int:
	    case conf_type_bool:
		PREFS_STORE_UINT(conf_table[i].store) = conf_table[i].defintval;
		break;
		
	    case conf_type_char:
		if (PREFS_STORE_CHAR(conf_table[i].store))
		    free((void *)PREFS_STORE_CHAR(conf_table[i].store)); /* avoid warning */
		if (!conf_table[i].defcharval)
		    PREFS_STORE_CHAR(conf_table[i].store) = NULL;
		else
		    if (!(PREFS_STORE_CHAR(conf_table[i].store) = strdup(conf_table[i].defcharval)))
		    {
			eventlog(eventlog_level_error,"prefs_load","could not allocate memory for conf_table[i].charval");
			return -1;
		    }
		break;
		
	    default:
		eventlog(eventlog_level_error,"prefs_load","invalid type %d in table",(int)conf_table[i].type);
		return -1;
	    }
    }
    
    /* load file */
    if (filename)
    {
	FILE *       fp;
	char *       buff;
	char *       cp;
	char *       temp;
	unsigned int currline;
	unsigned int j;
	char const * directive;
	char const * value;
	char *       rawvalue;
	
        if (!(fp = fopen(filename,"r")))
        {
            eventlog(eventlog_level_error,"prefs_load","could not open file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
            return -1;
        }
	
	/* Read the configuration file */
	for (currline=1; (buff = file_get_line(fp)); currline++)
	{
	    cp = buff;
	    
            while (*cp=='\t' || *cp==' ') cp++;
	    if (*cp=='\0' || *cp=='#')
	    {
		free(buff);
		continue;
	    }
	    temp = cp;
	    while (*cp!='\t' && *cp!=' ' && *cp!='\0') cp++;
	    if (*cp!='\0')
	    {
		*cp = '\0';
		cp++;
	    }
	    if (!(directive = strdup(temp)))
	    {
		eventlog(eventlog_level_error,"prefs_load","could not allocate memory for directive");
		free(buff);
		continue;
	    }
            while (*cp=='\t' || *cp==' ') cp++;
	    if (*cp!='=')
	    {
		eventlog(eventlog_level_error,"prefs_load","missing = on line %u",currline);
		free((void *)directive); /* avoid warning */
		free(buff);
		continue;
	    }
	    cp++;
	    while (*cp=='\t' || *cp==' ') cp++;
	    if (*cp=='\0')
	    {
		eventlog(eventlog_level_error,"prefs_load","missing value after = on line %u",currline);
		free((void *)directive); /* avoid warning */
		free(buff);
		continue;
	    }
	    if (!(rawvalue = strdup(cp)))
	    {
		eventlog(eventlog_level_error,"prefs_load","could not allocate memory for rawvalue");
		free((void *)directive); /* avoid warning */
		free(buff);
		continue;
	    }
	    
	    if (rawvalue[0]=='"')
	    {
		char prev;
		
		for (j=1,prev='\0'; rawvalue[j]!='\0'; j++)
		{
		    switch (rawvalue[j])
		    {
		    case '"':
			if (prev!='\\')
			    break;
			prev = '"';
			continue;
		    case '\\':
			if (prev=='\\')
			    prev = '\0';
			else
			    prev = '\\';
			continue;
		    default:
			prev = rawvalue[j];
			continue;
		    }
		    break;
		}
		if (rawvalue[j]!='"')
		{
		    eventlog(eventlog_level_error,"prefs_load","missing end quote for value of element \"%s\" on line %u",directive,currline);
		    free(rawvalue);
		    free((void *)directive); /* avoid warning */
		    free(buff);
		    continue;
		}
		rawvalue[j] = '\0';
		if (rawvalue[j+1]!='\0' && rawvalue[j+1]!='#')
		{
		    eventlog(eventlog_level_error,"prefs_load","extra characters after the value for element \"%s\" on line %u",directive,currline);
		    free(rawvalue);
		    free((void *)directive); /* avoid warning */
		    free(buff);
		    continue;
		}
		value = &rawvalue[1];
            }
	    else
	    {
		unsigned int k;
		
		for (j=0; rawvalue[j]!='\0' && rawvalue[j]!=' ' && rawvalue[j]!='\t'; j++);
		k = j;
		while (rawvalue[k]==' ' || rawvalue[k]=='\t') k++;
		if (rawvalue[k]!='\0' && rawvalue[k]!='#')
		{
		    eventlog(eventlog_level_error,"prefs_load","extra characters after the value for element \"%s\" on line %u (%s)",directive,currline,&rawvalue[k]);
		    free(rawvalue);
		    free((void *)directive); /* avoid warning */
		    free(buff);
		    continue;
		}
		rawvalue[j] = '\0';
		value = rawvalue;
	    }
            
	    processDirective(directive,value,currline);
	    
	    free(rawvalue);
	    free((void *)directive); /* avoid warning */
	    free(buff);
	}
	if (fclose(fp)<0)
	    eventlog(eventlog_level_error,"prefs_load","could not close prefs file \"%s\" after reading (fclose: %s)",filename,strerror(errno));
    }
    
    return 0;
}


extern void prefs_unload(void)
{
    unsigned int i;
    
    for (i=0; conf_table[i].directive; i++)
	switch (conf_table[i].type)
	{
	case conf_type_int:
	case conf_type_bool:
	    break;
	    
	case conf_type_char:
	    if (PREFS_STORE_CHAR(conf_table[i].store))
	    {
		free((void *)PREFS_STORE_CHAR(conf_table[i].store)); /* avoid warning */
		PREFS_STORE_CHAR(conf_table[i].store) = NULL;
	    }
	    break;
	    
	default:
	    eventlog(eventlog_level_error,"prefs_unload","invalid type %d in table",(int)conf_table[i].type);
	    break;
	}
}

extern char const * prefs_get_storage_path(void)
{
    return prefs_runtime_config.storage_path;
}


extern char const * prefs_get_filedir(void)
{
    return prefs_runtime_config.filedir;
}


extern char const * prefs_get_logfile(void)
{
    return prefs_runtime_config.logfile;
}


extern char const * prefs_get_loglevels(void)
{
    return prefs_runtime_config.loglevels;
}


extern char const * prefs_get_motdfile(void)
{
    return prefs_runtime_config.motdfile;
}


extern char const * prefs_get_newsfile(void)
{
    return prefs_runtime_config.newsfile;
}


extern char const * prefs_get_adfile(void)
{
    return prefs_runtime_config.adfile;
}

extern char const * prefs_get_topicfile(void)
{
    return prefs_runtime_config.topicfile;
}

extern char const * prefs_get_DBlayoutfile(void)
{
    return prefs_runtime_config.DBlayoutfile;
}


extern unsigned int prefs_get_user_sync_timer(void)
{
    return prefs_runtime_config.usersync;
}


extern unsigned int prefs_get_user_flush_timer(void)
{
    return prefs_runtime_config.userflush;
}


extern unsigned int prefs_get_user_step(void)
{
    return prefs_runtime_config.userstep;
}


extern char const * prefs_get_servername(void)
{
    return prefs_runtime_config.servername;
}

extern unsigned int prefs_get_track(void)
{
    unsigned int rez;
    
    rez = prefs_runtime_config.track;
    if (rez>0 && rez<60) rez = 60;
    return rez;
}


extern char const * prefs_get_location(void)
{
    return prefs_runtime_config.location;
}


extern char const * prefs_get_description(void)
{
    return prefs_runtime_config.description;
}


extern char const * prefs_get_url(void)
{
    return prefs_runtime_config.url;
}


extern char const * prefs_get_contact_name(void)
{
    return prefs_runtime_config.contact_name;
}


extern char const * prefs_get_contact_email(void)
{
    return prefs_runtime_config.contact_email;
}


extern unsigned int prefs_get_latency(void)
{
    return prefs_runtime_config.latency;
}


extern unsigned int prefs_get_irc_latency(void)
{
    return prefs_runtime_config.irc_latency;
}


extern unsigned int prefs_get_shutdown_delay(void)
{
    return prefs_runtime_config.shutdown_delay;
}


extern unsigned int prefs_get_shutdown_decr(void)
{
    return prefs_runtime_config.shutdown_decr;
}


extern unsigned int prefs_get_allow_new_accounts(void)
{
    return prefs_runtime_config.new_accounts;
}


extern unsigned int prefs_get_max_accounts(void)
{
    return prefs_runtime_config.max_accounts;
}


extern unsigned int prefs_get_kick_old_login(void)
{
    return prefs_runtime_config.kick_old_login;
}


extern char const * prefs_get_channelfile(void)
{
    return prefs_runtime_config.channelfile;
}


extern unsigned int prefs_get_ask_new_channel(void)
{
    return prefs_runtime_config.ask_new_channel;
}


extern unsigned int prefs_get_hide_pass_games(void)
{
    return prefs_runtime_config.hide_pass_games;
}


extern unsigned int prefs_get_hide_started_games(void)
{
    return prefs_runtime_config.hide_started_games;
}


extern unsigned int prefs_get_hide_temp_channels(void)
{
    return prefs_runtime_config.hide_temp_channels;
}


extern unsigned int prefs_get_hide_addr(void)
{
    return prefs_runtime_config.hide_addr;
}


extern unsigned int prefs_get_enable_conn_all(void)
{
    return prefs_runtime_config.enable_conn_all;
}


extern unsigned int prefs_get_extra_commands(void)
{
    return prefs_runtime_config.extra_commands;
}


extern char const * prefs_get_reportdir(void)
{
    return prefs_runtime_config.reportdir;
}


extern unsigned int prefs_get_report_all_games(void)
{
    return prefs_runtime_config.report_all_games;
}

extern unsigned int prefs_get_report_diablo_games(void)
{
    return prefs_runtime_config.report_diablo_games;
}

extern char const * prefs_get_pidfile(void)
{
    return prefs_runtime_config.pidfile;
}


extern char const * prefs_get_iconfile(void)
{
    return prefs_runtime_config.iconfile;
}

extern char const * prefs_get_war3_iconfile(void)
{
    return prefs_runtime_config.war3_iconfile;
}

extern char const * prefs_get_star_iconfile(void)
{
    return prefs_runtime_config.star_iconfile;
}


extern char const * prefs_get_tosfile(void)
{
    return prefs_runtime_config.tosfile;
}


extern char const * prefs_get_mpqfile(void)
{
    return prefs_runtime_config.mpqfile;
}


extern char const * prefs_get_trackserv_addrs(void)
{
    return prefs_runtime_config.trackaddrs;
}

extern char const * prefs_get_bnetdserv_addrs(void)
{
    return prefs_runtime_config.servaddrs;
}

extern char const * prefs_get_w3route_addr(void)
{
    return prefs_runtime_config.w3routeaddr;
}

extern char const * prefs_get_irc_addrs(void)
{
    return prefs_runtime_config.ircaddrs;
}


extern unsigned int prefs_get_use_keepalive(void)
{
    return prefs_runtime_config.use_keepalive;
}


extern unsigned int prefs_get_udptest_port(void)
{
    return prefs_runtime_config.udptest_port;
}


extern char const * prefs_get_ipbanfile(void)
{
    return prefs_runtime_config.ipbanfile;
}


extern unsigned int prefs_get_discisloss(void)
{
    return prefs_runtime_config.disc_is_loss;
}


extern char const * prefs_get_helpfile(void)
{
    return prefs_runtime_config.helpfile;
}


extern char const * prefs_get_fortunecmd(void)
{
    return prefs_runtime_config.fortunecmd;
}


extern char const * prefs_get_transfile(void)
{
    return prefs_runtime_config.transfile;
}


extern unsigned int prefs_get_chanlog(void)
{
    return prefs_runtime_config.chanlog;
}


extern char const * prefs_get_chanlogdir(void)
{
    return prefs_runtime_config.chanlogdir;
}


extern unsigned int prefs_get_quota(void)
{
    return prefs_runtime_config.quota;
}


extern unsigned int prefs_get_quota_lines(void)
{
    unsigned int rez;
    
    rez=prefs_runtime_config.quota_lines;
    if (rez<1) rez = 1;
    if (rez>100) rez = 100;
    return rez;
}


extern unsigned int prefs_get_quota_time(void)
{
    unsigned int rez;
    
    rez=prefs_runtime_config.quota_time;
    if (rez<1) rez = 1;
    if (rez>10) rez = 60;
    return rez;
}


extern unsigned int prefs_get_quota_wrapline(void)
{
    unsigned int rez;
    
    rez=prefs_runtime_config.quota_wrapline;
    if (rez<1) rez = 1;
    if (rez>256) rez = 256;
    return rez;
}


extern unsigned int prefs_get_quota_maxline(void)
{
    unsigned int rez;
    
    rez=prefs_runtime_config.quota_maxline;
    if (rez<1) rez = 1;
    if (rez>256) rez = 256;
    return rez;
}


extern unsigned int prefs_get_ladder_init_rating(void)
{
    return prefs_runtime_config.ladder_init_rating;
}


extern unsigned int prefs_get_quota_dobae(void)
{
    unsigned int rez;

    rez=prefs_runtime_config.quota_dobae;
    if (rez<1) rez = 1;
    if (rez>100) rez = 100;
    return rez;
}


extern char const * prefs_get_realmfile(void)
{
    return prefs_runtime_config.realmfile;
}


extern char const * prefs_get_issuefile(void)
{
    return prefs_runtime_config.issuefile;
}


extern char const * prefs_get_effective_user(void)
{
    return prefs_runtime_config.effective_user;
}


extern char const * prefs_get_effective_group(void)
{
    return prefs_runtime_config.effective_group;
}


extern unsigned int prefs_get_nullmsg(void)
{
    return prefs_runtime_config.nullmsg;
}


extern unsigned int prefs_get_mail_support(void)
{
    return prefs_runtime_config.mail_support;
}


extern unsigned int prefs_get_mail_quota(void)
{
    unsigned int rez;
    
    rez=prefs_runtime_config.mail_quota;
    if (rez<1) rez = 1;
    if (rez>30) rez = 30;
    return rez;
}


extern char const * prefs_get_maildir(void)
{
    return prefs_runtime_config.maildir;
}


extern char const * prefs_get_log_notice(void)
{
    return prefs_runtime_config.log_notice;
}


extern unsigned int prefs_get_savebyname(void)
{
    return prefs_runtime_config.savebyname;
}


extern unsigned int prefs_get_skip_versioncheck(void)
{
    return prefs_runtime_config.skip_versioncheck;
}


extern unsigned int prefs_get_allow_bad_version(void)
{
    return prefs_runtime_config.allow_bad_version;
}


extern unsigned int prefs_get_allow_unknown_version(void)
{
    return prefs_runtime_config.allow_unknown_version;
}


extern char const * prefs_get_versioncheck_file(void)
{
    return prefs_runtime_config.versioncheck_file;
}


extern unsigned int prefs_allow_d2cs_setname(void)
{
        return prefs_runtime_config.allow_d2cs_setname;
}


extern unsigned int prefs_get_d2cs_version(void)
{
        return prefs_runtime_config.d2cs_version;
}


extern unsigned int prefs_get_hashtable_size(void)
{
    return prefs_runtime_config.hashtable_size;
}


extern char const * prefs_get_telnet_addrs(void)
{
    return prefs_runtime_config.telnetaddrs;
}


extern unsigned int prefs_get_ipban_check_int(void)
{
    return prefs_runtime_config.ipban_check_int;
}


extern char const * prefs_get_version_exeinfo_match(void)
{
    return prefs_runtime_config.version_exeinfo_match;
}


extern unsigned int prefs_get_version_exeinfo_maxdiff(void)
{
    return prefs_runtime_config.version_exeinfo_maxdiff;
}

// added by NonReal
extern unsigned int prefs_get_max_concurrent_logins(void)
{
    return prefs_runtime_config.max_concurrent_logins;
}

/* ADDED BY UNDYING SOULZZ 4/9/02 */
extern unsigned int prefs_get_identify_timeout_secs(void)
{
    return prefs_runtime_config.identify_timeout_secs;
}

extern char const * prefs_get_server_info( void )
{
    return prefs_runtime_config.server_info;
}

extern char const * prefs_get_mapsfile(void)
{
    return prefs_runtime_config.mapsfile;
}

extern char const * prefs_get_xplevel_file(void)
{
    return prefs_runtime_config.xplevelfile;
}

extern char const * prefs_get_xpcalc_file(void)
{
    return prefs_runtime_config.xpcalcfile;
}

extern int prefs_get_initkill_timer(void)
{
	return prefs_runtime_config.initkill_timer;
 }

extern int prefs_get_war3_ladder_update_secs(void)
{
        return prefs_runtime_config.war3_ladder_update_secs;
}

extern int prefs_get_output_update_secs(void)
{
        return prefs_runtime_config.output_update_secs;
}

extern char const * prefs_get_ladderdir(void)
{
        return prefs_runtime_config.ladderdir;
}

extern char const * prefs_get_outputdir(void)
{
        return prefs_runtime_config.statusdir;
}

extern int prefs_get_XML_output_ladder(void)
{
        return prefs_runtime_config.XML_output_ladder;
}

extern int prefs_get_XML_status_output(void)
{
        return prefs_runtime_config.XML_status_output;
}

extern char const * prefs_get_account_allowed_symbols(void)
{
	return prefs_runtime_config.account_allowed_symbols;
}

extern char const * prefs_get_command_groups_file(void)
{
    return prefs_runtime_config.command_groups_file;
}

extern char const * prefs_get_tournament_file(void)
{
    return prefs_runtime_config.tournament_file;
}

extern char const * prefs_get_aliasfile(void)
{
   return prefs_runtime_config.aliasfile;
}

extern char const * prefs_get_w3trans_file(void)
{
    return prefs_runtime_config.w3trans_file;
}

extern char const * prefs_get_anongame_infos_file(void)
{
	return prefs_runtime_config.anongame_infos_file;
}

extern unsigned int prefs_get_max_conns_per_IP(void)
{
	return prefs_runtime_config.max_conns_per_IP;
}

extern int prefs_get_max_friends(void)
{
	return prefs_runtime_config.max_friends;
}

extern unsigned int prefs_get_clan_newer_time(void)
{
    return prefs_runtime_config.clan_newer_time;
}

extern unsigned int prefs_get_clan_max_members(void)
{
    return prefs_runtime_config.clan_max_members;
}

extern unsigned int prefs_get_clan_channel_default_private(void)
{
    return prefs_runtime_config.clan_channel_default_private;
}

extern unsigned int prefs_get_passfail_count(void)
{
    return prefs_runtime_config.passfail_count;
}

extern unsigned int prefs_get_passfail_bantime(void)
{
    return prefs_runtime_config.passfail_bantime;
}

extern unsigned int prefs_get_maxusers_per_channel(void)
{
    return prefs_runtime_config.maxusers_per_channel;
}

extern unsigned int prefs_get_load_new_account(void)
{
    return prefs_runtime_config.load_new_account;
}

extern char const * prefs_get_supportfile(void)
{
    return prefs_runtime_config.supportfile;
}
