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

#define VERSIONCHECK_INTERNAL_ACCESS
#include "common/setup_before.h"
#include <stdio.h>
#include <ctype.h>
// amadeo
#ifdef WIN32_GUI
#include <bnetd/winmain.h>
#endif
// NonReal:
#include <sys/stat.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS /* FIXME: remove ? */
# include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#endif
#ifdef WITH_BITS
# include "compat/memcpy.h"
#endif
#ifdef WIN32
# include "compat/socket.h"
#endif
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "compat/strchr.h"
#include "compat/strdup.h"
#include "common/packet.h"
#include "common/bnet_protocol.h"
#include "common/tag.h"
#include "message.h"
#include "common/eventlog.h"
#include "command.h"
#include "account.h"
#include "connection.h"
#include "channel.h"
#include "game.h"
#include "common/queue.h"
#include "tick.h"
#include "file.h"
#include "prefs.h"
#include "common/bnethash.h"
#include "common/bnethashconv.h"
#include "common/bn_type.h"
#include "common/field_sizes.h"
#include "ladder.h"
#include "common/list.h"
#include "common/bnettime.h"
#include "common/addr.h"
#ifdef WITH_BITS
# include "query.h"
# include "bits.h"
# include "bits_login.h"
# include "bits_va.h"
# include "bits_query.h"
# include "bits_packet.h"
# include "bits_game.h"
#endif
#include "game_conv.h"
#include "gametrans.h"
#include "autoupdate.h"
#include "realm.h"
#include "character.h"
#include "versioncheck.h"
#include "common/proginfo.h"
#include "handle_bnet.h"
#include "anongame.h"
#include "timer.h"
#include "common/setup_after.h"
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include "compat/netinet_in.h"
#include "watch.h"
#include "war3ladder.h"

#define MAX_LEVEL 100

static t_list * mapnames[ANONGAME_TYPES];
// [quetzal] 20020827 - this one get modified by anongame_queue player when there're enough
// players and map has been chosen based on their preferences. otherwise its NULL
static char *mapname = NULL;

static int players[ANONGAME_TYPES] = {0,0,0,0,0,0,0,0,0};
static t_connection * player[ANONGAME_TYPES][8];

// [quetzal] 20020815 - queue to hold matching players
static t_list * matchlists[ANONGAME_TYPES][MAX_LEVEL];

static int _anongame_type_getid(const char *name)
{
	if (strcmp(name, "1v1") == 0) return ANONGAME_TYPE_1V1;
	if (strcmp(name, "2v2") == 0) return ANONGAME_TYPE_2V2;
	if (strcmp(name, "3v3") == 0) return ANONGAME_TYPE_3V3;
	if (strcmp(name, "4v4") == 0) return ANONGAME_TYPE_4V4;
	if (strcmp(name, "sffa") == 0) return ANONGAME_TYPE_SMALL_FFA;
	if (strcmp(name, "tffa") == 0) return ANONGAME_TYPE_TEAM_FFA;
	if (strcmp(name, "at2v2") == 0) return ANONGAME_TYPE_AT_2V2;
	if (strcmp(name, "at3v3") == 0) return ANONGAME_TYPE_AT_3V3;
	if (strcmp(name, "at4v4") == 0) return ANONGAME_TYPE_AT_4V4;

	return -1;
}

extern int anongame_matchlists_create()
{
	int i, j;

	for (i = 0; i < ANONGAME_TYPES; i++) {
		for (j = 0; j < MAX_LEVEL; j++) {
			matchlists[i][j] = NULL;
		}
	}
	return 0;
}

extern int anongame_matchlists_destroy()
{
	int i, j;
	for (i = 0; i < ANONGAME_TYPES; i++) {
		for (j = 0; j < MAX_LEVEL; j++) {
			if (matchlists[i][j]) {
				list_destroy(matchlists[i][j]);
			}
		}
	}
	return 0;
}

/*
extern int anongame_matchmaking_process_map(FILE *f, int gametype, char *title, char *desc)
{
	t_uint8 i8;
	t_uint32 i32 = 0;

	t_elem *curr;

	if (!list_get_length(mapnames[gametype])) {
		eventlog(eventlog_level_debug, "anongame_matchmaking_process_map", 
			"no maps found for gametype = %d", gametype);
		return -1;
	}

	if (!anongame_arranged(gametype)) {
		i8 = gametype;
		fwrite(&i8, sizeof(i8), 1, f);
	} else {
		t_uint16 i16 = 1;
		i8 = anongame_totalplayers(gametype) / 2;
		fwrite(&i8, sizeof(i8), 1, f);
		fwrite(&i16, sizeof(i16), 1, f);
	}
	fwrite(title, strlen(title) + 1, 1, f);

	i8 = list_get_length(mapnames[gametype]);
	if (i8 > 0) {
		fwrite(&i8, sizeof(i8), 1, f);
		LIST_TRAVERSE(mapnames[gametype], curr) {
			fwrite(elem_get_data(curr), strlen(elem_get_data(curr)) + 1, 1, f);
			fwrite(&i32, sizeof(i32), 1, f);
			eventlog(eventlog_level_info, "anongame_matchmaking_process_map", 
				"Adding map: %p", elem_get_data(curr));
		}
	}
	fwrite(desc, strlen(desc) + 1, 1, f);
	return 0;
}

extern int anongame_matchmaking_create(void)
{
	static char title_1v1[]		= "One vs. One";
	static char desc_1v1[]		= "Two players fight to the death.";
	static char title_2v2[]		= "Two vs. Two";
	static char desc_2v2[]		= "Two teams of two vie for dominance.";
	static char title_3v3[]		= "Three vs. Three";
	static char desc_3v3[]		= "Two teams of three face off on the battlefield.";
	static char title_4v4[]		= "Four vs. Four";
	static char desc_4v4[]		= "Two teams of four head to battle.";
	static char title_sffa[]	= "Small Free for All";
	static char desc_sffa[]		= "Can you defeat 3-5 opponents alone?";

	FILE *f = NULL;
	t_uint32 i32;
	t_uint16 i16;
	t_uint8 i8;

	char fname[256];

	int i;

	strcpy(fname, prefs_get_filedir());
	if (fname[strlen(fname) - 1] != '/') {
		strcat(fname, "/");
	}
	strcat(fname, "matchmaking-war3-default.dat");

	f = fopen(fname, "wb");
	if (!f) {
		eventlog(eventlog_level_error, "anongame_matchmaking_create", 
			"could not create matchmaking file: %s", fname);
		return -1;
	}

	i32 = 2;
	fwrite(&i32, sizeof(i32), 1, f);
	i16 = 0;
	i8 = 0;
	for (i = 0; i < ANONGAME_TYPES; i++) {
		if (list_get_length(mapnames[i])) {
			if (!anongame_arranged(i)) i8++;
			else i16++;
		}
	}

	if (!i16 || !i8) {
		eventlog(eventlog_level_fatal, "anongame_matchmaking_create", "Unable to find maps for at least one PG and one AT gametype");
		fclose(f);
		return -1;
	}
	// dunno why but it should be 4
	i16 = 4;
	fwrite(&i16, sizeof(i16), 1, f);
	fwrite(&i8, sizeof(i8), 1, f);
	
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_1V1, title_1v1, desc_1v1);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_2V2, title_2v2, desc_2v2);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_3V3, title_3v3, desc_3v3);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_4V4, title_4v4, desc_4v4);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_SMALL_FFA, title_sffa, desc_sffa);

	anongame_matchmaking_process_map(f, ANONGAME_TYPE_AT_2V2, title_2v2, desc_2v2);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_AT_3V3, title_3v3, desc_3v3);
	anongame_matchmaking_process_map(f, ANONGAME_TYPE_AT_4V4, title_4v4, desc_4v4);

	fclose(f);
	return 0;
}
*/

