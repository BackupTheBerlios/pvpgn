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

/* amadeo */
#ifdef WIN32_GUI
#include <bnetd/winmain.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "common/packet.h"
#include "common/tag.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/list.h"
#include "anongame_maplists.h"
#include "common/setup_after.h"

static t_list * mapnames_war3[ANONGAME_TYPES];
static t_list * mapnames_w3xp[ANONGAME_TYPES];

static int _anongame_type_getid(const char * name);

/**********************************************************************************/
static int _anongame_type_getid(const char * name)
{
    if (strcmp(name, "1v1"  ) == 0) return ANONGAME_TYPE_1V1;
    if (strcmp(name, "2v2"  ) == 0) return ANONGAME_TYPE_2V2;
    if (strcmp(name, "3v3"  ) == 0) return ANONGAME_TYPE_3V3;
    if (strcmp(name, "4v4"  ) == 0) return ANONGAME_TYPE_4V4;
    if (strcmp(name, "sffa" ) == 0) return ANONGAME_TYPE_SMALL_FFA;
    if (strcmp(name, "tffa" ) == 0) return ANONGAME_TYPE_TEAM_FFA;
    if (strcmp(name, "at2v2") == 0) return ANONGAME_TYPE_AT_2V2;
    if (strcmp(name, "at3v3") == 0) return ANONGAME_TYPE_AT_3V3;
    if (strcmp(name, "at4v4") == 0) return ANONGAME_TYPE_AT_4V4;
    if (strcmp(name, "TY"   ) == 0) return ANONGAME_TYPE_TY;
    if (strcmp(name, "2v2v2") == 0) return ANONGAME_TYPE_2V2V2;
    return -1;
}

/**********************************************************************************/
extern int anongame_maplists_create(void)
{
   FILE *mapfd;
   char buffer[256];
   int len, i, type;
   char *p, *q, *r, *mapname, *u;
   t_list * * mapnames;
   int war3count, w3xpcount;

   if (prefs_get_mapsfile() == NULL) {
      eventlog(eventlog_level_error, "anongame_maplists_create","invalid mapsfile, check your config");
      return -1;
   }
   
   if ((mapfd = fopen(prefs_get_mapsfile(), "rt")) == NULL) {
      eventlog(eventlog_level_error, "anongame_maplists_create", "could not open mapsfile : \"%s\"", prefs_get_mapsfile());
      return -1;
   }
   
   /* init the maps, they say static vars are 0-ed anyway but u never know :) */
   for(i=0; i < ANONGAME_TYPES; i++) {
      mapnames_war3[i] = NULL;
      mapnames_w3xp[i] = NULL;
   }
   
   war3count = 0;
   w3xpcount = 0;
   while(fgets(buffer, 256, mapfd)) {
      len = strlen(buffer);
      if (len < 1) continue;
      if (buffer[len-1] == '\n') {
	 buffer[len-1] = '\0';
	 len--;
      }
      
      /* search for comments and comment them out */
      for(p = buffer; *p ; p++) 
	if (*p == '#') {
	   *p = '\0';
	   break;
	}
      
      /* skip spaces and/or tabs */
      for(p = buffer; *p && ( *p == ' ' || *p == '\t' ); p++);
      if (*p == '\0') continue;
      
      /* find next delimiter */
      for(q = p; *q && *q != ' ' && *q != '\t'; q++);
      if (*q == '\0') continue;
      
      *q = '\0';
      
      /* skip spaces and/or tabs */
      for (q++ ; *q && ( *q == ' ' || *q == '\t'); q++);
      if (*q == '\0') continue;
      
      /* find next delimiter */
      for (r = q+1; *r && *r != ' ' && *r != '\t'; r++);
      
      *r = '\0';
      
      /* skip spaces and/or tabs */
      for (r++ ; *r && ( *r == ' ' || *r == '\t'); r++);
      if (*r == '\0') continue;
      
      if (*r!='\"')
      /* find next delimiter */
        for (u = r+1; *u && *u != ' ' && *u != '\t'; u++);
      else
	{
	  r++;
          for (u = r+1; *u && *u != '\"'; u++);
	  if (*u!='\"')
	  {
	    eventlog(eventlog_level_error,__FUNCTION__,"missing \" at the end of the map name, presume it's ok anyway");
	  }
	}
      *u = '\0';
      
      if (strcmp(p, CLIENTTAG_WARCRAFT3) == 0) {
        if (++war3count > 255) continue;
	mapnames = mapnames_war3;
      }
      else if (strcmp(p, CLIENTTAG_WAR3XP) == 0) {
        if (++w3xpcount > 255) continue;
	mapnames = mapnames_w3xp;
      }
      else continue; /* invalid clienttag */

      if ((type = _anongame_type_getid(q)) < 0) continue; /* invalid game type */
      if (type >= ANONGAME_TYPES) {
	 eventlog(eventlog_level_error, "anongame_maplists_create", "invalid game type: %d", type);
	 anongame_maplists_destroy();
	 fclose(mapfd);
	 return -1;
      }

      if ((mapname = strdup(r)) == NULL) {
	 eventlog(eventlog_level_error, "anongame_maplists_create", "could not duplicate map name \"%s\"", r);
	 anongame_maplists_destroy();
	 fclose(mapfd);
	 return -1;
      }
      
      if (mapnames[type] == NULL) { /* uninitialized map name list */
	 if ((mapnames[type] = list_create()) == NULL) {
	    eventlog(eventlog_level_error, "anongame_maplists_create", "could not create list for type : %d", type);
	    free(mapname);
	    anongame_maplists_destroy();
	    fclose(mapfd);
	    return -1;
	 }
      }
      
      if (list_append_data(mapnames[type], mapname) < 0) {
	 eventlog(eventlog_level_error, "anongame_maplists_create" , "coould not add map to the list (map: \"%s\")", mapname);
	 free(mapname);
	 anongame_maplists_destroy();
	 fclose(mapfd);
	 return -1;
      }
      
      eventlog(eventlog_level_debug, "anongame_maplists_create", "loaded map: \"%s\" for \"%s\" of type \"%s\" : %d", mapname, p, q, type);
      
   }
   
   fclose(mapfd);
   
   return 0; // anongame_matchmaking_create(); disabled cause matchmaking format changed from 1.03 to 1.04
}

