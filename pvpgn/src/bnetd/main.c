/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#include "compat/exitstatus.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strdup.h"
#include <errno.h>
#include "compat/strerror.h"
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "compat/stdfileno.h"
#include "compat/psock.h"
#include "common/hexdump.h"
#include "channel.h"
#include "game.h"
#include "server.h"
#include "common/eventlog.h"
#include "account.h"
#include "connection.h"
#include "game.h"
#include "common/version.h"
#include "prefs.h"
#include "ladder.h"
#include "adbanner.h"
#include "ipban.h"
#include "autoupdate.h"
#include "helpfile.h"
#include "timer.h"
#include "watch.h"
#include "common/tracker.h"
#include "realm.h"
#include "character.h"
#include "common/give_up_root_privileges.h"
#include "versioncheck.h"
#include "storage.h"
#include "anongame.h"
//aaron
#include "command_groups.h"
#include "output.h"
#include "alias_command.h"
#include "anongame_infos.h"
#include "anongame_maplists.h"
#include "tournament.h"
#include "news.h"
#include "clan.h"
#include "team.h"
#include "topic.h"
#include "support.h"
#include "common/trans.h"
#include "common/xalloc.h"
#include "common/fdwatch.h"
#include "common/setup_after.h"

/* out of memory safety */
#define OOM_SAFE_MEM	1000000		/* 1 Mbyte of safety memory */

void *oom_buffer = NULL;

static int bnetd_oom_handler(void)
{
    /* no safety buffer, sorry :( */
    if (!oom_buffer) return 0;

    /* free the safety buffer hoping next allocs will succeed */
    free(oom_buffer);
    oom_buffer = NULL;

    eventlog(eventlog_level_fatal,__FUNCTION__,"out of memory, forcing immediate shutdown");

    /* shutdown immediatly */
    server_quit_delay(-1);

    return 1;	/* ask xalloc codes to retry the allocation request */
}

static int oom_setup(void)
{
    oom_buffer = malloc(OOM_SAFE_MEM);
    if (!oom_buffer) return -1;

    xalloc_setcb(bnetd_oom_handler);
    return 0;
}

static void oom_free(void)
{
    if (oom_buffer) free(oom_buffer);
    oom_buffer = NULL;
}

#ifdef WIN32
# include "win32/service.h"
#endif

#ifdef WIN32_GUI
#include "win32/winmain.h"
    int static fprintf(FILE *stream, const char *format, ...){
     va_list args;

        va_start(args, format);
        if(stream == stderr || stream == stdout) 
         return gui_lvprintf(eventlog_level_error, format, args);
        else return vfprintf(stream, format, args);
    }
#define printf gui_printf
#endif

FILE	*hexstrm = NULL;

// by quetzal. indicates service status
// -1 - not in service mode
//  0 - stopped
//  1 - running
//  2 - paused
int g_ServiceStatus = -1;

static void usage(char const * progname);

static void usage(char const * progname)
{
    fprintf(stderr,
	    "usage: %s [<options>]\n"
	    "    -c FILE, --config=FILE   use FILE as configuration file (default is " BNETD_DEFAULT_CONF_FILE ")\n"
	    "    -d FILE, --hexdump=FILE  do hex dump of packets into FILE\n"
#ifdef DO_DAEMONIZE
	    "    -f, --foreground         don't daemonize\n"
#else
	    "    -f, --foreground         don't daemonize (default)\n"
#endif
	    "    -D, --debug              run in debug mode (run in foreground and log to stdout)\n"
	    "    -h, --help, --usage      show this information and exit\n"
	    "    -v, --version            print version number and exit\n"
#ifdef WIN32
	    "    Running as service functions:\n"
	    "	 --service		  run as service\n"
	    "    -s install               install service\n"
	    "    -s uninstall             uninstall service\n"
#endif	    
	    ,progname);
}


// added some more exit status --> put in "compat/exitstatus.h" ???
#define STATUS_OOM_FAILURE		20
#define STATUS_STORAGE_FAILURE		30
#define STATUS_PSOCK_FAILURE		35
#define STATUS_MAPLISTS_FAILURE		40
#define STATUS_MATCHLISTS_FAILURE	50
#define STATUS_LADDERLIST_FAILURE	60
#define STATUS_WAR3XPTABLES_FAILURE	70
#define STATUS_SUPPORT_FAILURE          80
#define STATUS_FDWATCH_FAILURE          90