extern int anongame_maplists_create(void)
{
	FILE *mapfd;
	char buffer[256];
	int len, i, type;
	char *p, *q, *r, *mapname;

	if (prefs_get_mapsfile() == NULL) {
		eventlog(eventlog_level_error, "anongame_maplists_create","invalid mapsfile, check your config");
		return -1;
	}

	if ((mapfd = fopen(prefs_get_mapsfile(), "rt")) == NULL) {
		eventlog(eventlog_level_error, "anongame_maplists_create", "could not open mapsfile : \"%s\"", prefs_get_mapsfile());
		return -1;
	}

	/* init the maps, they say static vars are 0-ed anyway but u never know :) */
	for(i=0; i < ANONGAME_TYPES; i++)
		mapnames[i] = NULL;

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

			if ((type = _anongame_type_getid(p)) < 0) continue; /* invalid game type */
			if (type >= ANONGAME_TYPES) {
				eventlog(eventlog_level_error, "anongame_maplists_create", "invalid game type: %d", type);
				anongame_maplists_destroy();
				fclose(mapfd);
				return -1;
			}

			if ((mapname = strdup(q)) == NULL) {
				eventlog(eventlog_level_error, "anongame_maplists_create", "could not duplicate map name \"%s\"", q);
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

			eventlog(eventlog_level_debug, "anongame_maplists_create", "loaded map: \"%s\" of type \"%s\" : %d", mapname, p, type);

	}

	fclose(mapfd);

	return 0; // anongame_matchmaking_create(); disabled cause matchmaking format changed from 1.03 to 1.04
}

extern void anongame_maplists_destroy()
{
	t_elem *curr;
	char *mapname;
	int i;

	for(i = 0; i < ANONGAME_TYPES; i++) {
		if (mapnames[i]) {
			LIST_TRAVERSE(mapnames[i], curr) {
				if ((mapname = elem_get_data(curr)) == NULL)
					eventlog(eventlog_level_error, "anongame_maplists_destroy", "found NULL mapname");
				else
					free(mapname);

				list_remove_elem(mapnames[i], curr);
			}
			list_destroy(mapnames[i]);
		}
		mapnames[i] = NULL;
	}
}

/* Function not called anymore - CreepLord
static char const * anongame_maplists_getmap(t_list *list, unsigned int idx)
{
	if (list == NULL) return NULL;
	if (idx >= list_get_length(list)) return NULL;

	return (char const *)list_get_data_by_pos(list, idx);
}
*/

extern int anongame_totalplayers(t_uint8 gametype)
{
	switch(gametype) {
	case ANONGAME_TYPE_1V1:
		return 2;
	case ANONGAME_TYPE_2V2:
	case ANONGAME_TYPE_AT_2V2:
	case ANONGAME_TYPE_SMALL_FFA:
		return 4;
	case ANONGAME_TYPE_3V3:
	case ANONGAME_TYPE_AT_3V3:
		return 6;
	case ANONGAME_TYPE_4V4:
	case ANONGAME_TYPE_AT_4V4:
	case ANONGAME_TYPE_TEAM_FFA:
		return 8;
	default:
		eventlog(eventlog_level_error, "anongame_totalplayers", "unknown gametype: %d", (int)gametype);
		return 0;
	}
}

extern char anongame_arranged(t_uint8 gametype)
{
	switch(gametype) {
	case ANONGAME_TYPE_AT_2V2:
	case ANONGAME_TYPE_AT_3V3:
	case ANONGAME_TYPE_AT_4V4:
		return 1;
	default:
		return 0;
	}
}


int _anongame_level_by_gametype(t_connection *c, t_uint8 gametype) 
{
	if(gametype >= ANONGAME_TYPES) {
		eventlog(eventlog_level_fatal, "_anongame_level_by_gametype", "unknown gametype: %d", (int)gametype);
		return -1;
	}

	switch(gametype) {
		case ANONGAME_TYPE_1V1:
			return account_get_sololevel(conn_get_account(c));
		case ANONGAME_TYPE_2V2:
		case ANONGAME_TYPE_3V3:
		case ANONGAME_TYPE_4V4:
			return account_get_teamlevel(conn_get_account(c));
		case ANONGAME_TYPE_SMALL_FFA:
		case ANONGAME_TYPE_TEAM_FFA:
			return account_get_ffalevel(conn_get_account(c));
		case ANONGAME_TYPE_AT_2V2:
		case ANONGAME_TYPE_AT_3V3:
		case ANONGAME_TYPE_AT_4V4:
			return 0;
		default:
			break;
	}
	return -1;
}

// [quetzal] 20020815
char *_get_map_from_prefs(int gametype, t_uint32 cur_prefs)
{
	int i, j = 0;

	static char default_map[] = "Maps\\(8)PlainsOfSnow.w3m";

	char *res_maps[32];
	for (i = 0; i < 32; i++) res_maps[i] = NULL;

	for (i = 0; i < 32; i++) {
		if (cur_prefs & 1) {
			res_maps[j++] = list_get_data_by_pos(mapnames[gametype], i);
		}
		cur_prefs >>= 1;
	}
	i = rand() % j;
	eventlog(eventlog_level_debug, "_get_map_from_prefs", "got map %s from prefs", 
		res_maps[i]);
	if (res_maps[i]) return res_maps[i];
	else return default_map;
}

