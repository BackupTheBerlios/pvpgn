<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<HTML><HEAD>
<META http-equiv=Content-Type content="text/html; charset=windows-1252"></HEAD>
<BODY onunload=""><PRE>/* War3 Frozen Throne Beta */

/*
0000  ff 72 3a 00 00 00 00 00 - 36 38 58 49 50 58 33 57   .r:.....68XIPX3W
0010  2d 01 00 00 53 55 6e 65 - 43 08 45 1f 2c 01 00 00   -...SUneC.E.,...
0020  09 04 00 00 09 04 00 00 - 55 53 41 00 55 6e 69 74   ........USA.Unit
0030  65 64 20 53 74 61 74 65 - 73 00                     ed States.
*/
#define CLIENT_W3XP_COUNTRYINFO 0x72ff
typedef struct
{
    t_bnet_header	h;
	bn_int			unknown1; /* 00 00 00 00 */
	bn_int			arch_tag;
	bn_int			client_tag;
	bn_int			versionid;
	bn_int			client_lang; /* 53 55 6e 65 */
	bn_int			unknown2; /* 43 08 45 1f */
	bn_int			bias; /* (gmt-local)/60 (signed math) */
	bn_int			lcid; /* 09 04 00 00 */
	bn_int			langid; /* 09 04 00 00 */
    /* langstr */
    /* countryname */
} t_client_w3xp_countryinfo PACKED_ATTR();
#define CLIENT_W3XP_COUNTRYINFO_UNKNOWN1				0x1F450843
#define CLIENT_W3XP_COUNTRYINFO_LANGID_USENGLISH		0x00000409	
#define CLIENT_W3XP_COUNTRYINFO_COUNTRYNAME_USA			"United States"

/* Write Server Replay Code Here */


/* 0000  ff 08 08 00 d6 a0 1c cc -                           ........ */
/* Possible ECHO? */
#define CLIENT_W3XP_	0x08ff
typedef struct
{


} t_client_w3xp_  PACKED_ATTR();

#define CLIENT_W3XP_AUTHREQ	0x04ff
typedef struct
{
    t_bnet_header h;
    bn_int        ticks;
    bn_int        gameversion;
    bn_int        checksum;
    bn_int        cdkey_number; /* count of cdkeys, d2 = 1, lod = 2 */
    bn_int        unknown1; /* 00 00 00 00 */
    /* cdkey info(s) */
    /* executable info */
    /* cdkey owner */
} t_client_w3xp_authreq PACKED_ATTR();

#define CLIENT_W3XP_FILEINFOREQ 0x73ff
typedef struct
{
    t_bnet_header h;
	bn_int        type;     /* type of file (TOS,icons,etc.) */
    bn_int        unknown1; /* 00 00 00 00 */ /* always zero? */
    /* filename */          /* default/suggested filename? */
} t_client_fileinforeq PACKED_ATTR();
#define CLIENT_W3XP_FILEINFOREQ_TYPE_TOS			      0x00000001 /* Terms of Service enUS */
#define CLIENT_W3XP_FILEINFOREQ_TYPE_NWACCT				  0x00000002 /* New Account File */
#define CLIENT_W3XP_FILEINFOREQ_TYPE_CHTHLP				  0x00000003 /* Chat Help */
#define CLIENT_W3XP_FILEINFOREQ_TYPE_SRVLST				  0x00000005 /* Server List */
#define CLIENT_W3XP_FILEINFOREQ_UNKNOWN1	              0x00000000
#define CLIENT_W3XP_FILEINFOREQ__FILE_TOSUSA              "termsofservice-enUS.txt"
#define CLIENT_W3XP_FILEINFOREQ__FILE_NWACCT			  "newaccount-enUS.txt"
#define CLIENT_W3XP_FILEINFOREQ__FILE_CHTHLP			  "chathelp-war3-enUS.txt"
#define CLIENT_W3XP_FILEINFOREQ__FILE_SRVLST			  "bnserver-WAR3.ini"

#define CLIENT_W3XP_LOGINREQ 0x43ff
typedef struct
{
    t_bnet_header h;
    bn_byte        unknown[32];
    /* player name */
} t_client_w3xp_loginreq PACKED_ATTR();

#define CLIENT_W3XP_LOGONPROOFREQ 0x12ff
typedef struct
{
    t_bnet_header h;
    bn_int        password_hash1[5];
} t_client_w3xp_logonproofreq PACKED_ATTR();</PRE></BODY></HTML>