// new functions extracted from Fw()
int read_commandline(int argc, char * * argv, int *foreground, char const *preffile[], char *hexfile[]);
int pre_server_startup(void);
void post_server_shutdown(int status);
int eventlog_startup(void);
int fork_bnetd(int foreground);
char * write_to_pidfile(void);
void pvpgn_greeting(void);

// The functions 
int read_commandline(int argc, char * * argv, int *foreground, char const *preffile[], char *hexfile[])
{
    int a;
    
    if (argc<1 || !argv || !argv[0]) {
	fprintf(stderr,"bad arguments\n");
        return -1;
    }
#ifdef WIN32
    if (argc > 1 && strncmp(argv[1], "--service", 9) == 0) {
	Win32_ServiceRun();
	return 0;
    }
#endif
    for (a=1; a<argc; a++) {
	if (strncmp(argv[a],"--config=",9)==0) {
	    if (*preffile) {
		fprintf(stderr,"%s: configuration file was already specified as \"%s\"\n",argv[0],preffile[0]);
                usage(argv[0]);
                return -1;
            }
	    *preffile = &argv[a][9];
	}
	else if (strcmp(argv[a],"-c")==0) {
            if (a+1>=argc) {
                fprintf(stderr,"%s: option \"%s\" requires an argument\n",argv[0],argv[a]);
                usage(argv[0]);
                return -1;
            }
            if (*preffile) {
                fprintf(stderr,"%s: configuration file was already specified as \"%s\"\n",argv[0],preffile[0]);
                usage(argv[0]);
                return -1;
            }
            a++;
	    *preffile = argv[a];
        }
        else if (strncmp(argv[a],"--hexdump=",10)==0) {
            if (*hexfile) {
                fprintf(stderr,"%s: configuration file was already specified as \"%s\"\n",argv[0],hexfile[0]);
                usage(argv[0]);
                return -1;
            }
            *hexfile = &argv[a][10];
        }
        else if (strcmp(argv[a],"-d")==0) {
            if (a+1>=argc) {
                fprintf(stderr,"%s: option \"%s\" requires an argument\n",argv[0],argv[a]);
                usage(argv[0]);
                return -1;
            }
            if (*hexfile) {
                fprintf(stderr,"%s: configuration file was already specified as \"%s\"\n",argv[0],hexfile[0]);
                usage(argv[0]);
                return -1;
            }
            a++;
            *hexfile = argv[a];
        }
        else if (strcmp(argv[a],"-f")==0 || strcmp(argv[a],"--foreground")==0)
            *foreground = 1;
        else if (strcmp(argv[a],"-D")==0 || strcmp(argv[a],"--debug")==0) {
	    eventlog_set_debugmode(1);
            *foreground = 1;
        }
#ifdef WIN32
	else if (strcmp(argv[a],"-s") == 0) {
	    if (a < argc - 1) {
		if (strcmp(argv[a+1], "install") == 0) {
		    fprintf(stderr, "Installing service");
		    Win32_ServiceInstall();
		}
		if (strcmp(argv[a+1], "uninstall") == 0) {
		    fprintf(stderr, "Uninstalling service");
		    Win32_ServiceUninstall();
		}
	    }
	    return 0;
        }
#endif
        else if (strcmp(argv[a],"-v")==0 || strcmp(argv[a],"--version")==0) {
            printf(PVPGN_SOFTWARE" version "PVPGN_VERSION"\n");
            return 0;
	}
        else if (strcmp(argv[a],"-h")==0 || strcmp(argv[a],"--help")==0 || strcmp(argv[a],"--usage")==0) {
	    usage(argv[0]);
	    return 0;
	}
	else if (strcmp(argv[a],"--config")==0 || strcmp(argv[a],"--hexdump")==0) {
            fprintf(stderr,"%s: option \"%s\" requires and argument.\n",argv[0],argv[a]);
            usage(argv[0]);
            return -1;
	}
	else {
            fprintf(stderr,"%s: bad option \"%s\"\n",argv[0],argv[a]);
            usage(argv[0]);
            return -1;
        }
    }
    return 1; // continue without exiting
}