extern void anongame_maplists_destroy()
{
    t_elem * curr;
    char * mapname;
    int i;
   
    for(i = 0; i < ANONGAME_TYPES; i++) {
	if (mapnames_war3[i]) {
	    LIST_TRAVERSE(mapnames_war3[i], curr)
	    {
		if ((mapname = elem_get_data(curr)) == NULL)
		    eventlog(eventlog_level_error, "anongame_maplists_destroy", "found NULL mapname");
		else
		    free(mapname);
		
		list_remove_elem(mapnames_war3[i], curr);
	    }
	    list_destroy(mapnames_war3[i]);
	}
	mapnames_war3[i] = NULL;
	
	if (mapnames_w3xp[i]) {
	    LIST_TRAVERSE(mapnames_w3xp[i], curr)
	    {
		if ((mapname = elem_get_data(curr)) == NULL)
		    eventlog(eventlog_level_error, "anongame_maplists_destroy", "found NULL mapname");
		else
		    free(mapname);
	    
		list_remove_elem(mapnames_w3xp[i], curr);
	    }
	    list_destroy(mapnames_w3xp[i]);
        }
	mapnames_w3xp[i] = NULL;
    }
}

extern t_list * anongame_get_w3xp_maplist(int gametype, const char * clienttag)
{
    if ((gametype<0) || (gametype>=ANONGAME_TYPES)) {
	eventlog(eventlog_level_error,__FUNCTION__,"got invalid gametype for request");
	return NULL;
    }
    
    if (clienttag == NULL || strlen(clienttag) != 4) {
	eventlog(eventlog_level_error,__FUNCTION__,"got invalid clienttag");
	return NULL;
    }
    
    if (strcmp(clienttag, CLIENTTAG_WARCRAFT3) == 0)
	return mapnames_war3[gametype];
    
    if (strcmp(clienttag, CLIENTTAG_WAR3XP) == 0)
	return mapnames_w3xp[gametype];
	
    eventlog(eventlog_level_error, __FUNCTION__, "got unknown clienttag '%s'", clienttag);
    return NULL;
}

extern void anongame_add_maps_to_packet(t_packet * packet, int gametype, const char *clienttag)
{
    t_list * mapslist;
    t_elem * curr;
    char * mapname;
    
    if ((mapslist = anongame_get_w3xp_maplist(gametype, clienttag))) {
	LIST_TRAVERSE(mapslist, curr)
	{
	    if ((mapname = (char *)elem_get_data(curr)))
		packet_append_string(packet,mapname);
	    else
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL mapname, check your bnmaps.txt");
	}
    }
}

extern int anongame_add_tournament_map(char * ctag, char * mname)
{
    t_list * * mapnames;
    char *mapname;
	
    if (strcmp(ctag, CLIENTTAG_WARCRAFT3) == 0)
	mapnames = mapnames_war3;
    else if (strcmp(ctag, CLIENTTAG_WAR3XP) == 0)
        mapnames = mapnames_w3xp;
    else
	return -1; /* invalid clienttag */
    
    if ((mapname = strdup(mname)) == NULL)
	return -1;
    
    if (mapnames[ANONGAME_TYPE_TY] == NULL) { /* uninitialized map name list */
        if ((mapnames[ANONGAME_TYPE_TY] = list_create()) == NULL) {
    	    eventlog(eventlog_level_error,__FUNCTION__, "could not create list for type : %d", ANONGAME_TYPE_TY);
    	    free(mapname);
    	    return -1;
	}
    }
    
    if (list_append_data(mapnames[ANONGAME_TYPE_TY], mapname) < 0) {
        eventlog(eventlog_level_error,__FUNCTION__, "could not add map to the list (map: \"%s\")", mapname);
        free(mapname);
        return -1;
    }
    
    return 0;
}

extern void anongame_tournament_maplists_destroy(void)
{
    t_elem *curr;
    char *mapname;
   
    if (mapnames_war3[ANONGAME_TYPE_TY]) {
	LIST_TRAVERSE(mapnames_war3[ANONGAME_TYPE_TY], curr)
	{
	    if ((mapname = elem_get_data(curr)) == NULL)
		eventlog(eventlog_level_error,__FUNCTION__, "found NULL mapname");
	    else
	      free(mapname);
	    
	    list_remove_elem(mapnames_war3[ANONGAME_TYPE_TY], curr);
	}
	list_destroy(mapnames_war3[ANONGAME_TYPE_TY]);
	mapnames_war3[ANONGAME_TYPE_TY] = NULL;
    }
    
    if (mapnames_w3xp[ANONGAME_TYPE_TY]) {
	LIST_TRAVERSE(mapnames_w3xp[ANONGAME_TYPE_TY], curr)
	{
	    if ((mapname = elem_get_data(curr)) == NULL)
		eventlog(eventlog_level_error,__FUNCTION__, "found NULL mapname");
	    else
		free(mapname);
	    
	    list_remove_elem(mapnames_w3xp[ANONGAME_TYPE_TY], curr);
	}
	list_destroy(mapnames_w3xp[ANONGAME_TYPE_TY]);
	mapnames_w3xp[ANONGAME_TYPE_TY] = NULL;
    }
}