static int anongame_queue_player(t_connection * c, t_uint8 gametype, t_uint32 map_prefs)
{
	int level;
	t_elem *curr;
	t_matchdata *md;
	t_atcountinfo acinfo[100];
	int delta, i;
	t_uint32 cur_prefs = 0xffffffff;

	if (!c) {
		eventlog(eventlog_level_fatal, "anongame_queue_player", "got NULL connection");
	}

	if(gametype >= ANONGAME_TYPES) {
		eventlog(eventlog_level_fatal, "anongame_queue_player", "unknown gametype: %d", (int)gametype);
		return -1;
	}

	level = _anongame_level_by_gametype(c, gametype);

	if (!matchlists[gametype][level]) {
		matchlists[gametype][level] = list_create();
	}

	md = malloc(sizeof(t_matchdata));
	md->c = c;
	md->map_prefs = map_prefs;
	md->versiontag = strdup(versioncheck_get_versiontag(conn_get_versioncheck(c)));

	list_append_data(matchlists[gametype][level], md);

	delta = 0;
	players[gametype] = 0;

	if (anongame_arranged(gametype)) {
		////////////////////////////////////////////////////////////////////////////
		// match AT players ////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////
		int i, team1_id = 0, team2_id = 0;

		for (i = 0; i < 100; i++) {
			acinfo[i].atid = 0;
			acinfo[i].count = 0;
			acinfo[i].map_prefs = 0xffffffff;
		}

		eventlog(eventlog_level_debug, "anongame_queue_player", "[%d] AT matching for gametype %d, gathering teams (%d players available)",
			conn_get_socket(c),
			(int)gametype,
			list_get_length(matchlists[gametype][level]));

		// find out how many players in each team available
		LIST_TRAVERSE(matchlists[gametype][level], curr) {
			md = elem_get_data(curr);
			if (conn_get_atid(md->c) == 0) {
				eventlog(eventlog_level_error, "anongame_queue_player", 
					"AT matching, got zero arranged team id");
			} else {
			    if (!strcmp(md->versiontag, versioncheck_get_versiontag(conn_get_versioncheck(c)))) {
				for (i = 0; i < 100; i++) {
					if (acinfo[i].atid == conn_get_atid(md->c)) {
						acinfo[i].map_prefs &= md->map_prefs;
						acinfo[i].count++;
						break;
					} else if (acinfo[i].atid == 0) {
						acinfo[i].atid = conn_get_atid(md->c);
						acinfo[i].map_prefs &= md->map_prefs;
						acinfo[i].count = 1;
						break;
					}
				}
			    }
			}
		}

		// take first 2 teams with enough players
		for (i = 0; i < 100; i++) {
			if (!acinfo[i].atid) break;

			eventlog(eventlog_level_debug, "anongame_queue_player",
				"AT count information entry %d: id = %X, count = %X", 
				i, acinfo[i].atid, acinfo[i].count);
			// found team1
			if (acinfo[i].count >= anongame_totalplayers(gametype) / 2 && !team1_id
				&& cur_prefs & acinfo[i].map_prefs) {
				cur_prefs &= acinfo[i].map_prefs;
				eventlog(eventlog_level_debug, "anongame_queue_player", 
					"AT matching, found one team, teamid = %X", acinfo[i].atid);
				team1_id = acinfo[i].atid;
				// found team2
			} else if (acinfo[i].count >= anongame_totalplayers(gametype) / 2 && !team2_id 
				&& cur_prefs & acinfo[i].map_prefs) {
				cur_prefs &= acinfo[i].map_prefs;
				eventlog(eventlog_level_debug, "anongame_queue_player", 
					"AT matching, found other team, teamid = %X", acinfo[i].atid);
				team2_id = acinfo[i].atid;
			}
			// both teams found
			if (team1_id && team2_id) {
				int j = 0, k = 0;
				// add team1 first
				LIST_TRAVERSE(matchlists[gametype][level], curr) {
					md = elem_get_data(curr);
					if (team1_id == conn_get_atid(md->c)) {
						player[gametype][2*(j++)] = md->c;
					}
				}
				// and then team2
				LIST_TRAVERSE(matchlists[gametype][level], curr) {
					md = elem_get_data(curr);
					if (team2_id == conn_get_atid(md->c)) {
						player[gametype][1+2*(k++)] = md->c;
					}
				}
				players[gametype] = j + k;
				// added enough players? remove em from queue and quit
				if (players[gametype] == anongame_totalplayers(gametype)) {
					eventlog(eventlog_level_debug, "anongame_queue_player", 
						"AT Found enough players in both teams (%d), calling unqueue", 
						players[gametype]);
					for (i = 0; i < players[gametype]; i++) {
						anongame_unqueue_player(player[gametype][i], gametype);
					}
					mapname = _get_map_from_prefs(gametype, cur_prefs);
					return 0;
				}
				eventlog(eventlog_level_error, "anongame_get_player", "Found teams, but was unable to form players array");
			}
		}
		eventlog(eventlog_level_debug, "anongame_queue_player", "[%d] AT matching finished, not enough players",
			conn_get_socket(c));
	} else {
		////////////////////////////////////////////////////////////////////////////
		// match PG players ////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////
		eventlog(eventlog_level_debug, "anongame_queue_player", "[%d] Matching started for level %d player", 
			conn_get_socket(c), level);
		while (abs(delta) < 7) 
		{
			eventlog(eventlog_level_debug, "anongame_queue_player", 
				"Traversing level %d players", level + delta);

			LIST_TRAVERSE(matchlists[gametype][level + delta], curr) {
				md = elem_get_data(curr);
				if (!strcmp(md->versiontag, versioncheck_get_versiontag(conn_get_versioncheck(c)))) {
				    if (cur_prefs & md->map_prefs) {
					cur_prefs &= md->map_prefs;
					player[gametype][players[gametype]++] = md->c;
					if (players[gametype] == anongame_totalplayers(gametype)) {
						eventlog(eventlog_level_debug, "anongame_queue_player", 
							"Found enough players (%d), calling unqueue", players[gametype]);
						for (i = 0; i < players[gametype]; i++) {
							anongame_unqueue_player(player[gametype][i], gametype);
						}
						mapname = _get_map_from_prefs(gametype, cur_prefs);
						return 0;
					}
				    }
				}
			}

			if (delta <= 0 || level - delta < 0) {
				delta = abs(delta) + 1;
			} else {
				delta = -delta;
			}

			if (level + delta > MAX_LEVEL) {
				delta = -delta;
			}

			if (level + delta < 0) break; // cant really happen
		}
		eventlog(eventlog_level_debug, "anongame_queue_player", "[%d] Matching finished, not enough players (found %d)", 
			conn_get_socket(c), players[gametype]);
	}
	mapname = NULL;
	return 0;
}

// [quetzal] 20020815
extern int anongame_unqueue_player(t_connection * c, t_uint8 gametype)
{
	int i;
	t_elem *curr;
	t_matchdata *md;

	if(gametype >= ANONGAME_TYPES) {
		eventlog(eventlog_level_fatal, "anongame_unqueue_player", "unknown gametype: %d", (int)gametype);
		return -1;
	}

	for (i = 0; i < MAX_LEVEL; i++) {
		if (matchlists[gametype][i] == NULL) continue;

		LIST_TRAVERSE(matchlists[gametype][i], curr)
		{
			md = elem_get_data(curr);
			if (md->c == c) {
				eventlog(eventlog_level_debug, "anongame_unqueue_player", "unqueued player [%d] level %d", 
					conn_get_socket(c), i);
				list_remove_elem(matchlists[gametype][i], curr);
				free(md);
				return 0;
			}
		}
	}

	eventlog(eventlog_level_debug, "anongame_unqueue_player", "[%d] player not found in queue", 
		conn_get_socket(c));

	return -1;
}

extern int anongame_unqueue_team(t_connection *c, t_uint8 gametype)
{
	int id, i;
	t_elem *curr;
	t_matchdata *md;
	
	if(gametype >= ANONGAME_TYPES) {
		eventlog(eventlog_level_fatal, "anongame_unqueue_team", "unknown gametype: %d", (int)gametype);
		return -1;
	}
	if (!c) {
		eventlog(eventlog_level_fatal, "anongame_unqueue_team", "got NULL connection");
		return -1;
	}

	id = conn_get_atid(c);
	
	for (i = 0; i < MAX_LEVEL; i++) {
		if (matchlists[gametype][i] == NULL) continue;

		LIST_TRAVERSE(matchlists[gametype][i], curr) {
			md = elem_get_data(curr);
			if (id == conn_get_atid(md->c)) {
				eventlog(eventlog_level_debug, "anongame_unqueue_team", "unqueued player [%d] level %d", 
					conn_get_socket(md->c), i);
				list_remove_elem(matchlists[gametype][i], curr);
			}
		}
	}
	return 0;
}

// [zap-zero] 20020527
static int w3routeip = -1; /* changed by dizzy to show the w3routeshow addr if available */
static unsigned short w3routeport = 6200;