int eventlog_startup(void)
{
    char const * levels;
    char *       temp;
    char const * tok;
    
    eventlog_clear_level();
    if ((levels = prefs_get_loglevels())) {
	temp = xstrdup(levels);
	tok = strtok(temp,","); /* strtok modifies the string it is passed */
	while (tok) {
	    if (eventlog_add_level(tok)<0)
		eventlog(eventlog_level_error,__FUNCTION__,"could not add log level \"%s\"",tok);
	    tok = strtok(NULL,",");
	}
	xfree(temp);
    }
    if (eventlog_open(prefs_get_logfile())<0) {
	if (prefs_get_logfile()) {
	    eventlog(eventlog_level_fatal,"eventlog_startup","could not use file \"%s\" for the eventlog (exiting)",prefs_get_logfile());
	} else {
	    eventlog(eventlog_level_fatal,"eventlog_startup","no logfile specified in configuration file \"%s\" (exiting)",preffile);
	}
	return -1;
    }
    eventlog(eventlog_level_info,__FUNCTION__,"logging event levels: %s",prefs_get_loglevels());
    return 0;
}

int fork_bnetd(int foreground)
{
    int		pid;
    
#ifdef DO_DAEMONIZE
    if (!foreground) {
	if (chdir("/")<0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not change working directory to / (chdir: %s)",strerror(errno));
	    return -1;
	}
	
	switch ((pid = fork())) {
	    case -1:
		eventlog(eventlog_level_error,__FUNCTION__,"could not fork (fork: %s)",strerror(errno));
		return -1;
	    case 0: /* child */
		break;
	    default: /* parent */
		return pid;
	}
#ifndef WITH_D2	
	close(STDINFD);
	close(STDOUTFD);
	close(STDERRFD);
#endif
# ifdef HAVE_SETPGID
	if (setpgid(0,0)<0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not create new process group (setpgid: %s)",strerror(errno));
	    return -1;
	}
# else
#  ifdef HAVE_SETPGRP
#   ifdef SETPGRP_VOID
        if (setpgrp()<0) {
            eventlog(eventlog_level_error,__FUNCTION__,"could not create new process group (setpgrp: %s)",strerror(errno));
            return -1;
        }
#   else
	if (setpgrp(0,0)<0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not create new process group (setpgrp: %s)",strerror(errno));
	    return -1;
	}
#   endif
#  else
#   ifdef HAVE_SETSID
	if (setsid()<0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not create new process group (setsid: %s)",strerror(errno));
	    return -1;
	}
#   else
#    error "One of setpgid(), setpgrp(), or setsid() is required"
#   endif
#  endif
# endif
    }
    return 0;
#endif
return 0;
}

char * write_to_pidfile(void)
{
    char *pidfile = xstrdup(prefs_get_pidfile());
    
    if (pidfile[0]=='\0') {
	xfree((void *)pidfile); /* avoid warning */
	return NULL;
    }
    if (pidfile) {
#ifdef HAVE_GETPID
    FILE * fp;
	
    if (!(fp = fopen(pidfile,"w"))) {
	eventlog(eventlog_level_error,__FUNCTION__,"unable to open pid file \"%s\" for writing (fopen: %s)",pidfile,strerror(errno));
	xfree((void *)pidfile); /* avoid warning */
	return NULL;
    } else {
	fprintf(fp,"%u",(unsigned int)getpid());
	if (fclose(fp)<0)
	    eventlog(eventlog_level_error,__FUNCTION__,"could not close pid file \"%s\" after writing (fclose: %s)",pidfile,strerror(errno));
    }
#else
    eventlog(eventlog_level_warn,__FUNCTION__,"no getpid() system call, disable pid file in bnetd.conf");
    xfree((void *)pidfile); /* avoid warning */
    return NULL;
#endif
    }
    return pidfile;
}

