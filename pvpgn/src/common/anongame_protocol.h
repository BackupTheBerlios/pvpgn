#ifndef INCLUDED_ANONGAME_PROTOCOL_TYPES
#define INCLUDED_ANONGAME_PROTOCOL_TYPES

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

/***********************************************************************************/
/* first packet recieved from client - option decides which struct to use next */
#define CLIENT_FINDANONGAME 			0x44ff
#define SERVER_FINDANONGAME 			0x44ff
typedef struct
{
    t_bnet_header	h;
    bn_byte		option;
    /* rest of packet data */
} t_client_anongame PACKED_ATTR();

/***********************************************************************************/
#define CLIENT_FINDANONGAME_SEARCH              0x00
#define CLIENT_FINDANONGAME_INFOS               0x02
#define CLIENT_FINDANONGAME_CANCEL		0x03
#define CLIENT_FINDANONGAME_PROFILE             0x04
#define CLIENT_FINDANONGAME_AT_SEARCH           0x05
#define CLIENT_FINDANONGAME_AT_INVITER_SEARCH   0x06
#define CLIENT_ANONGAME_TOURNAMENT		0X07
#define CLIENT_FINDANONGAME_GET_ICON            0x09
#define CLIENT_FINDANONGAME_SET_ICON            0x0A

#define SERVER_FINDANONGAME_SEARCH              0x00
#define SERVER_FINDANONGAME_FOUND		0x01
#define SERVER_FINDANONGAME_CANCEL              0x03
/***********************************************************************************/
/* option 00 - anongame search */
typedef struct
{
    t_bnet_header	h;
    bn_byte		option;
    bn_int		count;     /* Goes up each time client clicks search */
    bn_int		unknown1;
    bn_byte		unknown2;  /* 0 = PG  , 1 = AT  , 2 = TY */
    bn_byte		gametype;  /* 0 = 1v1 , 1 = 2v2 , 2 = 3v3 , 3 = 4v4 */
    bn_int		map_prefs;
    bn_byte		unknown5;  /* 8 */
    bn_int		id;        /* random */
    bn_int       	race;      /* see defines */
} t_client_findanongame PACKED_ATTR();

typedef struct
{
    t_bnet_header h;
    bn_byte      option;
    bn_int       count;     //Goes up each time client clicks search -01 00 00 00
    bn_int       unknown1[8];
    bn_byte      unknown2;  /* 0 = PG  , 1 = AT  , 2 = TY */
    bn_byte      teamsize;  // 2=2v2, 3=3v3
    bn_byte      unknown3;
    bn_int       map_prefs; // map preferences bitmask
    bn_byte      unknown4;
    bn_int       id;        // random
    bn_int       race;      // see defines
} t_client_findanongame_at_inv PACKED_ATTR();

typedef struct
{
    t_bnet_header h;
    bn_byte      option;
    bn_int       count;     /* Goes up each time client clicks search */
    bn_int       unknown1;
    bn_int       timestamp;
    bn_byte      gametype;  // 1=2v2, 2=3v3, 3=4v4
    bn_int       unknown2[6];
    bn_byte      unknown3;
    bn_int       id;
    bn_int       race;
} t_client_findanongame_at PACKED_ATTR();

#define SERVER_ANONGAME_SEARCH_REPLY		0x44ff
typedef struct
{
    t_bnet_header h;
    bn_byte   option;
    bn_int    count;
    bn_int    reply;
    /*      bn_short  avgtime; - only in W3XP so far average time in seconds of search */
} t_server_anongame_search_reply PACKED_ATTR();

/***********************************************************************************/
/* option 01 - anongame found */
#define SERVER_ANONGAME_FOUND			0x44ff
typedef struct
{
    t_bnet_header h;
    bn_byte type;               /* 1: anongame found */
    bn_int count;
    bn_int unknown1;
    bn_int ip;
    bn_short port;
    bn_byte numplayers; /* 2 for 1vs1, 4 for 2vs2 etc */
    bn_byte playernum;  /* 1-8 */
    bn_byte gametype;
    bn_byte unknown2;   /* 0x00 */
    bn_int id;          /* random val for identifying client */
    bn_byte unknown4;   /* 0x06 */
    bn_short unknown5;  /* 0x0000 */
    
    // MAP NAME //
    // MISC PACKET APPEND DATA's //
} t_server_anongame_found PACKED_ATTR();

/***********************************************************************************/
/* option 02 - info request */
#define CLIENT_FINDANONGAME_INFOREQ             0x44ff
typedef struct
{
    t_bnet_header       h;
    bn_byte             option;         /* type of request: 
					 * 0x02 for matchmaking infos */
    bn_int              count;          /* 0x00000001 increments each request of same type */
    bn_byte             noitems;
} t_client_findanongame_inforeq PACKED_ATTR();

#define SERVER_FINDANONGAME_INFOREPLY           0x44ff
typedef struct {
    t_bnet_header       h;
    bn_byte             option; /* as received from client */
    bn_int              count; /* as received from client */
    bn_byte             noitems; /* not very sure about it */
    /* data */
/*
for type 0x02 :
    <type of info><unknown int><info>
    if <type of info> is :
	URL\0 : <info> contains 3 NULL terminated urls/strings
	MAP\0 : <info> contains 7 (seen so far) maps names
	TYPE  : <info> unknown 38 bytes probably meaning anongame types
	DESC  : <info>
*/
} t_server_findanongame_inforeply PACKED_ATTR();

/***********************************************************************************/
/* option 03 - playgame cancel */
#define SERVER_FINDANONGAME_PLAYGAME_CANCEL 	0x44ff
typedef struct
{
    t_bnet_header h; // header
    bn_byte cancel; // Cancel byte always 03
    bn_int  count;
} t_server_findanongame_playgame_cancel PACKED_ATTR();