extern void handle_anongame_search(t_connection * c, t_packet const * packet)
{
	t_packet * rpacket = NULL;
	int Count;
	int temp;
	t_uint32 race;
	t_anongame *a = NULL; 
	/* const char * map = "Maps\\(8)Battleground.w3m"; */
	t_uint8 option, gametype;
	int i, j;
	t_anongameinfo *info;
	t_uint32 map_prefs;

	option = bn_byte_get(packet->u.client_findanongame.option);

	if(option==CLIENT_FINDANONGAME_AT_INVITER_SEARCH) {
		t_uint8 teamsize = bn_byte_get(packet->u.client_findanongame_at_inv.teamsize);
		map_prefs = bn_int_get(packet->u.client_findanongame_at_inv.map_prefs);
		switch(teamsize) {
		case 2:
			gametype = ANONGAME_TYPE_AT_2V2;
			break;
		case 3:
			gametype = ANONGAME_TYPE_AT_3V3;
			break;
		case 4:
			gametype = ANONGAME_TYPE_AT_4V4;
			break;
		default:
			eventlog(eventlog_level_error,"handle_anongame_search","invalid AT team size: %d",(int)teamsize);
			return;
		}
	} else if(option==CLIENT_FINDANONGAME_AT_SEARCH) {
		gametype = bn_byte_get(packet->u.client_findanongame_at.gametype);
		map_prefs = 0xffffffff;
		switch(gametype) {
		case ANONGAME_TYPE_2V2:
			gametype = ANONGAME_TYPE_AT_2V2;
			break;
		case ANONGAME_TYPE_3V3:
			gametype = ANONGAME_TYPE_AT_3V3;
			break;
		case ANONGAME_TYPE_4V4:
			gametype = ANONGAME_TYPE_AT_4V4;
			break;
		default:
			eventlog(eventlog_level_error,"handle_anongame_search","invalid AT game type: %d",(int)gametype);
			return;
		}

	} else {
		gametype = bn_byte_get(packet->u.client_findanongame.gametype);
		map_prefs = bn_int_get(packet->u.client_findanongame.map_prefs);
	}

	if(gametype >= ANONGAME_TYPES) {
		eventlog(eventlog_level_error,"handle_anongame_search","invalid game type %d",(int)gametype);
		return;
	}

	eventlog(eventlog_level_trace, "handle_anongame_search", "gametype: %d", (int)gametype);

	/*if (mapnames[gametype] != NULL) {
		if ((map = anongame_maplists_getmap(mapnames[gametype], Map[gametype])) == NULL) {
			eventlog(eventlog_level_error, "handle_anongame_search", "could not get mapname for gametype : %d", gametype);
			return;
		}
		eventlog(eventlog_level_trace,"handle_anongame_search", "selected map: \"%s\"", map);
		if (++Map[gametype] >= list_get_length(mapnames[gametype]))
			Map[gametype] = 0;
	}*/

	if(!(a = conn_get_anongame(c)))
		a = conn_create_anongame(c);

	if(!a) {
		eventlog(eventlog_level_error,"handle_anongame_search","[%d] conn_create_anongame failed",conn_get_socket(c));
		return;
	}

	if(option==CLIENT_FINDANONGAME_AT_INVITER_SEARCH) {
		anongame_set_id(a,bn_int_get(packet->u.client_findanongame_at_inv.id));
		race = bn_int_get(packet->u.client_findanongame_at_inv.race);
	} else if(option==CLIENT_FINDANONGAME_AT_SEARCH) {
		anongame_set_id(a,bn_int_get(packet->u.client_findanongame_at.id));
		race = bn_int_get(packet->u.client_findanongame_at.race);
	} else {
		anongame_set_id(a,bn_int_get(packet->u.client_findanongame.id));
		race = bn_int_get(packet->u.client_findanongame.race);
	}


	anongame_set_race(a,race);
	account_set_w3pgrace(conn_get_account(c), race);

	Count = bn_int_get(packet->u.client_findanongame.count);
	anongame_set_count(a,Count);

	if (!(rpacket = packet_create(packet_class_bnet)))
		return;
	packet_set_size(rpacket,sizeof(t_server_playgame_ack));
	packet_set_type(rpacket,SERVER_PLAYGAME_ACK);
	bn_byte_set(&rpacket->u.server_playgame_ack.playgameack1,SERVER_PLAYGAME_ACK1);
	bn_int_set(&rpacket->u.server_playgame_ack.count,Count);
	bn_int_set(&rpacket->u.server_playgame_ack.playgameack2,SERVER_PLAYGAME_ACK2);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);

	anongame_set_type(a, gametype);
	if(anongame_queue_player(c, gametype, map_prefs) < 0) {
		eventlog(eventlog_level_error,"handle_anongame_search","queue failed");
		return;
	}

	eventlog(eventlog_level_trace, "handle_anongame_search", "totalplayers for type %d: %d", (int)gametype, anongame_totalplayers(gametype));

	// wait till enough players are queued
	if(players[gametype] < anongame_totalplayers(gametype))
		return;

	// FIXME: maybe periodically lookup w3routeaddr to support dynamic ips?
	// (or should dns lookup be even quick enough to do it everytime?)
	if(w3routeip==-1) {
		t_addr * routeraddr;

		if (prefs_get_w3route_show()) routeraddr = addr_create_str(prefs_get_w3route_show(), 0, 6113);
		else routeraddr = addr_create_str(prefs_get_w3route_addr(), 0, 6113);

		eventlog(eventlog_level_trace,"handle_anongame_search","[%d] setting w3routeip, addr from prefs", conn_get_socket(c));

		if(!routeraddr) {
			eventlog(eventlog_level_error,"handle_anongame_search","[%d] error getting w3route_addr",conn_get_socket(c));
			return;
		}

		w3routeip = htonl(addr_get_ip(routeraddr));
		w3routeport = addr_get_port(routeraddr);
		addr_destroy(routeraddr);
	}

	info = anongameinfo_create(anongame_totalplayers(gametype));
	if(!info) {
		eventlog(eventlog_level_error,"handle_anongame_search","[%d] anongameinfo_create failed",conn_get_socket(c));
		return;
	}
	for(i=0; i<players[gametype]; i++) {
		if(!(a = conn_get_anongame(player[gametype][i]))) {
			eventlog(eventlog_level_error,"handle_anongame_search","[%d] no anongame struct for queued player",conn_get_socket(c));
			return;
		}
		anongame_set_info(a, info);
		eventlog(eventlog_level_debug, "handle_anongame_search", "anongame_get_totalplayers: %d", anongame_get_totalplayers(a));
		anongame_set_playernum(a, i+1);
		for(j=0; j<players[gametype]; j++)
			anongame_set_player(a, j, player[gametype][j]);

		if (!(rpacket = packet_create(packet_class_bnet)))
			return;
		packet_set_size(rpacket,sizeof(t_server_anongame_found));
		packet_set_type(rpacket,SERVER_ANONGAME_FOUND);
		bn_byte_set(&rpacket->u.server_anongame_found.type,1);
		bn_int_set(&rpacket->u.server_anongame_found.count,anongame_get_count(conn_get_anongame(player[gametype][i])));
		bn_int_set(&rpacket->u.server_anongame_found.unknown1,0);
		bn_int_set(&rpacket->u.server_anongame_found.ip,w3routeip);
		bn_short_set(&rpacket->u.server_anongame_found.port,w3routeport);
		bn_byte_set(&rpacket->u.server_anongame_found.numplayers,anongame_totalplayers(gametype));
		bn_byte_set(&rpacket->u.server_anongame_found.playernum, i+1);
		bn_byte_set(&rpacket->u.server_anongame_found.gametype,gametype);
		bn_byte_set(&rpacket->u.server_anongame_found.unknown2,0);
		bn_int_set(&rpacket->u.server_anongame_found.id,0xdeadbeef);
		bn_byte_set(&rpacket->u.server_anongame_found.unknown4,6);
		bn_short_set(&rpacket->u.server_anongame_found.unknown5,0);
		if (!mapname) {
			eventlog(eventlog_level_fatal, "handle_anongame_search", 
				"got all players, but there's no map to play on");
		}	else {
			eventlog(eventlog_level_info, "handle_anongame_search", 
				"selected map: %s", mapname);
		}
		packet_append_string(rpacket, mapname); 
		temp=-1;
		packet_append_data(rpacket, &temp, 4);

		// team num
		{
		/* dizzy: this changed in 1.05 */
		    int gametype_tab [] = { SERVER_ANONGAME_SOLO_STR,
					    SERVER_ANONGAME_TEAM_STR,
					    SERVER_ANONGAME_TEAM_STR,
					    SERVER_ANONGAME_TEAM_STR,
					    SERVER_ANONGAME_SFFA_STR,
					    SERVER_ANONGAME_AT2v2_STR,
					    0, /* Team FFA is no longer supported */
					    SERVER_ANONGAME_AT3v3_STR,
					    SERVER_ANONGAME_AT4v4_STR };
		    if (gametype > 8) {
			eventlog(eventlog_level_error, "handle_anongame_search", "invalid gametype (%d)", gametype);
			temp = 0;
		    } else temp = gametype_tab[gametype];
		}
		packet_append_data(rpacket, &temp, 4);

		// total players
		temp=anongame_totalplayers(gametype);
		packet_append_data(rpacket, &temp, 1);

		if(gametype == ANONGAME_TYPE_1V1 || gametype == ANONGAME_TYPE_SMALL_FFA)
			temp = 0;
		else
			temp = 2;

		packet_append_data(rpacket, &temp, 1);

		temp=0;
		packet_append_data(rpacket, &temp, 2);
		temp=0x02; // visibility. 0x01 - dark 0x02 - default
		packet_append_data(rpacket, &temp, 1);
		temp=0x02;
		packet_append_data(rpacket, &temp, 1);

		queue_push_packet(conn_get_out_queue(player[gametype][i]),rpacket);
		packet_del_ref(rpacket);
	}

	// clear queue
	players[gametype] = 0;
}


