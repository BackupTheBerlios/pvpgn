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
#ifndef INCLUDED_ANONGAME_INFOS_TYPES
#define INCLUDED_ANONGAME_INFOS_TYPES

#ifdef JUST_NEED_TYPES
#include "common/list.h"
#else
#define JUST_NEED_TYPES
#include "common/list.h"
#undef JUST_NEED_TYPES
#endif

typedef struct {
	char * server_URL;
	char * player_URL;
	char * tourney_URL;

	char * ladder_PG_1v1_URL;
	char * ladder_PG_ffa_URL;
	char * ladder_PG_team_URL;

	char * ladder_AT_2v2_URL;
	char * ladder_AT_3v3_URL;
	char * ladder_AT_4v4_URL;
} t_anongame_infos_URL;

typedef struct {
	char * langID;
	
	char * ladder_PG_1v1_desc;
	char * ladder_PG_ffa_desc;
	char * ladder_PG_team_desc;

	char * ladder_AT_2v2_desc;
	char * ladder_AT_3v3_desc;
	char * ladder_AT_4v4_desc;

	char * gametype_1v1_short;
	char * gametype_1v1_long;
	char * gametype_2v2_short;
	char * gametype_2v2_long;
	char * gametype_3v3_short;
	char * gametype_3v3_long;
	char * gametype_4v4_short;
	char * gametype_4v4_long;
	char * gametype_ffa_short;
	char * gametype_ffa_long;
	char * gametype_2v2v2_short;
	char * gametype_2v2v2_long;

	char * gametype_sffa_short;
	char * gametype_sffa_long;
	char * gametype_tffa_short;
	char * gametype_tffa_long;
	char * gametype_3v3v3_short;
	char * gametype_3v3v3_long;
	char * gametype_4v4v4_short;
	char * gametype_4v4v4_long;
	char * gametype_2v2v2v2_short;
	char * gametype_2v2v2v2_long;
	char * gametype_3v3v3v3_short;
	char * gametype_3v3v3v3_long;
	char * gametype_5v5_short;
	char * gametype_5v5_long;
	char * gametype_6v6_short;
	char * gametype_6v6_long;
	
} t_anongame_infos_DESC;

typedef struct {
	char PG_1v1;
	char PG_2v2;
	char PG_3v3;
	char PG_4v4;
	char PG_ffa;
	char AT_2v2;
	char AT_3v3;
	char AT_4v4;
	char PG_2v2v2;
	char AT_ffa;
	char PG_5v5;
	char PG_6v6;
	char PG_3v3v3;
	char PG_4v4v4;
	char PG_2v2v2v2;
	char PG_3v3v3v3;
	char AT_2v2v2;
} t_anongame_infos_THUMBSDOWN;

typedef struct {
	t_anongame_infos_URL	* anongame_infos_URL;
	t_anongame_infos_DESC	* anongame_infos_DESC;			// for default DESC
	t_list			* anongame_infos_DESC_list;		// for localized DESC's
	t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN;	// for storing thumbs down config
} t_anongame_infos;

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_ANONGAME_INFOS_PROTOS
#define INCLUDED_ANONGAME_INFOS_PROTOS

extern int anongame_infos_load(char const * filename);
extern int anongame_infos_unload(void);

extern char * anongame_infos_URL_get_server_url();
extern char * anongame_infos_URL_get_player_url();
extern char * anongame_infos_URL_get_tourney_url();
extern char * anongame_infos_URL_get_ladder_PG_1v1_url();
extern char * anongame_infos_URL_get_ladder_PG_ffa_url();
extern char * anongame_infos_URL_get_ladder_PG_team_url();
extern char * anongame_infos_URL_get_ladder_AT_2v2_url();
extern char * anongame_infos_URL_get_ladder_AT_3v3_url();
extern char * anongame_infos_URL_get_ladder_AT_4v4_url();

extern char * anongame_infos_DESC_get_ladder_PG_1v1_desc(char * langID);
extern char * anongame_infos_DESC_get_ladder_PG_ffa_desc(char * langID);
extern char * anongame_infos_DESC_get_ladder_PG_team_desc(char * langID);

extern char * anongame_infos_DESC_get_ladder_AT_2v2_desc(char * langID);
extern char * anongame_infos_DESC_get_ladder_AT_3v3_desc(char * langID);
extern char * anongame_infos_DESC_get_ladder_AT_4v4_desc(char * langID);

extern char * anongame_infos_get_short_desc(char * langID, int queue);
extern char * anongame_infos_get_long_desc(char * langID, int queue);

extern char anongame_infos_get_thumbsdown(int queue);

#endif
#endif