int pre_server_startup(void)
{
    pvpgn_greeting();
    if (oom_setup() < 0) {
	eventlog(eventlog_level_error, __FUNCTION__, "OOM init failed");
	return STATUS_OOM_FAILURE;
    }
    if (storage_init(prefs_get_storage_path()) < 0) {
	eventlog(eventlog_level_error, "pre_server_startup", "storage init failed");
	return STATUS_STORAGE_FAILURE;
    }
    if (psock_init() < 0) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not initialize socket functions");
	return STATUS_PSOCK_FAILURE;
    }
    if (support_check_files(prefs_get_supportfile()) < 0) {
        eventlog(eventlog_level_error, "pre_server_startup","some needed files are missing");
	eventlog(eventlog_level_error, "pre_server_startup","please make sure you installed the supportfiles in %s",prefs_get_filedir());
	return STATUS_SUPPORT_FAILURE;
    }
    if (anongame_maplists_create() < 0) {
	eventlog(eventlog_level_error, "pre_server_startup", "could not load maps");
	return STATUS_MAPLISTS_FAILURE;
    }
    if (anongame_matchlists_create() < 0) {
	eventlog(eventlog_level_error, "pre_server_startup", "could not create matchlists");
	return STATUS_MATCHLISTS_FAILURE;
    }
    if (fdwatch_init(prefs_get_max_connections())) {
	eventlog(eventlog_level_error, __FUNCTION__, "error initilizing fdwatch");
	return STATUS_FDWATCH_FAILURE;
    }
    if (realmlist_create(prefs_get_realmfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load realm list");
    connlist_create();
    gamelist_create();
    timerlist_create();
    server_set_name();
    channellist_create();
    if (helpfile_init(prefs_get_helpfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load helpfile");
    ipbanlist_create();
    if (ipbanlist_load(prefs_get_ipbanfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load IP ban list");
    if (adbannerlist_create(prefs_get_adfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load adbanner list");
    if (autoupdate_load(prefs_get_mpqfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load autoupdate list");
    if (versioncheck_load(prefs_get_versioncheck_file())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load versioncheck list");
   if (news_load(prefs_get_newsfile())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load news list");
    watchlist_create();
    output_init();
    accountlist_load_default();
    accountlist_create();
    if (ladder_createxptable(prefs_get_xplevel_file(),prefs_get_xpcalc_file())<0) {
        eventlog(eventlog_level_error, "pre_server_startup", "could not load WAR3 xp calc tables");
        return STATUS_WAR3XPTABLES_FAILURE;
    }
    ladders_init();
    ladders_load_accounts_to_ladderlists();
    ladder_update_all_accounts();
    if (characterlist_create("")<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load character list");
    if (prefs_get_track()) /* setup the tracking mechanism */
        tracker_set_servers(prefs_get_trackserv_addrs());
    if (command_groups_load(prefs_get_command_groups_file())<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load command_groups list");
    aliasfile_load(prefs_get_aliasfile());
    if (trans_load(prefs_get_transfile(),TRANS_BNETD)<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not load trans list");
    tournament_init(prefs_get_tournament_file());
    anongame_infos_load(prefs_get_anongame_infos_file());
    clanlist_load();
    teamlist_load();
    topiclist_load(prefs_get_topicfile());
    return 0;
}

void post_server_shutdown(int status)
{
    switch (status)
    {
	case 0:
	    topiclist_unload();
	    teamlist_unload();
            clanlist_unload();
	    tournament_destroy();
	    anongame_infos_unload();
	    trans_unload();
	    aliasfile_unload();
	    command_groups_unload();
	    tracker_set_servers(NULL);
	    characterlist_destroy();
            ladder_destroyxptable();
        case STATUS_WAR3XPTABLES_FAILURE:
    	    
	case STATUS_LADDERLIST_FAILURE:
	    ladder_update_all_accounts();
    	    ladders_destroy();
	    output_dispose_filename();
	    accountlist_destroy();
    	    accountlist_unload_default();
    	    watchlist_destroy();
	    news_unload();
    	    versioncheck_unload();
    	    autoupdate_unload();
    	    adbannerlist_destroy();
    	    ipbanlist_save(prefs_get_ipbanfile());
    	    ipbanlist_destroy();
    	    helpfile_unload();
    	    channellist_destroy();
	    server_clear_name();
    	    timerlist_destroy();
	    gamelist_destroy();
	    connlist_destroy();
    	    realmlist_destroy();
	    fdwatch_close();
	case STATUS_FDWATCH_FAILURE:
	    anongame_matchlists_destroy();
	case STATUS_MATCHLISTS_FAILURE:
	    anongame_maplists_destroy();
	case STATUS_MAPLISTS_FAILURE:
	case STATUS_SUPPORT_FAILURE:
	    if (psock_deinit())
		eventlog(eventlog_level_error, __FUNCTION__, "got error from psock_deinit()");
	case STATUS_PSOCK_FAILURE:
	    storage_close();
	case STATUS_STORAGE_FAILURE:
	    oom_free();
	case STATUS_OOM_FAILURE:
	case -1:
	    break;
	default:
	    eventlog(eventlog_level_error,__FUNCTION__,"got bad status \"%d\" during shutdown",status);
    }
    return;
}    

void pvpgn_greeting(void)
{
#ifdef HAVE_GETPID
    eventlog(eventlog_level_info,__FUNCTION__,PVPGN_SOFTWARE" version "PVPGN_VERSION" process %u",(unsigned int)getpid());
#else
    eventlog(eventlog_level_info,__FUNCTION__,PVPGN_SOFTWARE" version "PVPGN_VERSION);
#endif
    
    printf("You are currently Running "PVPGN_SOFTWARE" "PVPGN_VERSION"\n");
    printf("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
    printf("Make sure to visit:\n");
    printf("http://www.pvpgn.org\n");
    printf("We can also be found on: irc.pvpgn.org\n");
    printf("Channel: #pvpgn\n");
    printf("Server is now running.\n");
    printf("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
    
    return;
}

// MAIN STARTS HERE!!!
#ifdef WIN32_GUI
extern int bnetd_main(int argc, char * * argv)
#else
extern int main(int argc, char * * argv)
#endif
{
    int a;
    int foreground = 0;
    char *hexfile = NULL;
    char *pidfile;

// Read the command line and set variables
    if ((a = read_commandline(argc, argv, &foreground, &preffile, &hexfile)) != 1)
	return a;

// Fork to child process if not set to foreground    
    if ((a = fork_bnetd(foreground)) != 0)
	return a < 0 ? a : 0; /* dizzy: dont return code != 0 when things are OK! */

    eventlog_set(stderr);
    /* errors to eventlog from here on... */

// Load the prefs
    if (preffile) {
	if (prefs_load(preffile)<0) { // prefs are loaded here ...
	    eventlog(eventlog_level_fatal,"main","could not parse specified configuration file (exiting)");
	    return -1;
	}
    } else {
	if (prefs_load(BNETD_DEFAULT_CONF_FILE)<0) // or prefs are loaded here ..  if not defined on command line ...
	    eventlog(eventlog_level_warn,__FUNCTION__,"using default configuration"); // or use defaults if default conf is not found
    }

// Start logging to log file
    if (eventlog_startup() == -1)
	return -1;
    /* eventlog goes to log file from here on... */

// Give up root privileges
    /* Hakan: That's way too late to give up root privileges... Have to look for a better place */
    if (give_up_root_privileges(prefs_get_effective_user(),prefs_get_effective_group())<0) {
        eventlog(eventlog_level_fatal,"main","could not give up privileges (exiting)");
        return -1;
    }
	
// Write the pidfile
    pidfile = write_to_pidfile();

// Open the hexfile for writing
    if (hexfile) {
	if (!(hexstrm = fopen(hexfile,"w")))
	    eventlog(eventlog_level_error,__FUNCTION__,"could not open file \"%s\" for writing the hexdump (fopen: %s)",hexfile,strerror(errno));
	else
	    fprintf(hexstrm,"# dump generated by "PVPGN_SOFTWARE" version "PVPGN_VERSION"\n");
    }

// Run the pre server stuff
    a = pre_server_startup();
    
// now process connections and network traffic
    if (a == 0) {
	if (server_process() < 0) 
	    eventlog(eventlog_level_fatal,"main","failed to initialize network (exiting)");
    }
    
// run post server stuff and exit
    post_server_shutdown(a);    

// Close hexfile
    if (hexstrm) {
	fprintf(hexstrm,"# end of dump\n");
	if (fclose(hexstrm)<0)
	    eventlog(eventlog_level_error,__FUNCTION__,"could not close hexdump file \"%s\" after writing (fclose: %s)",hexfile,strerror(errno));
    }

// Delete pidfile
    if (pidfile) {
	if (remove(pidfile)<0)
	    eventlog(eventlog_level_error,__FUNCTION__,"could not remove pid file \"%s\" (remove: %s)",pidfile,strerror(errno));
	xfree((void *)pidfile); /* avoid warning */
    }

    if (a == 0)
	eventlog(eventlog_level_info,__FUNCTION__,"server has shut down");
    prefs_unload();
    eventlog_close();
    
    if (a == 0)
	return 0;

    return -1;
}