extern int handle_w3route_packet(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket=NULL;
   t_connection * gamec=NULL;
   char const * username;
   t_anongame * a = NULL;
   t_uint8 gametype, plnum;
   int tp, i;
   
   if(!c) {
      eventlog(eventlog_level_error,"handle_w3route_packet","[%d] got NULL connection",conn_get_socket(c));
      return -1;
   }
   if(!packet) {
      eventlog(eventlog_level_error,"handle_w3route_packet","[%d] got NULL packet",conn_get_socket(c));
      return -1;
   }
   if(packet_get_class(packet)!=packet_class_w3route) {
      eventlog(eventlog_level_error,"handle_w3route_packet","[%d] got bad packet (class %d)",conn_get_socket(c),packet_get_class(packet));
      return -1;
   }
   if(conn_get_state(c) != conn_state_connected) {
      eventlog(eventlog_level_error,"handle_w3route_packet","[%d] not connected",conn_get_socket(c));
      return -1;
   }
   
   // init route connection
   if(packet_get_type(packet) == CLIENT_W3ROUTE_REQ) {
      t_connection * oldc;
      
      eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] sizeof t_client_w3route_req %d", conn_get_socket(c), sizeof(t_client_w3route_req));
      username = packet_get_str_const(packet,sizeof(t_client_w3route_req),USER_NAME_MAX);
      eventlog(eventlog_level_info,"handle_w3route_packet","[%d] got username '%s'",conn_get_socket(c),username);
      gamec = connlist_find_connection_by_accountname(username);
      
      if(!gamec) {
	 eventlog(eventlog_level_info,"handle_w3route_packet","[%d] no game connection found for this w3route connection; closing",conn_get_socket(c));
	 conn_set_state(c, conn_state_destroy);
	 return 0;
      }
      
      if(!(a = conn_get_anongame(gamec))) {
	 eventlog(eventlog_level_info,"handle_w3route_packet","[%d] no anongame struct for game connection",conn_get_socket(c));
	 conn_set_state(c, conn_state_destroy);
	 return 0;
      }
      
      if(bn_int_get((unsigned char const *)packet->u.data+sizeof(t_client_w3route_req)+strlen(username)+2) != anongame_get_id(a)) {
	 eventlog(eventlog_level_info,"handle_w3route_packet","[%d] client sent wrong id for user '%s', closing connection",conn_get_socket(c),username);
	 conn_set_state(c, conn_state_destroy);
	 return 0;
      }
      
      oldc = conn_get_routeconn(gamec);
      if(oldc) {
	 conn_set_routeconn(oldc, NULL);
	 conn_set_state(oldc, conn_state_destroy);
      }
      
      if(conn_set_routeconn(c, gamec) < 0 || conn_set_routeconn(gamec, c) < 0) {
	 eventlog(eventlog_level_error,"handle_w3route_packet","[%d] conn_set_routeconn failed",conn_get_socket(c));
	 return -1;
      }
      
      anongame_set_addr(a, bn_int_get((unsigned char const *)packet->u.data+sizeof(t_client_w3route_req)+strlen(username)+2+12));
      anongame_set_joined(a, 0);
      anongame_set_loaded(a, 0);
      anongame_set_result(a, -1);
      
      anongame_set_handle(a, bn_int_get(packet->u.client_w3route_req.handle));
      
      if(!(rpacket = packet_create(packet_class_w3route))) {
	 eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
	 return -1;
      }
      
      packet_set_size(rpacket,sizeof(t_server_w3route_ack));
      packet_set_type(rpacket,SERVER_W3ROUTE_ACK);
      bn_byte_set(&rpacket->u.server_w3route_ack.unknown1,7);
      bn_short_set(&rpacket->u.server_w3route_ack.unknown2,0);
      bn_int_set(&rpacket->u.server_w3route_ack.unknown3,0xdeafbeef);
      bn_short_set(&rpacket->u.server_w3route_ack.unknown4,0xcccc);
      bn_byte_set(&rpacket->u.server_w3route_ack.playernum,anongame_get_playernum(a));
      bn_short_set(&rpacket->u.server_w3route_ack.unknown5,0x0002);
      bn_short_set(&rpacket->u.server_w3route_ack.port,conn_get_port(c));
      bn_int_set(&rpacket->u.server_w3route_ack.ip,htonl(conn_get_addr(c)));
      bn_int_set(&rpacket->u.server_w3route_ack.unknown7,0);
      bn_int_set(&rpacket->u.server_w3route_ack.unknown8,0);
      queue_push_packet(conn_get_out_queue(c), rpacket);
      packet_del_ref(rpacket);
      
      return 0;
   } else {
      gamec = conn_get_routeconn(c);
      if(gamec)
	a = conn_get_anongame(gamec);
   }
   
   if(!gamec) {
      eventlog(eventlog_level_info,"handle_w3route_packet","[%d] no game connection found for this w3route connection", conn_get_socket(c));
      return 0;
   }
   
   if(!a) {
      eventlog(eventlog_level_info,"handle_w3route_packet","[%d] no anongame struct found for this w3route connection", conn_get_socket(c));
      return 0;
   }
   
   gametype = anongame_get_type(a);
   plnum = anongame_get_playernum(a);
   tp = anongame_get_totalplayers(a);
   
   // handle these packets _before_ checking for routeconn of other players
   switch(packet_get_type(packet)) {
    case CLIENT_W3ROUTE_ECHOREPLY:
      return 0;
    case CLIENT_W3ROUTE_CONNECTED:
      //		eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] got W3ROUTE_CONNECTED: %d",conn_get_socket(c), (int)bn_short_get(packet->u.client_w3route_connected.unknown1));
      return 0;
    case CLIENT_W3ROUTE_GAMERESULT:
	{
	   int result = bn_int_get(packet->u.client_w3route_gameresult.result);
	   t_timer_data data;
	   t_anongameinfo *inf = anongame_get_info(a);
	   t_connection *ac;

	   data.p = NULL;
	   	   
	   eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] got W3ROUTE_GAMERESULT: %08x",conn_get_socket(c), result);
	   
	   if(!inf) {
	      eventlog(eventlog_level_error,"handle_w3route_packet","[%d] NULL anongameinfo",conn_get_socket(c));
	      return -1;
	   }
	   
	   anongame_set_result(a, result);
	   conn_set_state(c, conn_state_destroy);
	   
	   // activate timers on open w3route connectons
	   if (result == W3_GAMERESULT_WIN) {
	      for(i=0; i<tp; i++) {
	         ac = conn_get_routeconn(anongame_get_player(a,i));
	         if(ac) {
		    timerlist_add_timer(ac,time(NULL)+(time_t)300,conn_shutdown,data); // 300 seconds or 5 minute timer
		    eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] started timer to close w3route -> USER: %s",conn_get_socket(ac),conn_get_username(ac));
		 }
	      }
	   }

	   return 0;
	}
   }
   
   for(i=0; i<tp; i++)
     if(i+1 != plnum && anongame_get_player(a, i))
       if(!conn_get_routeconn(anongame_get_player(a, i)) || !conn_get_anongame(anongame_get_player(a, i))) {
	  eventlog(eventlog_level_info,"handle_w3route_packet","[%d] not all players have w3route connections up yet", conn_get_socket(c));
	  return 0;
       }
   
   // handle these packets _after_ checking for routeconns of other players
   // 
   switch(packet_get_type(packet)) {
    case CLIENT_W3ROUTE_LOADINGDONE:
      eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] got LOADINGDONE, playernum: %d",conn_get_socket(c),plnum);
      
      anongame_set_loaded(a, 1);
      
      for(i=0; i<tp; i++) {
	 if(!anongame_get_player(a,i))	// ignore disconnected players
	   continue;
	 if(!(rpacket = packet_create(packet_class_w3route))) {
	    eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
	    return -1;
	 }
	 packet_set_size(rpacket,sizeof(t_server_w3route_loadingack));
	 packet_set_type(rpacket,SERVER_W3ROUTE_LOADINGACK);
	 bn_byte_set(&rpacket->u.server_w3route_loadingack.playernum,plnum);
	 queue_push_packet(conn_get_out_queue(conn_get_routeconn(anongame_get_player(a, i))),rpacket);
	 packet_del_ref(rpacket);
      }
      
      // have all players loaded?
      for(i=0; i<tp; i++)
	if(i+1 != plnum && anongame_get_player(a,i) && !anongame_get_loaded(conn_get_anongame(anongame_get_player(a,i))))
	  return 0;
      
      for(i=0; i<tp; i++) {
	 if(!anongame_get_player(a,i))
	   continue;
	 
	 if(!(rpacket = packet_create(packet_class_w3route))) {
	    eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
	    return -1;
	 }
	 
	 packet_set_size(rpacket,sizeof(t_server_w3route_ready));
	 packet_set_type(rpacket,SERVER_W3ROUTE_READY);
	 bn_byte_set(&rpacket->u.server_w3route_host.unknown1,0);
	 queue_push_packet(conn_get_out_queue(conn_get_routeconn(anongame_get_player(a, i))),rpacket);
	 packet_del_ref(rpacket);
      }
      
      break;
      
    case CLIENT_W3ROUTE_ABORT:
      eventlog(eventlog_level_debug,"handle_w3route_packet","[%d] got W3ROUTE_ABORT",conn_get_socket(c));
      break;
      
    default:
      eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] default: got packet type: %04x",conn_get_socket(c),packet_get_type(packet));
   }
   
   return 0;
}

