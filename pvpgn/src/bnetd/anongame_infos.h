#define JUST_NEED_TYPES
#include "common/list.h"
#undef JUST_NEED_TYPES

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
} t_anongame_infos_THUMBSDOWN;

typedef struct {
	t_anongame_infos_URL	* anongame_infos_URL;
	t_anongame_infos_DESC	* anongame_infos_DESC;			// for default DESC
	t_list			* anongame_infos_DESC_list;		// for localized DESC's
	t_anongame_infos_THUMBSDOWN * anongame_infos_THUMBSDOWN;	// for storing thumbs down config
} t_anongame_infos;

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

extern char * anongame_infos_DESC_get_gametype_1v1_short(char * langID);
extern char * anongame_infos_DESC_get_gametype_1v1_long(char * langID);
extern char * anongame_infos_DESC_get_gametype_2v2_short(char * langID);
extern char * anongame_infos_DESC_get_gametype_2v2_long(char * langID);
extern char * anongame_infos_DESC_get_gametype_3v3_short(char * langID);
extern char * anongame_infos_DESC_get_gametype_3v3_long(char * langID);
extern char * anongame_infos_DESC_get_gametype_4v4_short(char * langID);
extern char * anongame_infos_DESC_get_gametype_4v4_long(char * langID);
extern char * anongame_infos_DESC_get_gametype_ffa_short(char * langID);
extern char * anongame_infos_DESC_get_gametype_ffa_long(char * langID);

extern char anongame_infos_THUMBSDOWN_get_PG_1v1();
extern char anongame_infos_THUMBSDOWN_get_PG_2v2();
extern char anongame_infos_THUMBSDOWN_get_PG_3v3();
extern char anongame_infos_THUMBSDOWN_get_PG_4v4();
extern char anongame_infos_THUMBSDOWN_get_PG_ffa();
extern char anongame_infos_THUMBSDOWN_get_AT_2v2();
extern char anongame_infos_THUMBSDOWN_get_AT_3v3();
extern char anongame_infos_THUMBSDOWN_get_AT_4v4();
