/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2005 Dizzy 
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
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "common/xalloc.h"
#ifdef WIN32
# include "win32/service.h"
#endif
#ifdef WIN32_GUI
# include "win32/winmain.h"
# define printf gui_printf
#endif
#include "common/eventlog.h"
#include "common/setup_after.h"

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
            "    --service                run as service\n"
            "    -s install               install service\n"
            "    -s uninstall             uninstall service\n"
#endif      
            ,progname);
}

extern int read_commandline(int argc, char * * argv, int *foreground, char const *preffile[], char *hexfile[])
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
    return 1;
}