// [zap-zero] 20020529
extern int handle_anongame_join(t_connection * c)
{
	t_anongame *a, *ja, *oa;
	t_connection *rc, *jc, *o;
	t_packet * rpacket = NULL;
	int tp, level;
	char gametype;
	t_account *acct;

	static t_server_w3route_playerinfo2 pl2;
	static t_server_w3route_levelinfo2 li2;
	static t_server_w3route_playerinfo_addr pl_addr;

	int i, j;

	if(!c) {
		eventlog(eventlog_level_error,"handle_anongame_join","[%d] got NULL connection",conn_get_socket(c));
		return -1;
	}
	if(!(rc = conn_get_routeconn(c))) {
		eventlog(eventlog_level_info,"handle_anongame_join","[%d] no route connection",conn_get_socket(c));
		return 0;
	}
	if(!(a = conn_get_anongame(c))) {
		eventlog(eventlog_level_error,"handle_anongame_join","[%d] no anongame struct",conn_get_socket(c));
		return -1;
	}

	anongame_set_joined(a, 1);
	gametype = anongame_get_type(a);
	tp = anongame_get_totalplayers(a);

	// wait till all players have w3route conns
	for(i=0; i<tp; i++)
		if(anongame_get_player(a, i) && (!conn_get_routeconn(anongame_get_player(a, i)) || !conn_get_anongame(anongame_get_player(a, i)) || !anongame_get_joined(conn_get_anongame(anongame_get_player(a, i))))) {
			eventlog(eventlog_level_info,"handle_anongame_join","[%d] not all players have joined game BNet yet", conn_get_socket(c));
			return 0;
		}

		// then send each player info about all others

		for(j=0; j<tp; j++) {
			jc = anongame_get_player(a, j);
			if(!jc)			// ignore disconnected players
				continue;
			ja = conn_get_anongame(jc);

			// send a playerinfo packet for this player to each other player
			for(i=0; i<tp; i++) {
				if(i+1 != anongame_get_playernum(ja)) {
					eventlog(eventlog_level_trace, "handle_anongame_join", "i = %d", i);

					if(!(o = anongame_get_player(ja,i))) {
						eventlog(eventlog_level_warn,"handle_anongame_join","[%d] player %d disconnected, ignoring",conn_get_socket(c),i);
						continue;
					}

					if(!(rpacket = packet_create(packet_class_w3route))) {
						eventlog(eventlog_level_error,"handle_anongame_join","[%d] packet_create failed",conn_get_socket(c));
						return -1;
					}

					packet_set_size(rpacket,sizeof(t_server_w3route_playerinfo));
					packet_set_type(rpacket,SERVER_W3ROUTE_PLAYERINFO);


					if(!(oa = conn_get_anongame(o))) {
						eventlog(eventlog_level_error,"handle_anongame_join","[%d] no anongame struct of player %d", conn_get_socket(c), i);
						return -1;
					}

					bn_int_set(&rpacket->u.server_w3route_playerinfo.handle,anongame_get_handle(oa));
					bn_byte_set(&rpacket->u.server_w3route_playerinfo.playernum,anongame_get_playernum(oa));

					packet_append_string(rpacket, conn_get_username(o));

					// playerinfo2
					bn_byte_set(&pl2.unknown1,8);
					bn_int_set(&pl2.id,anongame_get_id(oa));
					bn_int_set(&pl2.race,anongame_get_race(oa));
					packet_append_data(rpacket, &pl2, sizeof(pl2));

					// external addr
					bn_short_set(&pl_addr.unknown1,2);
					bn_short_set(&pl_addr.port,htons(conn_get_game_port(o)));
					bn_int_set(&pl_addr.ip,htonl(conn_get_game_addr(o)));
					bn_int_set(&pl_addr.unknown2,0);
					bn_int_set(&pl_addr.unknown3,0);
					packet_append_data(rpacket, &pl_addr, sizeof(pl_addr));

					// local addr
					bn_short_set(&pl_addr.unknown1,2);
					bn_short_set(&pl_addr.port,htons(conn_get_game_port(o)));
					bn_int_set(&pl_addr.ip,(anongame_get_addr(oa)));
					bn_int_set(&pl_addr.unknown2,0);
					bn_int_set(&pl_addr.unknown3,0);
					packet_append_data(rpacket, &pl_addr, sizeof(pl_addr));

					queue_push_packet(conn_get_out_queue(conn_get_routeconn(jc)), rpacket);
					packet_del_ref(rpacket);
				}
			}

			// levelinfo
			if(!(rpacket = packet_create(packet_class_w3route))) {
				eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
				return -1;
			}

			packet_set_size(rpacket,sizeof(t_server_w3route_levelinfo));
			packet_set_type(rpacket,SERVER_W3ROUTE_LEVELINFO);
			bn_byte_set(&rpacket->u.server_w3route_levelinfo.numplayers, anongame_get_currentplayers(a));

			for(i=0; i<tp; i++) {
				if(!anongame_get_player(ja,i))
					continue;

				bn_byte_set(&li2.plnum, i+1);
				bn_byte_set(&li2.unknown1, 3);

				switch(gametype) {
			case ANONGAME_TYPE_1V1:
				level = account_get_sololevel(conn_get_account(anongame_get_player(ja,i)));
				break;
			case ANONGAME_TYPE_SMALL_FFA:
			case ANONGAME_TYPE_TEAM_FFA:
				level = account_get_ffalevel(conn_get_account(anongame_get_player(ja,i)));
				break;
			case ANONGAME_TYPE_AT_2V2:
			case ANONGAME_TYPE_AT_3V3:
			case ANONGAME_TYPE_AT_4V4:
				acct = conn_get_account(anongame_get_player(ja,i));
				level = account_get_atteamlevel((acct),account_get_currentatteam(acct));
				break;
			default:
				level = account_get_teamlevel(conn_get_account(anongame_get_player(ja,i)));
				break;
				}

				// first anongame shows level 0 as level 1
				bn_byte_set(&li2.level, level ? level : 1);

				bn_short_set(&li2.unknown2,0);
				packet_append_data(rpacket, &li2, sizeof(li2));
			}

			queue_push_packet(conn_get_out_queue(conn_get_routeconn(jc)), rpacket);
			packet_del_ref(rpacket);

			// startgame1
			if(!(rpacket = packet_create(packet_class_w3route))) {
				eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
				return -1;
			}
			packet_set_size(rpacket,sizeof(t_server_w3route_startgame1));
			packet_set_type(rpacket,SERVER_W3ROUTE_STARTGAME1);
			queue_push_packet(conn_get_out_queue(conn_get_routeconn(jc)), rpacket);
			packet_del_ref(rpacket);

			// startgame2
			if(!(rpacket = packet_create(packet_class_w3route))) {
				eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
				return -1;
			}
			packet_set_size(rpacket,sizeof(t_server_w3route_startgame2));
			packet_set_type(rpacket,SERVER_W3ROUTE_STARTGAME2);
			queue_push_packet(conn_get_out_queue(conn_get_routeconn(jc)), rpacket);
			packet_del_ref(rpacket);
		}
		return 0;
}