/* option 04 - profile request */
typedef struct
{
    t_bnet_header h;
    bn_byte     option;
    bn_int          count;
    // USERNAME TO LOOKUP //
    // CLIENT TAG //
} t_client_findanongame_profile PACKED_ATTR();

#define SERVER_FINDANONGAME_PROFILE		0x44ff
/*
typedef struct
{
    t_bnet_header h; //header
    bn_byte option; // in this case it will be 0x04 (for profile)
    bn_int count; // count that goes up each time user clicks on someones profile
    // REST OF PROFILE STATS - THIS WILL BE SET IN HANDLE_BNET.C after
    // SERVER LOOKS UP THE USER ACCOUNT
} t_server_findanongame_profile PACKED_ATTR();
*/
typedef struct
{
    t_bnet_header	h;
    bn_byte		option;
    bn_int		count;
    bn_int		icon;
    bn_byte		rescount;
    // REST OF PROFILE STATS - THIS WILL BE SET IN HANDLE_BNET.C after
    // SERVER LOOKS UP THE USER ACCOUNT
} t_server_findanongame_profile2 PACKED_ATTR();

/***********************************************************************************/
/* option 07 - tournament request */
#define CLIENT_FINDANONGAME_TOURNAMENT_REQUEST  0x44ff
typedef struct
{
    t_bnet_header       h;
    bn_byte             option; /* 07 */
    bn_int              count;  /* 01 00 00 00 */
} t_client_anongame_tournament_request PACKED_ATTR();

#define SERVER_FINDANONGAME_TOURNAMENT_REPLY    0x44ff
typedef struct
{
    t_bnet_header       h;
    bn_byte             option;     /* 07 */
    bn_int              count;      /* 00 00 00 01 reply with same number */
    bn_byte             type;	    /* type - 	01 = notice - time = prelim round begins
				     *		02 = signups - time = signups end
				     *		03 = signups over - time = prelim round ends
				     *		04 = prelim over - time = finals round 1 begins
				     */
    bn_byte             unknown;    /* 00 */
    bn_short            unknown4;   /* random ? might be part of time/date ? */
    bn_int              timestamp;
    bn_byte             unknown5;   /* 01 effects time/date */
    bn_short            countdown;  /* countdown until next timestamp (seconds?) */
    bn_short            unknown2;   /* 00 00 */
    bn_byte		wins;	    /* during prelim */
    bn_byte		losses;     /* during prelim */
    bn_byte             ties;	    /* during prelim */
    bn_byte             unknown3;   /* 00 = notice.  08 = signups thru prelim over (02-04) */
    bn_byte             selection;  /* matches anongame_TY_section of DESC */
    bn_byte             descnum;    /* matches desc_count of DESC */
    bn_byte             nulltag;    /* 00 */
} t_server_anongame_tournament_reply PACKED_ATTR();

/***********************************************************************************/
/* option 9 - icon request */
#define SERVER_FINDANONGAME_ICONREPLY           0x44ff
typedef struct{
    t_bnet_header         h;
    bn_byte               option;                 /* as received from client */
    bn_int                count;                  /* as received from client */
    bn_int                curricon;               /* current icon code */
    bn_byte               table_width;            /* the icon table width */
    bn_byte               table_size;             /* the icon table total size */
    /* table data */
} t_server_findanongame_iconreply PACKED_ATTR();

/***********************************************************************************/
#define SERVER_ANONGAME_SOLO_STR        	0x534F4C4F /* "SOLO" */
#define SERVER_ANONGAME_TEAM_STR        	0x5445414D /* "TEAM" */
#define SERVER_ANONGAME_SFFA_STR        	0x46464120 /* "FFA " */
#define SERVER_ANONGAME_AT2v2_STR       	0x32565332 /* "2VS2" */
#define SERVER_ANONGAME_AT3v3_STR       	0x33565333 /* "3VS3" */
#define SERVER_ANONGAME_AT4v4_STR       	0x34565334 /* "4VS4" */
#define SERVER_ANONGAME_TY_STR			0X54592020 /* "TY  " FIXME-TY: WHAT TO PUT HERE */

#define CLIENT_FINDANONGAME_INFOTAG_URL         0x55524c        //  URL\0
#define CLIENT_FINDANONGAME_INFOTAG_MAP         0x4d4150        //  MAP\0
#define CLIENT_FINDANONGAME_INFOTAG_TYPE        0x54595045      //  TYPE
#define CLIENT_FINDANONGAME_INFOTAG_DESC        0x44455343      //  DESC
#define CLIENT_FINDANONGAME_INFOTAG_LADR        0x4c414452      //  LADR
#define CLIENT_FINDANONGAME_INFOTAG_SOLO        0x534f4c4f      //  SOLO
#define CLIENT_FINDANONGAME_INFOTAG_TEAM        0x5445414d      //  TEAM
#define CLIENT_FINDANONGAME_INFOTAG_FFA         0x46464120      //  FFA\20

#define ANONGAME_TYPE_1V1       0
#define ANONGAME_TYPE_2V2       1
#define ANONGAME_TYPE_3V3       2
#define ANONGAME_TYPE_4V4       3
#define ANONGAME_TYPE_SMALL_FFA 4
#define ANONGAME_TYPE_AT_2V2    5
#define ANONGAME_TYPE_TEAM_FFA  6
#define ANONGAME_TYPE_AT_3V3    7
#define ANONGAME_TYPE_AT_4V4    8
#define ANONGAME_TYPE_TY	9

#define ANONGAME_TYPES 10

#define SERVER_FINDANONGAME_PROFILE_UNKNOWN2    0x6E736865 //Sheep
/***********************************************************************************/
#endif
