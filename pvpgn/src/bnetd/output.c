/*
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
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <errno.h>
#include "output.h"
#include "prefs.h"
#include "connection.h"
#include "game.h"
#include "ladder.h"
#include "server.h"
#include "channel.h"
#include "account.h"
#include "common/util.h"
#include "common/bnettime.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/proginfo.h"
#include "compat/strerror.h"
#include "clienttag.h"

char * status_filename;

int output_standard_writer(FILE * fp);

/* 
 * Initialisation Output *
 */

extern void output_init(void)
{
    eventlog(eventlog_level_info,"output_init","initializing output file");

    if (prefs_get_XML_status_output())
	status_filename = create_filename(prefs_get_outputdir(),"server",".xml"); // WarCraft III
    else
	status_filename = create_filename(prefs_get_outputdir(),"server",".dat"); // WarCraft III

    return;
} 

/* 
 * Write Functions *
 */

int output_standard_writer(FILE * fp)
{
    t_elem const	*		curr;
    t_connection	*		conn;
    t_channel const *		channel;
    t_game const	*		game;
    char const		*		channel_name;
    char const		*		game_name;
    char const		*		tname;
	int						number;
    
    if (prefs_get_XML_status_output())
    {
	fprintf(fp,"<?xml version=\"1.0\"?>\n<status>\n");
        fprintf(fp,"\t\t<Version>%s</Version>\n",PVPGN_VERSION);
	fprintf(fp,"\t\t<Uptime>%s</Uptime>\n",seconds_to_timestr(server_get_uptime()));
	fprintf(fp,"\t\t<Users>\n");
	fprintf(fp,"\t\t<Number>%d</Number>\n",connlist_login_get_length());

	LIST_TRAVERSE_CONST(connlist(),curr)
	{
	    conn = elem_get_data(curr);
	    if (conn_get_account(conn))
	    {
		tname = conn_get_username(conn);
		fprintf(fp,"\t\t<user><name>%s</name><clienttag>%s</clienttag><version>%s</version></user>\n",tname,clienttag_uint_to_str(conn_get_clienttag(conn)),vernum_to_verstr(conn_get_gameversion(conn)));
		conn_unget_username(conn,tname);
	    }
        }
	
	fprintf(fp,"\t\t</Users>\n");
	fprintf(fp,"\t\t<Games>\n");
	fprintf(fp,"\t\t<Number>%d</Number>\n",gamelist_get_length());
	
	LIST_TRAVERSE_CONST(gamelist(),curr)
	{
    	    game = elem_get_data(curr);
	    if (game_get_name(game)!=NULL)
	    {
		game_name = game_get_name(game);
		fprintf(fp,"\t\t<game><name>%s</name><clienttag>%s</clienttag></game>\n",game_name,game_get_clienttag(game));
	    }
	}

	fprintf(fp,"\t\t</Games>\n");
	fprintf(fp,"\t\t<Channels>\n");
	fprintf(fp,"\t\t<Number>%d</Number>\n",channellist_get_length());

	LIST_TRAVERSE_CONST(channellist(),curr)
	{
    	    channel = elem_get_data(curr);
    	    if (channel_get_name(channel)!=NULL)
	    {
		channel_name = channel_get_name(channel);
		fprintf(fp,"\t\t<channel>%s</channel>\n",channel_name);
	    }
	}
	
	fprintf(fp,"\t\t</Channels>\n");
	fprintf(fp,"</status>\n");
	return 0;
    }
    else
    {
	fprintf(fp,"[STATUS]\nVersion=%s\nUptime=%s\nGames=%d\nUsers=%d\nChannels=%d\nUserAccounts=%d\n",PVPGN_VERSION,seconds_to_timestr(server_get_uptime()),gamelist_get_length(),connlist_login_get_length(),channellist_get_length(),accountlist_get_length()); // Status
	fprintf(fp,"[CHANNELS]\n");
	number=1;
	LIST_TRAVERSE_CONST(channellist(),curr)
	{
    	    channel = elem_get_data(curr);
    	    if (channel_get_name(channel)!=NULL)
	    {
		channel_name = channel_get_name(channel);
		fprintf(fp,"channel%d=%s\n",number,channel_name);
		number++;
	    }
	}

	fprintf(fp,"[GAMES]\n");
	number=1;
	LIST_TRAVERSE_CONST(gamelist(),curr)
	{
    	    game = elem_get_data(curr);
	    if (game_get_name(game)!=NULL)
	    {
		game_name = game_get_name(game);
		fprintf(fp,"game%d=%s,%s\n",number,game_get_clienttag(game),game_name);
		number++;
	    }
	}

	fprintf(fp,"[USERS]\n");
	number=1;
	LIST_TRAVERSE_CONST(connlist(),curr)
	{
    	    conn = elem_get_data(curr);
    	    if (conn_get_account(conn))
	    {
		tname = conn_get_username(conn);
		fprintf(fp,"user%d=%s,%s\n",number,clienttag_uint_to_str(conn_get_clienttag(conn)),tname);
		conn_unget_username(conn,tname);
		number++;
	    }
	}
	
	return 0;
    }
}

extern int output_write_to_file(void)
{
    FILE * fp;
  
    if (!status_filename)
    {
	eventlog(eventlog_level_error,"ouput_write_to_file","got NULL filename");
	return -1;
    }
    
    if (!(fp = fopen(status_filename,"w")))
    {
        eventlog(eventlog_level_error,"ouput_write_to_file","could not open file \"%s\" for writing (fopen: %s)",status_filename,strerror(errno)); 
        return -1;
    }
    
    output_standard_writer(fp);
    fclose(fp);
    return 0;
}

extern void output_dispose_filename(void)
{
  if (status_filename) free(status_filename);
}