extern t_anongameinfo * anongameinfo_create(int totalplayers)
{
	t_anongameinfo *temp;
	int i;

	temp = malloc(sizeof(t_anongameinfo));
	if(!temp) {
		eventlog(eventlog_level_error, "anongameinfo_create", "could not allocate memory for temp");
		return NULL;
	}

	temp->totalplayers = temp->currentplayers = totalplayers;
	for(i=0; i<8; i++) {
	   temp->player[i] = NULL;
	   temp->account[i] = NULL;
	   temp->result[i] = -1; /* consider DISC default */
	}

	return temp;
}


extern void anongameinfo_destroy(t_anongameinfo *i)
{
	if(!i) {
		eventlog(eventlog_level_error, "anongameinfo_destroy", "got NULL anongameinfo");
		return;
	}
	free(i);
}


extern void anongame_set_count(t_anongame * a, int count)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_count","got NULL anongame");
		return;
	}

	a->count = count;
}

extern int anongame_get_count(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_count","got NULL anongame");
		return 0;
	}
	return a->count;
}

extern void anongame_set_info(t_anongame * a, t_anongameinfo * i)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_count","got NULL anongame");
		return;
	}
	if (!i)
	{
		eventlog(eventlog_level_error,"anongame_set_count","got NULL anongameinfo");
		return;
	}

	a->info = i;
}

extern t_anongameinfo * anongame_get_info(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_count","got NULL anongame");
		return NULL;
	}

	return a->info;
}


// [zap-zero] 20020629
extern void anongame_set_loaded(t_anongame * a, char loaded)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_loaded","got NULL anongame");
		return;
	}

	a->loaded = loaded;
}

extern char anongame_get_loaded(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_loaded","got NULL anongame");
		return 0;
	}
	return a->loaded;
}


extern void anongame_set_joined(t_anongame * a, char joined)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_joined","got NULL anongame");
		return;
	}

	a->joined = joined;
}

extern char anongame_get_joined(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_joined","got NULL anongame");
		return 0;
	}
	return a->joined;
}




// [zap-zero] 20020701
extern void anongame_set_result(t_anongame * a, int result)
{
   if (!a)
     {
	eventlog(eventlog_level_error,"anongame_set_result","got NULL anongame");
	return;
     }
   if (!a->info)
     {
	eventlog(eventlog_level_error,"anongame_set_result","NULL anongameinfo");
	return;
     }
   
   if (a->playernum < 1 || a->playernum > 8) 
     {
	eventlog(eventlog_level_error,"anongame_set_result","invalid playernum: %d", a->playernum);
	return;
     }
   
   a->info->result[a->playernum-1] = result;
}


// [zap-zero] 20020529
extern void anongame_set_id(t_anongame * a, t_uint32 id)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_id","got NULL anongame");
		return;
	}

	a->id = id;
}

extern t_uint32 anongame_get_id(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_id","got NULL anongame");
		return 0;
	}
	return a->id;
}

extern void anongame_set_race(t_anongame *a, t_uint32 race)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_race","got NULL anongame");
		return;
	}

	a->race = race;
}

extern t_uint32 anongame_get_race(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_race","got NULL anongame");
		return 0;
	}
	return a->race;
}


// [zap-zero] 20020602
extern void anongame_set_handle(t_anongame *a, t_uint32 h)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_handle","got NULL anongame");
		return;
	}

	a->handle = h;
}

extern t_uint32 anongame_get_handle(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_handle","got NULL anongame");
		return 0;
	}
	return a->handle;
}




// [zap-zero] 20020530
extern void anongame_set_playernum(t_anongame *a, t_uint8 plnum)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_playernum","got NULL anongame");
		return;
	}

	a->playernum = plnum; 
}

extern t_uint8 anongame_get_playernum(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_playernum","got NULL anongame");
		return 0;
	}
	return a->playernum;
}


extern void anongame_set_type(t_anongame *a, t_uint8 type)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_type","got NULL anongame");
		return;
	}

	a->type = type;
}

extern t_uint8 anongame_get_type(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_type","got NULL anongame");
		return 0;
	}
	return a->type;
}



// [zap-zero] 20020607
extern void anongame_set_addr(t_anongame *a, unsigned int addr)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_addr","got NULL anongame");
		return;
	}

	a->addr = addr;
}

extern unsigned int anongame_get_addr(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_addr","got NULL anongame");
		return 0;
	}
	return a->addr;
}


extern int anongame_get_totalplayers(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_totalplayers","got NULL anongame");
		return 0;
	}
	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_get_totalplayers","NULL anongameinfo");
		return 0;
	}

	return a->info->totalplayers;
}


extern void anongame_set_currentplayers(t_anongame *a, int currplayers)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_currentplayers","got NULL anongame");
		return;
	}

	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_set_currentplayers","NULL anongameinfo");
		return;
	}

	a->info->currentplayers = currplayers;
}

