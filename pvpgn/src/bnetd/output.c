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
#include "output.h"
#include "common/bnettime.h"
#include <errno.h>
#include <compat/strerror.h>
#include "common/eventlog.h"
#include "prefs.h"
#include "connection.h"
#include "common/list.h"
#include "game.h"
#include "war3ladder.h"
#include "common/util.h"
#include "server.h"
#include "channel.h"

char * war3_file   = "server";
char * str_end   = ".dat";
char * xml2_end   = ".xml";
char * status_filename;
/* 
 * Initialisation Output *
 */

extern void output_init(void)
{
  eventlog(eventlog_level_info,"output_init","initializing output file");
  if (prefs_get_XML_status_output_ladder()) {
  status_filename = create_filename(prefs_get_outputdir(),war3_file,xml2_end); // WarCraft III
  }
  else {
status_filename = create_filename(prefs_get_outputdir(),war3_file,str_end); // WarCraft III
  }
} 

/* 
 * Write Functions *
 */

int output_standard_writer(FILE * fp)
{
	  t_elem const * curr;
  t_connection * conn;
  t_channel const * channel;
  t_game const * game;
  char const *   channel_name;
  char const *   game_name;
  if (prefs_get_XML_status_output_ladder()) {
fprintf(fp,"<?xml version=\"1.0\"?>\n<status>\n");
fprintf(fp,"\t\t<Uptime>%s</Uptime>\n",seconds_to_timestr(server_get_uptime()));
fprintf(fp,"\t\t<Users>\n");
fprintf(fp,"\t\t<Number>%d</Number>\n",connlist_login_get_length());
 LIST_TRAVERSE_CONST(connlist(),curr)
  {
      conn = elem_get_data(curr);
      if (conn_get_account(conn))
	  {
		  char const * tname;

		fprintf(fp,"\t\t%s\n",(tname = conn_get_username(conn)));
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
	game_name = game_get_name(game);
	  fprintf(fp,"\t\t%s\n",game_name);
	  }
fprintf(fp,"\t\t</Games>\n");

fprintf(fp,"\t\t<Channels>\n");
fprintf(fp,"\t\t<Number>%d</Number>\n",channellist_get_length());
LIST_TRAVERSE_CONST(channellist(),curr)
  {
      channel = elem_get_data(curr);
      if (channel_get_name(channel)!=NULL)
	channel_name = channel_get_name(channel);
	  fprintf(fp,"\t\t%s\n",channel_name);
	  }
fprintf(fp,"\t\t</Channels>\n");
fprintf(fp,"</status>\n");
return 0;
  }
  else {
fprintf(fp,"[STATUS]\nUptime=%s\nGames=%d\nUsers=%d\nChannels=%d\n",seconds_to_timestr(server_get_uptime()),gamelist_get_length(),connlist_login_get_length(),channellist_get_length()); // Status
fprintf(fp,"[CHANNELS]\n");

LIST_TRAVERSE_CONST(channellist(),curr)
  {
      channel = elem_get_data(curr);
      if (channel_get_name(channel)!=NULL)
	channel_name = channel_get_name(channel);
	  fprintf(fp,"%s\n",channel_name);
	  }

  fprintf(fp,"[GAMES]\n");

LIST_TRAVERSE_CONST(gamelist(),curr)
  {
      game = elem_get_data(curr);
if (game_get_name(game)!=NULL)
	game_name = game_get_name(game);
	  fprintf(fp,"%s\n",game_name);
	  }

fprintf(fp,"[USERS]\n");
  LIST_TRAVERSE_CONST(connlist(),curr)
  {
      conn = elem_get_data(curr);
      if (conn_get_account(conn))
	  {
		  char const * tname;

		fprintf(fp,"%.16s\n",(tname = conn_get_username(conn)));
	  conn_unget_username(conn,tname);
	  }
  }
  return 0;
  }
}

extern int output_write_to_file(char const * filename)
{
  FILE * fp;
  
  if (!filename)
  {
    eventlog(eventlog_level_error,"ouput_write_to_file","got NULL filename");
    return -1;
  }
  
  if (!(fp = fopen(filename,"w")))
  { 
     eventlog(eventlog_level_error,"ouput_write_to_file","could not open file \"%s\" for writing (fopen: %s)",filename,strerror(errno)); 
     return -1;
  }
    output_standard_writer(fp);

  fclose(fp);
  return 0;
}

extern int output1_write_to_file()
{
  eventlog(eventlog_level_info,"output_write_to_file","flushing output to disk");
  output_write_to_file(status_filename); // Status
  return 0;
}

void output_dispose_filename(char * filename)
{
  if (filename) free(filename);
}