extern int anongame_get_currentplayers(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_totalplayers","got NULL anongame");
		return 0;
	}
	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_get_totalplayers","NULL anongameinfo");
		return 0;
	}

	return a->info->currentplayers;
}

extern void anongame_set_player(t_anongame * a, int plnum, t_connection * c)
{
	if (!a) {
		eventlog(eventlog_level_error,"anongame_set_player","got NULL anongame");
		return;
	}

	if (!a->info) {
		eventlog(eventlog_level_error,"anongame_set_player","NULL anongameinfo");
		return;
	}

	if(plnum < 0 || plnum > 7 || plnum >= a->info->totalplayers) {
		eventlog(eventlog_level_error,"anongame_set_player","invalid plnum: %d", plnum);
		return;
	}

	a->info->player[plnum] = c;
	a->info->account[plnum] = conn_get_account(c);
}


extern t_connection * anongame_get_player(t_anongame * a, int plnum)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_player","got NULL anongame");
		return NULL;
	}
	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_get_player","NULL anongameinfo");
		return NULL;
	}

	if(plnum < 0 || plnum > 7 || plnum >= a->info->totalplayers) {
		eventlog(eventlog_level_error,"anongame_get_player","invalid plnum: %d", plnum);
		return NULL;
	}

	return a->info->player[plnum];
}

extern int anongame_get_result(t_anongame * a, int plnum)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_result","got NULL anongame");
		return -1;
	}
	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_get_result","NULL anongameinfo");
		return -1;
	}

	if(plnum < 0 || plnum > 7 || plnum >= a->info->totalplayers) {
		eventlog(eventlog_level_error,"anongame_get_result","invalid plnum: %d", plnum);
		return -1;
	}

	return a->info->result[plnum];
}


extern t_account * anongame_get_account(t_anongame * a, int plnum)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_player","got NULL anongame");
		return NULL;
	}
	if (!a->info)
	{
		eventlog(eventlog_level_error,"anongame_get_player","NULL anongameinfo");
		return NULL;
	}

	if(plnum < 0 || plnum > 7 || plnum >= a->info->totalplayers) {
		eventlog(eventlog_level_error,"anongame_get_player","invalid plnum: %d", plnum);
		return NULL;
	}

	return a->info->account[plnum];
}

extern int anongame_stats(t_connection * c)
{
    int			i;
    int			wins=0, losses=0, discs=0;
    t_connection	* gamec = conn_get_routeconn(c);
    t_anongame		* a = conn_get_anongame(gamec);
    int			tp = anongame_get_totalplayers(a);
    t_uint8		gametype = anongame_get_type(a);
    t_uint8		plnum = anongame_get_playernum(a);
    
    // do nothing till all other players have w3route conn closed
    for(i=0; i<tp; i++)
	if(i+1 != plnum && anongame_get_player(a,i))
	    if(conn_get_routeconn(anongame_get_player(a,i)))
		return 0;
    
    // count wins, losses, discs
    for(i=0; i<tp; i++) {
	if(anongame_get_result(a,i) == W3_GAMERESULT_WIN)
	    wins++;
	else if(anongame_get_result(a,i) == W3_GAMERESULT_LOSS)
	    losses++;
	else
	    discs++;
    }
    
    // do some sanity checking (hack prevention)
    switch(gametype) {
	case ANONGAME_TYPE_SMALL_FFA:
	    if(wins != 1) {
		eventlog(eventlog_level_info, "handle_w3route_packet", "bogus game result: wins != 1 in small ffa game");
		return -1;
	    }
	    break;
	case ANONGAME_TYPE_TEAM_FFA:
	    if(!discs && wins != 2) {
		eventlog(eventlog_level_info, "handle_w3route_packet", "bogus game result: wins != 2 in team ffa game");
		return -1;
	    }
	    break;
	default:
	    if(!discs && wins > losses) {
		eventlog(eventlog_level_info, "handle_w3route_packet", "bogus game result: wins > losses");
		return -1;
	    }
	    break;
    }
    
    // prevent users from getting loss if server is shutdown (does not prevent errors from crash) - creep
    if (discs == tp)
	if (!wins)
    	    return -1;
    
    for(i=0; i<tp; i++) {
	t_account * oacc = anongame_get_account(a, (i+1)%tp);
	int result = anongame_get_result(a,i);
	
	if(result == -1)
	    result = W3_GAMERESULT_LOSS;
	        
	switch(gametype) {
	    case ANONGAME_TYPE_1V1:
		if(result == W3_GAMERESULT_WIN)
		    account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_win,account_get_sololevel(oacc));
		if(result == W3_GAMERESULT_LOSS)
		    account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_loss,account_get_sololevel(oacc));
		break;
	    case ANONGAME_TYPE_SMALL_FFA:
		// FIXME: need to adjust FFA xp and levels
		if(result == W3_GAMERESULT_WIN)
		    //account_set_ffawin(anongame_get_account(a,i));
		    account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_win,account_get_ffalevel(oacc));
		if(result == W3_GAMERESULT_LOSS)
		    //account_set_ffaloss(anongame_get_account(a,i));
	            account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_loss,account_get_ffalevel(oacc));
		break;
	    case ANONGAME_TYPE_AT_2V2:
	    case ANONGAME_TYPE_AT_3V3:
	    case ANONGAME_TYPE_AT_4V4:
		if(result == W3_GAMERESULT_WIN) { //Modified by DJP in an attempt to manage teamcount ! ( bug of previous CVS 1.2.4 )
			if(account_get_new_at_team(conn_get_account(c))==1) {
					 int temp;

				account_set_new_at_team(conn_get_account(c),0);
			    temp = account_get_atteamcount(conn_get_account(c));
				temp = temp+1;
				account_set_atteamcount(conn_get_account(c),temp);
				account_set_saveATladderstats(anongame_get_account(a,i),gametype,game_result_win,account_get_atteamlevel(oacc,account_get_currentatteam(oacc)),account_get_currentatteam(anongame_get_account(a,i)));
			}
			else {
				account_set_saveATladderstats(anongame_get_account(a,i),gametype,game_result_win,account_get_atteamlevel(oacc,account_get_currentatteam(oacc)),account_get_currentatteam(anongame_get_account(a,i)));
			}
		}
		if(result == W3_GAMERESULT_LOSS) { //Modified by DJP in an attempt to manage teamcount ! ( bug of previous CVS 1.2.4 )
			if(account_get_new_at_team(conn_get_account(c))==1) {
					int temp;

				account_set_new_at_team(conn_get_account(c),0);
			    temp = account_get_atteamcount(conn_get_account(c));
				temp = temp+1;
				account_set_atteamcount(conn_get_account(c),temp);
				account_set_saveATladderstats(anongame_get_account(a,i),gametype,game_result_loss,account_get_atteamlevel(oacc,account_get_currentatteam(oacc)),account_get_currentatteam(anongame_get_account(a,i)));
			}
			else {
				account_set_saveATladderstats(anongame_get_account(a,i),gametype,game_result_loss,account_get_atteamlevel(oacc,account_get_currentatteam(oacc)),account_get_currentatteam(anongame_get_account(a,i)));
			}
		}
		break;
	    default:
		// FIXME: only get team level of one opponent here... should get max or average level of all opponents?
		if(result == W3_GAMERESULT_WIN)
		    account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_win,account_get_teamlevel(oacc));
		if(result == W3_GAMERESULT_LOSS)
		    account_set_saveladderstats(anongame_get_account(a,i),gametype,game_result_loss,account_get_teamlevel(oacc));
		break;
	}
    }
    // aaron: now update war3 ladders
    war3_ladder_update_all_accounts();
    return 1;
}

