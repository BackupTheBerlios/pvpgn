/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999,2000  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2003 Dizzy 
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
 #include "compat/socket.h"
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
#include "common/util.h"
#include "common/bnethash.h"
#include "common/bnethashconv.h"
#include "common/bn_type.h"
#include "common/field_sizes.h"
#include "ladder.h"
#include "adbanner.h"
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
#include "anongame.h"
#include "common/proginfo.h"
#include "handle_bnet.h"
#include "handlers.h"
#include "common/setup_after.h"
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include "compat/netinet_in.h"
#include "watch.h"

static int compar(const void* a, const void* b);

/* handlers prototypes */
static int _client_unknown_1b(t_connection * c, t_packet const * const packet);
static int _client_compinfo1(t_connection * c, t_packet const * const packet);
static int _client_compinfo2(t_connection * c, t_packet const * const packet);
static int _client_countryinfo1(t_connection * c, t_packet const * const packet);
static int _client_countryinfo109(t_connection * c, t_packet const * const packet);
static int _client_unknown2b(t_connection * c, t_packet const * const packet);
static int _client_progident(t_connection * c, t_packet const * const packet);
static int _client_createaccountw3(t_connection * c, t_packet const * const packet);
static int _client_createacctreq1(t_connection * c, t_packet const * const packet);
static int _client_createacctreq2(t_connection * c, t_packet const * const packet);
static int _client_changepassreq(t_connection * c, t_packet const * const packet);
static int _client_echoreply(t_connection * c, t_packet const * const packet);
static int _client_authreq1(t_connection * c, t_packet const * const packet);
static int _client_authreq109(t_connection * c, t_packet const * const packet);
static int _client_regsnoopreply(t_connection * c, t_packet const * const packet);
static int _client_iconreq(t_connection * c, t_packet const * const packet);
static int _client_cdkey(t_connection * c, t_packet const * const packet);
static int _client_cdkey2(t_connection * c, t_packet const * const packet);
static int _client_cdkey3(t_connection * c, t_packet const * const packet);
static int _client_udpok(t_connection * c, t_packet const * const packet);
static int _client_fileinforeq(t_connection * c, t_packet const * const packet);
static int _client_statsreq(t_connection * c, t_packet const * const packet);
static int _client_loginreq1(t_connection * c, t_packet const * const packet);
static int _client_loginreq2(t_connection * c, t_packet const * const packet);
static int _client_loginreqw3(t_connection * c, t_packet const * const packet);
static int _client_pingreq(t_connection * c, t_packet const * const packet);
static int _client_logonproofreq(t_connection * c, t_packet const * const packet);
static int _client_changegameport(t_connection * c, t_packet const * const packet);
static int _client_friendslistreq(t_connection * c, t_packet const * const packet);
static int _client_friendinforeq(t_connection * c, t_packet const * const packet);
static int _client_atfriendscreen(t_connection * c, t_packet const * const packet);
static int _client_atinvitefriend(t_connection * c, t_packet const * const packet);
static int _client_atacceptdeclineinvite(t_connection * c, t_packet const * const packet);
static int _client_findanongame(t_connection * c, t_packet const * const packet);
static int _client_motdw3(t_connection * c, t_packet const * const packet);
static int _client_realmlistreq(t_connection * c, t_packet const * const packet);
static int _client_realmjoinreq(t_connection * c, t_packet const * const packet);
static int _client_realmjoinreq109(t_connection * c, t_packet const * const packet);
static int _client_unknown39(t_connection * c, t_packet const * const packet);
static int _client_charlistreq(t_connection * c, t_packet const * const packet);
static int _client_adreq(t_connection * c, t_packet const * const packet);
static int _client_adack(t_connection * c, t_packet const * const packet);
static int _client_adclick(t_connection * c, t_packet const * const packet);
static int _client_adclick2(t_connection * c, t_packet const * const packet);
static int _client_statsupdate(t_connection * c, t_packet const * const packet);
static int _client_playerinforeq(t_connection * c, t_packet const * const packet);
static int _client_progident2(t_connection * c, t_packet const * const packet);
static int _client_joinchannel(t_connection * c, t_packet const * const packet);
static int _client_message(t_connection * c, t_packet const * const packet);
static int _client_gamelistreq(t_connection * c, t_packet const * const packet);
static int _client_joingame(t_connection * c, t_packet const * const packet);
static int _client_startgame1(t_connection * c, t_packet const * const packet);
static int _client_startgame3(t_connection * c, t_packet const * const packet);
static int _client_startgame4(t_connection * c, t_packet const * const packet);
static int _client_closegame(t_connection * c, t_packet const * const packet);
static int _client_gamereport(t_connection * c, t_packet const * const packet);
static int _client_leavechannel(t_connection * c, t_packet const * const packet);
static int _client_ladderreq(t_connection * c, t_packet const * const packet);
static int _client_laddersearchreq(t_connection * c, t_packet const * const packet);
static int _client_mapauthreq1(t_connection * c, t_packet const * const packet);
static int _client_mapauthreq2(t_connection * c, t_packet const * const packet);


/* connection state connected handler table */
static t_htable_row bnet_htable_con[] = {
     { CLIENT_UNKNOWN_1B,       _client_unknown_1b},
     { CLIENT_COMPINFO1,        _client_compinfo1},
     { CLIENT_COMPINFO2,        _client_compinfo2},
     { CLIENT_COUNTRYINFO1,     _client_countryinfo1},
     { CLIENT_COUNTRYINFO_109,  _client_countryinfo109},
     { CLIENT_UNKNOWN_2B,       _client_unknown2b},
     { CLIENT_PROGIDENT,        _client_progident},
     { CLIENT_CLOSEGAME,        NULL},
     { CLIENT_CREATEACCOUNT_W3, _client_createaccountw3},
     { CLIENT_CREATEACCTREQ1,   _client_createacctreq1},
     { CLIENT_CREATEACCTREQ2,   _client_createacctreq2},
     { CLIENT_CHANGEPASSREQ,    _client_changepassreq},
     { CLIENT_ECHOREPLY,        _client_echoreply},
     { CLIENT_AUTHREQ1,         _client_authreq1},
     { CLIENT_AUTHREQ_109,      _client_authreq109},
     { CLIENT_REGSNOOPREPLY,    _client_regsnoopreply},
     { CLIENT_ICONREQ,          _client_iconreq},
     { CLIENT_CDKEY,            _client_cdkey},
     { CLIENT_CDKEY2,           _client_cdkey2},
     { CLIENT_CDKEY3,           _client_cdkey3},
     { CLIENT_UDPOK,            _client_udpok},
     { CLIENT_FILEINFOREQ,      _client_fileinforeq},
     { CLIENT_STATSREQ,         _client_statsreq},
     { CLIENT_PINGREQ,          _client_pingreq},
     { CLIENT_LOGINREQ1,        _client_loginreq1},
     { CLIENT_LOGINREQ2,        _client_loginreq2},
     { CLIENT_LOGINREQ_W3,      _client_loginreqw3},
     { CLIENT_LOGONPROOFREQ,    _client_logonproofreq},
     { -1,                       NULL}
};

/* connection state loggedin handlers */
static t_htable_row bnet_htable_log [] = {
     { CLIENT_CHANGEGAMEPORT,   _client_changegameport},
     { CLIENT_FRIENDSLISTREQ,   _client_friendslistreq},
     { CLIENT_FRIENDINFOREQ,    _client_friendinforeq},
     { CLIENT_ARRANGEDTEAM_FRIENDSCREEN, _client_atfriendscreen},
     { CLIENT_ARRANGEDTEAM_INVITE_FRIEND, _client_atinvitefriend},
     { CLIENT_ARRANGEDTEAM_ACCEPT_DECLINE_INVITE, _client_atacceptdeclineinvite},
     { CLIENT_FINDANONGAME,     _client_findanongame},
     { CLIENT_FILEINFOREQ,      _client_fileinforeq},
     { CLIENT_MOTD_W3,          _client_motdw3},
     { CLIENT_REALMLISTREQ,     _client_realmlistreq},
     { CLIENT_REALMJOINREQ,     _client_realmjoinreq},
     { CLIENT_REALMJOINREQ_109, _client_realmjoinreq109},
     { CLIENT_UNKNOWN_37,       _client_charlistreq},
     { CLIENT_UNKNOWN_39,       _client_unknown39},
     { CLIENT_ECHOREPLY,        _client_echoreply},
     { CLIENT_PINGREQ,          _client_pingreq},
     { CLIENT_ADREQ,            _client_adreq},
     { CLIENT_ADACK,            _client_adack},
     { CLIENT_ADCLICK,          _client_adclick},
     { CLIENT_ADCLICK2,         _client_adclick2},
     { CLIENT_STATSREQ,         _client_statsreq},
     { CLIENT_STATSUPDATE,      _client_statsupdate},
     { CLIENT_PLAYERINFOREQ,    _client_playerinforeq},
     { CLIENT_PROGIDENT2,       _client_progident2},
     { CLIENT_JOINCHANNEL,      _client_joinchannel},
     { CLIENT_MESSAGE,          _client_message},
     { CLIENT_GAMELISTREQ,      _client_gamelistreq},
     { CLIENT_JOIN_GAME,        _client_joingame},
     { CLIENT_STARTGAME1,       _client_startgame1},
     { CLIENT_STARTGAME3,       _client_startgame3},
     { CLIENT_STARTGAME4,       _client_startgame4},
     { CLIENT_CLOSEGAME,        _client_closegame},
     { CLIENT_GAME_REPORT,      _client_gamereport},
     { CLIENT_LEAVECHANNEL,     _client_leavechannel},
     { CLIENT_LADDERREQ,        _client_ladderreq},
     { CLIENT_LADDERSEARCHREQ,  _client_laddersearchreq},
     { CLIENT_MAPAUTHREQ1,      _client_mapauthreq1},
     { CLIENT_MAPAUTHREQ2,      _client_mapauthreq2},
     { -1,                      NULL}
};

/* main handler function */
static int handle(t_htable_row * htable, int type, t_connection * c, t_packet const * const packet);

extern int handle_bnet_packet(t_connection * c, t_packet const * const packet)
{
   if (!c)
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got NULL connection",conn_get_socket(c));
	return -1;
     }
   if (!packet)
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got NULL packet",conn_get_socket(c));
	return -1;
     }
   if (packet_get_class(packet) != packet_class_bnet)
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad packet (class %d)",conn_get_socket(c),(int)packet_get_class(packet));
	return -1;
     }
   
   switch (conn_get_state(c)) {
    case conn_state_connected:
      switch (handle(bnet_htable_con, packet_get_type(packet), c, packet)) {
       case 1:
	 eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown (unlogged in) bnet packet type 0x%04x, len %u",conn_get_socket(c),packet_get_type(packet),packet_get_size(packet));
	 break;
       case -1:
	 eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got error handling packet type 0x%04x, len %u", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
	 break;
      };
      break;

    case conn_state_loggedin:
      switch (handle(bnet_htable_log, packet_get_type(packet), c, packet)) {
       case 1:
	 eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown (logged in) bnet packet type 0x%04x, len %u",conn_get_socket(c),packet_get_type(packet),packet_get_size(packet));
	 break;
       case -1:
	 eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got error handling packet type 0x%04x, len %u", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
	 break;
      };
      break;

    case conn_state_untrusted:
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown (untrusted) bnet packet type 0x%04x, len %u",conn_get_socket(c),packet_get_type(packet),packet_get_size(packet));
      break;
      
    default:
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] invalid login state %d",conn_get_socket(c),conn_get_state(c));
   };
       
    return 0;
}

static int compar(const void* a, const void* b)
{
	return strcasecmp(*(char **)a, *(char **)b);
}

static int handle(t_htable_row *htable, int type, t_connection * c, t_packet const * const packet)
{
   t_htable_row *p;
   int res = 1;
   
   for(p = htable; p->type != -1; p++)
     if (p->type == type) {
	res = 0;
	if (p->handler != NULL) res = p->handler(c, packet);
	break;
     }
   
   return res;
}

/* handlers for bnet packets */
static int _client_unknown_1b(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_unknown_1b))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad UNKNOWN_1B packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_unknown_1b),packet_get_size(packet));
	return -1;
     }
   
     {
	unsigned int   newip;
	unsigned short newport;
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] UNKNOWN_1B unknown1=0x%04hx",conn_get_socket(c),bn_short_get(packet->u.client_unknown_1b.unknown1));
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] UNKNOWN_1B unknown2=0x%08x",conn_get_socket(c),bn_int_get(packet->u.client_unknown_1b.unknown2));
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] UNKNOWN_1B unknown3=0x%08x",conn_get_socket(c),bn_int_get(packet->u.client_unknown_1b.unknown3));
	
	newip = bn_int_nget(packet->u.client_unknown_1b.ip);
	newport = bn_short_nget(packet->u.client_unknown_1b.port);
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] UNKNOWN_1B set new UDP address to %s",conn_get_socket(c),addr_num_to_addr_str(newip,newport));
	conn_set_game_addr(c,newip);
	conn_set_game_port(c,newport);
     }
   return 0;
}

static int _client_compinfo1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_compinfo1))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_compinfo1),packet_get_size(packet));
	return -1;
     }
	    
     {
	char const * host;
	char const * user;
	
	if (!(host = packet_get_str_const(packet,sizeof(t_client_compinfo1),MAX_WINHOST_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO1 packet (missing or too long host)",conn_get_socket(c));
	     return -1;
	  }
	if (!(user = packet_get_str_const(packet,sizeof(t_client_compinfo1)+strlen(host)+1,MAX_WINUSER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO1 packet (missing or too long user)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_host(c,host);
	conn_set_user(c,user);
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_compreply));
	packet_set_type(rpacket,SERVER_COMPREPLY);
	bn_int_set(&rpacket->u.server_compreply.reg_version,SERVER_COMPREPLY_REG_VERSION);
	bn_int_set(&rpacket->u.server_compreply.reg_auth,SERVER_COMPREPLY_REG_AUTH);
	bn_int_set(&rpacket->u.server_compreply.client_id,SERVER_COMPREPLY_CLIENT_ID);
	bn_int_set(&rpacket->u.server_compreply.client_token,SERVER_COMPREPLY_CLIENT_TOKEN);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_sessionkey1));
	packet_set_type(rpacket,SERVER_SESSIONKEY1);
	bn_int_set(&rpacket->u.server_sessionkey1.sessionkey,conn_get_sessionkey(c));
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   return 0;
}

static int _client_compinfo2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_compinfo2))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_compinfo2),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * host;
	char const * user;
	
	if (!(host = packet_get_str_const(packet,sizeof(t_client_compinfo2),MAX_WINHOST_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO2 packet (missing or too long host)",conn_get_socket(c));
	     return -1;
	  }
	if (!(user = packet_get_str_const(packet,sizeof(t_client_compinfo2)+strlen(host)+1,MAX_WINUSER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COMPINFO2 packet (missing or too long user)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_host(c,host);
	conn_set_user(c,user);
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_compreply));
	packet_set_type(rpacket,SERVER_COMPREPLY);
	bn_int_set(&rpacket->u.server_compreply.reg_version,SERVER_COMPREPLY_REG_VERSION);
	bn_int_set(&rpacket->u.server_compreply.reg_auth,SERVER_COMPREPLY_REG_AUTH);
	bn_int_set(&rpacket->u.server_compreply.client_id,SERVER_COMPREPLY_CLIENT_ID);
	bn_int_set(&rpacket->u.server_compreply.client_token,SERVER_COMPREPLY_CLIENT_TOKEN);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_sessionkey2));
	packet_set_type(rpacket,SERVER_SESSIONKEY2);
	bn_int_set(&rpacket->u.server_sessionkey2.sessionnum,conn_get_sessionnum(c));
	bn_int_set(&rpacket->u.server_sessionkey2.sessionkey,conn_get_sessionkey(c));
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_countryinfo1(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_countryinfo1))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_countryinfo1),packet_get_size(packet));
	return -1;
     }
     {
	char const * langstr;
	char const * countrycode;
	char const * country;
	char const * countryname;
	unsigned int tzbias;
	
	if (!(langstr = packet_get_str_const(packet,sizeof(t_client_countryinfo1),MAX_LANG_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO1 packet (missing or too long langstr)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(countrycode = packet_get_str_const(packet,sizeof(t_client_countryinfo1)+strlen(langstr)+1,MAX_COUNTRYCODE_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO1 packet (missing or too long countrycode)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(country = packet_get_str_const(packet,sizeof(t_client_countryinfo1)+strlen(langstr)+1+strlen(countrycode)+1,MAX_COUNTRY_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO1 packet (missing or too long country)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(countryname = packet_get_str_const(packet,sizeof(t_client_countryinfo1)+strlen(langstr)+1+strlen(countrycode)+1+strlen(country)+1,MAX_COUNTRYNAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO1 packet (missing or too long countryname)",conn_get_socket(c));
	     return -1;
	  }
	
	tzbias = bn_int_get(packet->u.client_countryinfo1.bias);
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] COUNTRYINFO1 packet from tzbias=0x%04x(%+d) langstr=%s countrycode=%s country=%s",tzbias,uint32_to_int(tzbias),conn_get_socket(c),langstr,countrycode,country);
	conn_set_country(c,country);
	conn_set_tzbias(c,uint32_to_int(tzbias));
     }
   return 0;
}

static int _client_countryinfo109(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_countryinfo_109))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO_109 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_countryinfo_109),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * langstr;
	char const * countryname;
	unsigned int tzbias;
	
	if (!(langstr = packet_get_str_const(packet,sizeof(t_client_countryinfo_109),MAX_LANG_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO_109 packet (missing or too long langstr)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(countryname = packet_get_str_const(packet,sizeof(t_client_countryinfo_109)+strlen(langstr)+1,MAX_COUNTRYNAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad COUNTRYINFO_109 packet (missing or too long countryname)",conn_get_socket(c));
	     return -1;
	  }
	
	tzbias = bn_int_get(packet->u.client_countryinfo_109.bias);
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] COUNTRYINFO_109 packet tzbias=0x%04x(%+d) lcid=%u langid=%u arch=%04x client=%04x versionid=%04x",conn_get_socket(c),
		 tzbias,uint32_to_int(tzbias),
		 bn_int_get(packet->u.client_countryinfo_109.lcid),
		 bn_int_get(packet->u.client_countryinfo_109.langid),
		 bn_int_get(packet->u.client_countryinfo_109.archtag),
		 bn_int_get(packet->u.client_countryinfo_109.clienttag),
		 bn_int_get(packet->u.client_countryinfo_109.versionid));
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] COUNTRYINFO_109 packet from \"%s\" \"%s\"", conn_get_socket(c),countryname,langstr);
	
	conn_set_country(c,langstr); /* FIXME: This isn't right.  We want USA not ENU (English-US) */
	conn_set_tzbias(c,uint32_to_int(tzbias));
	
	conn_set_versionid(c,bn_int_get(packet->u.client_countryinfo_109.versionid));
	
	if (bn_int_tag_eq(packet->u.client_countryinfo_109.archtag,ARCHTAG_WINX86)==0)
	  conn_set_archtag(c,ARCHTAG_WINX86);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.archtag,ARCHTAG_MACPPC)==0)
	  conn_set_archtag(c,ARCHTAG_MACPPC);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.archtag,ARCHTAG_OSXPPC)==0)
	  conn_set_archtag(c,ARCHTAG_OSXPPC);
	else
	  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown client arch 0x%08x, don't expect this to work",conn_get_socket(c),bn_int_get(packet->u.client_countryinfo_109.archtag));
	
	if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_STARCRAFT)==0)
	  conn_set_clienttag(c,CLIENTTAG_STARCRAFT);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_BROODWARS)==0)
	  conn_set_clienttag(c,CLIENTTAG_BROODWARS);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_SHAREWARE)==0)
	  conn_set_clienttag(c,CLIENTTAG_SHAREWARE);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_DIABLORTL)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLORTL);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_WARCIIBNE)==0)
	  conn_set_clienttag(c,CLIENTTAG_WARCIIBNE);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_DIABLO2DV)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLO2DV);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_DIABLO2XP)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLO2XP);
	else if (bn_int_tag_eq(packet->u.client_countryinfo_109.clienttag,CLIENTTAG_WARCRAFT3)==0)
	  conn_set_clienttag(c,CLIENTTAG_WARCRAFT3);
	else
	  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown client program type 0x%08x, don't expect this to work",conn_get_socket(c),bn_int_get(packet->u.client_countryinfo_109.clienttag));
	
	/* First, send an ECHO_REQ */
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_echoreq));
	     packet_set_type(rpacket,SERVER_ECHOREQ);
	     bn_int_set(&rpacket->u.server_echoreq.ticks,get_ticks());
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     t_versioncheck * vc;
	     
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] selecting version check",conn_get_socket(c));
	     vc = versioncheck_create(conn_get_archtag(c),conn_get_clienttag(c));
	     conn_set_versioncheck(c,vc);
	     packet_set_size(rpacket,sizeof(t_server_authreq_109));
	     packet_set_type(rpacket,SERVER_AUTHREQ_109);
	     
	     // WarCraft III expects a different value than other clients
	     eventlog(eventlog_level_trace,"handle_bnet_packet","conn_get_clienttag(c)=%s (WCIII is %s)",
		      conn_get_clienttag(c), CLIENTTAG_WARCRAFT3);
	     if (strcmp(conn_get_clienttag(c),CLIENTTAG_WARCRAFT3)==0) {
		eventlog(eventlog_level_trace,"handle_bnet_packet","Responding with WarCraft III AUTHREQ response.");
		bn_int_set(&rpacket->u.server_authreq_109.unknown1,SERVER_AUTHREQ_109_UNKNOWN1_W3);
	     } else {
		bn_int_set(&rpacket->u.server_authreq_109.unknown1,SERVER_AUTHREQ_109_UNKNOWN1);
	     }
	     
	     bn_int_set(&rpacket->u.server_authreq_109.sessionkey,conn_get_sessionkey(c));
	     bn_int_set(&rpacket->u.server_authreq_109.sessionnum,conn_get_sessionnum(c));
	     file_to_mod_time(versioncheck_get_mpqfile(vc),&rpacket->u.server_authreq_109.timestamp);
	     packet_append_string(rpacket,versioncheck_get_mpqfile(vc));
	     packet_append_string(rpacket,versioncheck_get_eqn(vc));
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] selected \"%s\" \"%s\"",conn_get_socket(c),versioncheck_get_mpqfile(vc),versioncheck_get_eqn(vc));                   
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   return 0;
}

static int _client_unknown2b(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_unknown_2b)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad UNKNOWN_2B packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_unknown_2b),packet_get_size(packet));
      return -1;
   }
   return 0;
}

static int _client_progident(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_progident))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PROGIDENT packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_progident),packet_get_size(packet));
	return -1;
     }
   
   eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_PROGIDENT archtag=0x%08x clienttag=0x%08x versionid=0x%08x unknown1=0x%08x",
	    conn_get_socket(c),
	    bn_int_get(packet->u.client_progident.archtag),
	    bn_int_get(packet->u.client_progident.clienttag),
	    bn_int_get(packet->u.client_progident.versionid),
	    bn_int_get(packet->u.client_progident.unknown1));
   
   if (bn_int_tag_eq(packet->u.client_progident.archtag,ARCHTAG_WINX86)==0)
     conn_set_archtag(c,ARCHTAG_WINX86);
   else if (bn_int_tag_eq(packet->u.client_progident.archtag,ARCHTAG_MACPPC)==0)
     conn_set_archtag(c,ARCHTAG_MACPPC);
   else if (bn_int_tag_eq(packet->u.client_progident.archtag,ARCHTAG_OSXPPC)==0)
     conn_set_archtag(c,ARCHTAG_OSXPPC);
   else
     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown client arch 0x%08x, don't expect this to work",conn_get_socket(c),bn_int_get(packet->u.client_progident.archtag));
   
   if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_STARCRAFT)==0)
     conn_set_clienttag(c,CLIENTTAG_STARCRAFT);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_BROODWARS)==0)
     conn_set_clienttag(c,CLIENTTAG_BROODWARS);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_SHAREWARE)==0)
     conn_set_clienttag(c,CLIENTTAG_SHAREWARE);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLORTL)==0)
     conn_set_clienttag(c,CLIENTTAG_DIABLORTL);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLOSHR)==0)
     conn_set_clienttag(c,CLIENTTAG_DIABLOSHR);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_WARCIIBNE)==0)
     conn_set_clienttag(c,CLIENTTAG_WARCIIBNE);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2DV)==0)
     conn_set_clienttag(c,CLIENTTAG_DIABLO2DV);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2XP)==0)
     conn_set_clienttag(c,CLIENTTAG_DIABLO2XP);
   else if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_WARCRAFT3)==0)
     conn_set_clienttag(c,CLIENTTAG_WARCRAFT3);
   else
     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown client program type 0x%08x, don't expect this to work",conn_get_socket(c),bn_int_get(packet->u.client_progident.clienttag));
   
   if (prefs_get_skip_versioncheck())
     {
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] attempting to skip version check by sending early authreply",conn_get_socket(c));
	/* skip over SERVER_AUTHREQ1 and CLIENT_AUTHREQ1 */
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_authreply1));
	     packet_set_type(rpacket,SERVER_AUTHREPLY1);
	     if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2XP)==0)
	       bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_D2XP_MESSAGE_OK);
	     else
	       bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_MESSAGE_OK);
	     packet_append_string(rpacket,"");
	     packet_append_string(rpacket,""); /* FIXME: what's the second string for? */
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   else
     {
	t_versioncheck * vc;
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] selecting version check",conn_get_socket(c));
	vc = versioncheck_create(conn_get_archtag(c),conn_get_clienttag(c));
	conn_set_versioncheck(c,vc);
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_authreq1));
	     packet_set_type(rpacket,SERVER_AUTHREQ1);
	     file_to_mod_time(versioncheck_get_mpqfile(vc),&rpacket->u.server_authreq1.timestamp);
	     packet_append_string(rpacket,versioncheck_get_mpqfile(vc));
	     packet_append_string(rpacket,versioncheck_get_eqn(vc));
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] selected \"%s\" \"%s\"",conn_get_socket(c),versioncheck_get_mpqfile(vc),versioncheck_get_eqn(vc));
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }

   return 0;
}

static int _client_createaccountw3(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_createaccount_w3))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] (W3) got bad CREATEACCOUNT_W3 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_createaccount_w3),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  temp;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_createaccount_w3),UNCHECKED_NAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] (W3) got bad CREATEACCOUNT_W3 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) new account requested for \"%s\"",conn_get_socket(c),username);
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_createaccount_w3));
	packet_set_type(rpacket,SERVER_CREATEACCOUNT_W3);
	
	if (prefs_get_allow_new_accounts()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) account not created (disabled)",conn_get_socket(c));
	     bn_int_set(&rpacket->u.server_createaccount_w3.result,SERVER_CREATEACCOUNT_W3_RESULT_NO);
	  }
	else
	  {
	     // [zap-zero] 20020521 - support for plaintext passwords at w3 login screen
	     char const *plainpass = packet_get_str_const(packet,4+8*4,16);
	     char upass[20];
	     char lpass[20];
	     t_hash sc_hash;
	     unsigned int i;
	     
	     if (!plainpass) {
		eventlog(eventlog_level_error,"handle_bnet_packet","[%d] (W3) got bad CREATEACCOUNT_W3 (missing password)",conn_get_socket(c));
		bn_int_set(&rpacket->u.server_createaccount_w3.result,SERVER_CREATEACCOUNT_W3_RESULT_NO);
	     } else {
		/* convert plaintext password to uppercase */
		strncpy(upass,plainpass,16);
		upass[16] = 0;
		for (i=0; i<strlen(upass); i++)
		  if (isascii((int)upass[i]) && islower((int)upass[i]))
		    upass[i] = toupper((int)upass[i]);
		
		/* convert plaintext password to lowercase for sc etc. */
		strncpy(lpass,plainpass,16);
		lpass[16] = 0;
		for (i=0; i<strlen(lpass); i++)
		  if (isascii((int)lpass[i]) && isupper((int)lpass[i]))
		    lpass[i] = tolower((int)lpass[i]);
		
		
		//set password hash for sc etc.
		
		bnet_hash(&sc_hash, strlen(lpass), lpass);
		if (!(temp = account_create(username,hash_get_str(sc_hash))))
		  {
		     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) account not created (failed)",conn_get_socket(c));
		     bn_int_set(&rpacket->u.server_createaccount_w3.result,SERVER_CREATEACCOUNT_W3_RESULT_NO);
		  }
		else if (!accountlist_add_account(temp))
		  {
		     account_destroy(temp);
		     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) account not inserted",conn_get_socket(c));
		     bn_int_set(&rpacket->u.server_createaccount_w3.result,SERVER_CREATEACCOUNT_W3_RESULT_NO);
		  } else {
		     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account created",conn_get_socket(c));
		     bn_int_set(&rpacket->u.server_createaccount_w3.result,SERVER_CREATEACCOUNT_W3_RESULT_OK);
		     
		  }
	     }
	  }
	bn_int_set(&rpacket->u.server_createacctreply1.unknown1,SERVER_CREATEACCTREPLY1_UNKNOWN1);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown2,SERVER_CREATEACCTREPLY1_UNKNOWN2);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown3,SERVER_CREATEACCTREPLY1_UNKNOWN3);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown4,SERVER_CREATEACCTREPLY1_UNKNOWN4);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }

   return 0;
}

static int _client_createacctreq1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_createacctreq1))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CREATEACCTREQ1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_createacctreq1),packet_get_size(packet));
	return -1;
     }
   
#ifdef WITH_BITS
   if (!bits_master) { /* BITS client */
      const char * username;
      
      if (!(username = packet_get_str_const(packet,sizeof(t_client_createacctreq1),UNCHECKED_NAME_STR)))
	{
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CREATEACCTREQ1 (missing or too long username)",conn_get_socket(c));
	   return -1;
	}
      
      send_bits_va_create_req(c,username,packet->u.client_createacctreq1.password_hash1);
   } else /* standalone or BITS master */
#endif
     {
	char const * username;
	t_account *  temp;
	t_hash       newpasshash1;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_createacctreq1),UNCHECKED_NAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CREATEACCTREQ1 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] new account requested for \"%s\"",conn_get_socket(c),username);
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_createacctreply1));
	packet_set_type(rpacket,SERVER_CREATEACCTREPLY1);
	
	if (prefs_get_allow_new_accounts()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not created (disabled)",conn_get_socket(c));
	     bn_int_set(&rpacket->u.server_createacctreply1.result,SERVER_CREATEACCTREPLY1_RESULT_NO);
	  }
	else
	  {
	     bnhash_to_hash(packet->u.client_createacctreq1.password_hash1,&newpasshash1);
	     if (!(temp = account_create(username,hash_get_str(newpasshash1))))
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not created (failed)",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply1.result,SERVER_CREATEACCTREPLY1_RESULT_NO);
	       }
	     else if (!accountlist_add_account(temp))
	       {
		  account_destroy(temp);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not inserted",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply1.result,SERVER_CREATEACCTREPLY1_RESULT_NO);
	       }
	     else
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account created",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply1.result,SERVER_CREATEACCTREPLY1_RESULT_OK);
	       }
	  }
	bn_int_set(&rpacket->u.server_createacctreply1.unknown1,SERVER_CREATEACCTREPLY1_UNKNOWN1);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown2,SERVER_CREATEACCTREPLY1_UNKNOWN2);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown3,SERVER_CREATEACCTREPLY1_UNKNOWN3);
	bn_int_set(&rpacket->u.server_createacctreply1.unknown4,SERVER_CREATEACCTREPLY1_UNKNOWN4);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_createacctreq2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_createacctreq2))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_CREATEACCTREQ2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_createacctreq2),packet_get_size(packet));
	return -1;
     }
   
#ifdef WITH_BITS
   if (!bits_master) { /* BITS client */
      const char * username;
      
      if (!(username = packet_get_str_const(packet,sizeof(t_client_createacctreq2),UNCHECKED_NAME_STR)))
	{
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_CREATEACCTREQ2 (missing or too long username)",conn_get_socket(c));
	   return -1;
	}
      
      send_bits_va_create_req(c,username,packet->u.client_createacctreq2.password_hash1);
   } else /* standalone or BITS master */
#endif
     {
	char const * username;
	t_account *  temp;
	t_hash       newpasshash1;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_createacctreq2),UNCHECKED_NAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CREATEACCTREQ2 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] new account requested for \"%s\"",conn_get_socket(c),username);
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_createacctreply2));
	packet_set_type(rpacket,SERVER_CREATEACCTREPLY2);
	
	if (prefs_get_allow_new_accounts()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not created (disabled)",conn_get_socket(c));
	     bn_int_set(&rpacket->u.server_createacctreply2.result,SERVER_CREATEACCTREPLY2_RESULT_SHORT);
	  }
	else
	  {
	     bnhash_to_hash(packet->u.client_createacctreq2.password_hash1,&newpasshash1);
	     if (!(temp = account_create(username,hash_get_str(newpasshash1))))
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not created (failed)",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply2.result,SERVER_CREATEACCTREPLY2_RESULT_SHORT); /* FIXME: return reason for failure */
	       }
	     else if (!accountlist_add_account(temp))
	       {
		  account_destroy(temp);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account not inserted",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply2.result,SERVER_CREATEACCTREPLY2_RESULT_SHORT);
	       }
	     else
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] account created",conn_get_socket(c));
		  bn_int_set(&rpacket->u.server_createacctreply2.result,SERVER_CREATEACCTREPLY2_RESULT_OK);
	       }
	  }
	bn_int_set(&rpacket->u.server_createacctreply2.unknown1,SERVER_CREATEACCTREPLY2_UNKNOWN1);
	bn_int_set(&rpacket->u.server_createacctreply2.unknown2,SERVER_CREATEACCTREPLY2_UNKNOWN2);
	bn_int_set(&rpacket->u.server_createacctreply2.unknown3,SERVER_CREATEACCTREPLY2_UNKNOWN3);
	bn_int_set(&rpacket->u.server_createacctreply2.unknown4,SERVER_CREATEACCTREPLY2_UNKNOWN4);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return -1;
}

static int _client_changepassreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_changepassreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CHANGEPASSREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_changepassreq),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  account;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_changepassreq),UNCHECKED_NAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CHANGEPASSREQ (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change requested for \"%s\"",conn_get_socket(c),username);
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_changepassack));
	packet_set_type(rpacket,SERVER_CHANGEPASSACK);
	
	/* fail if logged in or no account */
	if (connlist_find_connection_by_accountname(username) || !(account = accountlist_find_account(username)))
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" refused (no such account)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_FAIL);
	  }
	else if (account_get_auth_changepass(account)==0) /* default to true */
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" refused (no change access)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_FAIL);
	  }
	else if (conn_get_sessionkey(c)!=bn_int_get(packet->u.client_changepassreq.sessionkey))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] password change for \"%s\" refused (expected session key 0x%08x, got 0x%08x)",conn_get_socket(c),username,conn_get_sessionkey(c),bn_int_get(packet->u.client_changepassreq.sessionkey));
	     bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_FAIL);
	  }   
	else
	  {
	     struct
	       {
		  bn_int   ticks;
		  bn_int   sessionkey;
		  bn_int   passhash1[5];
	       }            temp;
	     char const * oldstrhash1;
	     t_hash       oldpasshash1;
	     t_hash       oldpasshash2;
	     t_hash       trypasshash2;
	     t_hash       newpasshash1;
	     char const * tname;
	     
	     
	     if ((oldstrhash1 = account_get_pass(account)))
	       {
		  bn_int_set(&temp.ticks,bn_int_get(packet->u.client_changepassreq.ticks));
		  bn_int_set(&temp.sessionkey,bn_int_get(packet->u.client_changepassreq.sessionkey));
		  if (hash_set_str(&oldpasshash1,oldstrhash1)<0)
		    {
		       account_unget_pass(oldstrhash1);
		       bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1,&newpasshash1);
		       account_set_pass(account,hash_get_str(newpasshash1));
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" successful (bad previous password)",conn_get_socket(c),(tname = account_get_name(account)));
		       account_unget_name(tname);
		       bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
		    }
		  else
		    {
		       account_unget_pass(oldstrhash1);
		       hash_to_bnhash((t_hash const *)&oldpasshash1,temp.passhash1); /* avoid warning */
		       bnet_hash(&oldpasshash2,sizeof(temp),&temp); /* do the double hash */
		       bnhash_to_hash(packet->u.client_changepassreq.oldpassword_hash2,&trypasshash2);
		       
		       if (hash_eq(trypasshash2,oldpasshash2)==1)
			 {
			    bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1,&newpasshash1);
			    account_set_pass(account,hash_get_str(newpasshash1));
			    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" successful (previous password)",conn_get_socket(c),(tname = account_get_name(account)));
			    account_unget_name(tname);
			    bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
			 }
		       else
			 {
			    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" refused (wrong password)",conn_get_socket(c),(tname = account_get_name(account)));
			    account_unget_name(tname);
			    bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_FAIL);
			 }
		    }
	       }
	     else
	       {
		  bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1,&newpasshash1);
		  account_set_pass(account,hash_get_str(newpasshash1));
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] password change for \"%s\" successful (no previous password)",conn_get_socket(c),(tname = account_get_name(account)));
		  account_unget_name(tname);
		  bn_int_set(&rpacket->u.server_changepassack.message,SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
	       }
	  }
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
	
     }
   
   return 0;
}

static int _client_echoreply(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_echoreply))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ECHOREPLY packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_echoreply),packet_get_size(packet));
	return -1;
     }
   
     {
	unsigned int now;
	unsigned int then;
	
	now = get_ticks();
	then = bn_int_get(packet->u.client_echoreply.ticks);
	if (!now || !then || now<then)
	  eventlog(eventlog_level_warn,"handle_bnet_packet","[%d] bad timing in echo reply: now=%u then=%u",conn_get_socket(c),now,then);
	else
	  conn_set_latency(c,now-then);
     }
   
   return 0;
}

static int _client_authreq1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_authreq1))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad AUTHREQ1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_authreq1),packet_get_size(packet));
	return -1;
     }
   
     {
	char         verstr[16];
	char const * exeinfo;
	char const * versiontag;
	unsigned int versionid;
	unsigned int gameversion;
	unsigned int checksum;
	int          failed;
	
	failed = 0;
	if (bn_int_tag_eq(packet->u.client_authreq1.archtag,conn_get_archtag(c))<0)
	  failed = 1;
	if (bn_int_tag_eq(packet->u.client_authreq1.clienttag,conn_get_clienttag(c))<0)
	  failed = 1;
	
	if (!(exeinfo = packet_get_str_const(packet,sizeof(t_client_authreq1),MAX_EXEINFO_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad AUTHREQ1 (missing or too long exeinfo)",conn_get_socket(c));
	     exeinfo = "badexe";
	     failed = 1;
	  }
	versionid = bn_int_get(packet->u.client_authreq1.versionid);
	checksum = bn_int_get(packet->u.client_authreq1.checksum);
	gameversion = bn_int_get(packet->u.client_authreq1.gameversion);
	strcpy(verstr,vernum_to_verstr(gameversion));
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_AUTHREQ1 archtag=0x%08x clienttag=0x%08x verstr=%s exeinfo=\"%s\" versionid=0x%08x gameversion=0x%08x checksum=0x%08x",
		 conn_get_socket(c),
		 bn_int_get(packet->u.client_authreq1.archtag),
		 bn_int_get(packet->u.client_authreq1.clienttag),
		 verstr,
		 exeinfo,
		 versionid,
		 gameversion,
		 checksum);
	
	conn_set_clientver(c,verstr);
	conn_set_clientexe(c,exeinfo);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_authreply1));
	     packet_set_type(rpacket,SERVER_AUTHREPLY1);
	     
	     versiontag = NULL;
	     
	     if (!conn_get_versioncheck(c) && prefs_get_skip_versioncheck())
	       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] skip versioncheck enabled and client did not request validation",conn_get_socket(c));
	     else
	       switch (versioncheck_validate(conn_get_versioncheck(c),
					     conn_get_archtag(c),
					     conn_get_clienttag(c),
					     exeinfo,
					     versionid,
					     gameversion,
					     checksum,
					     &versiontag))
		 {
		  case -1: /* failed test... client has been modified */
		    if (!prefs_get_allow_bad_version())
		      {
			 eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client failed test (marking untrusted)",conn_get_socket(c));
			 failed = 1;
		      }
		    else
		      eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client failed test, allowing anyway",conn_get_socket(c));
		    break;
		  case 0: /* not listed in table... can't tell if client has been modified */
		    if (!prefs_get_allow_unknown_version())
		      {
			 eventlog(eventlog_level_info,"handle_bnet_packet","[%d] unable to test client (marking untrusted)",conn_get_socket(c));
			 failed = 1;
		      }
		    else
		      eventlog(eventlog_level_info,"handle_bnet_packet","[%d] unable to test client, allowing anyway",conn_get_socket(c));
		    break;
		    /* 1 == test passed... client seems to be ok */
		 }
	     
	     if (versiontag)
	     {
	       t_versioncheck * vc;
	       
	       vc = conn_get_versioncheck(c);
	       versioncheck_set_versiontag(vc, versiontag);
	       conn_set_versioncheck(c, vc);
	       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client matches versiontag \"%s\"",conn_get_socket(c),versiontag);
	     }
	     
	     if (failed)
	       {
		  conn_set_state(c,conn_state_untrusted);
		  if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2XP)==0)
		    bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_D2XP_MESSAGE_BADVERSION);
		  else
		    bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_MESSAGE_BADVERSION);
		  packet_append_string(rpacket,"");
	       }
	     else
	       {
		  /* Only handle updates when there is an update file available. */
		  if (autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag)!=NULL)
		    {
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] an upgrade from %s-%s %s(%s) to %s is available \"%s\"",
				conn_get_socket(c),
				conn_get_archtag(c),
				conn_get_clienttag(c),
				verstr,
				versiontag?versiontag:"(none)",
				autoupdate_version(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag),
				autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag));
		       if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2XP)==0)
			 bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_D2XP_MESSAGE_UPDATE);
		       else
			 bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_MESSAGE_UPDATE);
		       packet_append_string(rpacket,autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag));
		    }
		  else
		    {
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] no upgrades from %s %s are available",conn_get_socket(c),conn_get_clienttag(c),verstr);
		       if (bn_int_tag_eq(packet->u.client_progident.clienttag,CLIENTTAG_DIABLO2XP)==0)
			 bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_D2XP_MESSAGE_OK);
		       else
			 bn_int_set(&rpacket->u.server_authreply1.message,SERVER_AUTHREPLY1_MESSAGE_OK);
		       packet_append_string(rpacket,"");
		    }
	       }
	     
	     packet_append_string(rpacket,""); /* FIXME: what's the second string for? */
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_authreq109(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_authreq_109))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad AUTHREQ_109 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_authreq_109),packet_get_size(packet));
	return 0;
     }
   
     {
	char verstr[16];
	char const * exeinfo;
	char const * versiontag;
	unsigned int versionid;
	unsigned int gameversion;
	unsigned int checksum;
	int          failed;
	char const * owner;
	unsigned int count;
	unsigned int pos;
	
	failed = 0;
	count = bn_int_get(packet->u.client_authreq_109.cdkey_number);
	pos = sizeof(t_client_authreq_109) + (count * sizeof(t_cdkey_info));
	
	if (!(exeinfo = packet_get_str_const(packet,pos,MAX_EXEINFO_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad AUTHREQ_109 (missing or too long exeinfo)",conn_get_socket(c));
	     exeinfo = "badexe";
	     failed = 1;
	  }
	conn_set_clientexe(c,exeinfo);
	pos += strlen(exeinfo) + 1;
	
	if (!(owner = packet_get_str_const(packet,pos,MAX_OWNER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad AUTHREQ_109 (missing or too long owner)",conn_get_socket(c)); 
	     owner = ""; /* maybe owner was missing, use empty string */
	  }
	conn_set_owner(c,owner);
	
	versionid = conn_get_versionid(c);
	checksum = bn_int_get(packet->u.client_authreq_109.checksum);
	gameversion = bn_int_get(packet->u.client_authreq_109.gameversion);
	strcpy(verstr,vernum_to_verstr(bn_int_get(packet->u.client_authreq_109.gameversion)));
	conn_set_clientver(c,verstr);
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_AUTHREQ_109 ticks=0x%08x, verstr=%s exeinfo=\"%s\" versionid=0x%08x gameversion=0x%08x checksum=0x%08x",
		 conn_get_socket(c),
		 bn_int_get(packet->u.client_authreq_109.ticks),
		 verstr,
		 exeinfo,
		 versionid,
		 gameversion,
		 checksum);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_authreply_109));
	     packet_set_type(rpacket,SERVER_AUTHREPLY_109);
	     
	     versiontag = NULL;
	     
	     if (!conn_get_versioncheck(c) && prefs_get_skip_versioncheck())
	       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] skip versioncheck enabled and client did not request validation",conn_get_socket(c));
	     else
	       switch (versioncheck_validate(conn_get_versioncheck(c),
					     conn_get_archtag(c),
					     conn_get_clienttag(c),
					     exeinfo,
					     versionid,
					     gameversion,
					     checksum,
					     &versiontag))
		 {
		  case -1: /* failed test... client has been modified */
		    if (!prefs_get_allow_bad_version())
		      {
			 eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client failed test (closing connection)",conn_get_socket(c));
			 failed = 1;
		      }
		    else
		      eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client failed test, allowing anyway",conn_get_socket(c));
		    break;
		  case 0: /* not listed in table... can't tell if client has been modified */
		    if (!prefs_get_allow_unknown_version())
		      {
			 eventlog(eventlog_level_info,"handle_bnet_packet","[%d] unable to test client (closing connection)",conn_get_socket(c));
			 failed = 1;
		      }
		    else
		      eventlog(eventlog_level_info,"handle_bnet_packet","[%d] unable to test client, allowing anyway",conn_get_socket(c));
		    break;
		    /* 1 == test passed... client seems to be ok */
		 }
	     
	     if (versiontag)
	     {
	       t_versioncheck * vc;
	       
	       vc = conn_get_versioncheck(c);
	       versioncheck_set_versiontag(vc, versiontag);
	       conn_set_versioncheck(c, vc);
	       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client matches versiontag \"%s\"",conn_get_socket(c),versiontag);
	     }
	     
	     if (failed)
	       {
		  conn_set_state(c,conn_state_untrusted);
		  bn_int_set(&rpacket->u.server_authreply_109.message,SERVER_AUTHREPLY_109_MESSAGE_BADVERSION);
		  packet_append_string(rpacket,"");
	       }
	     else
	       {
		  /* Only handle updates when there is an update file available. */
		  if (autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag)!=NULL)
		    {
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] an upgrade from %s-%s %s(%s) to %s is available \"%s\"",
				conn_get_socket(c),
				conn_get_archtag(c),
				conn_get_clienttag(c),
				verstr,
				versiontag?versiontag:"(none)",
				autoupdate_version(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag),
				autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag));
		       bn_int_set(&rpacket->u.server_authreply_109.message,SERVER_AUTHREPLY_109_MESSAGE_UPDATE);
		       packet_append_string(rpacket,autoupdate_file(conn_get_archtag(c),conn_get_clienttag(c),verstr,versiontag));
		    }
		  else
		    {
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] no upgrades from %s %s are available",conn_get_socket(c),conn_get_clienttag(c),verstr);
		       bn_int_set(&rpacket->u.server_authreply_109.message,SERVER_AUTHREPLY_109_MESSAGE_OK);
		       packet_append_string(rpacket,"");
		    }
	       }
	     
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;  
}

static int _client_regsnoopreply(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_regsnoopreply)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REGSNOOPREPLY packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_regsnoopreply),packet_get_size(packet));
      return -1;
   }
   return 0;
}

static int _client_iconreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_iconreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ICONREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_iconreq),packet_get_size(packet));
	return -1;
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_iconreply));
	packet_set_type(rpacket,SERVER_ICONREPLY);
	file_to_mod_time(prefs_get_iconfile(),&rpacket->u.server_iconreply.timestamp);
	
	// select the icon file based on the client
	// FIXME: what about diablo II? does it have its own icon file? [Omega]
	if (strcmp(conn_get_clienttag(c),CLIENTTAG_WARCRAFT3)==0)
	    packet_append_string(rpacket,prefs_get_war3_iconfile());
	else if (strcmp(conn_get_clienttag(c),CLIENTTAG_STARCRAFT)==0)
	    packet_append_string(rpacket,prefs_get_star_iconfile());
	else if (strcmp(conn_get_clienttag(c),CLIENTTAG_BROODWARS)==0)
	    packet_append_string(rpacket,prefs_get_star_iconfile());
	else
	    packet_append_string(rpacket,prefs_get_iconfile());
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_cdkey(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_cdkey))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_cdkey),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * cdkey;
	char const * owner;
	
	if (!(cdkey = packet_get_str_const(packet,sizeof(t_client_cdkey),MAX_CDKEY_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY packet (missing or too long cdkey)",conn_get_socket(c));
	     return -1;
	  }
	if (!(owner = packet_get_str_const(packet,sizeof(t_client_cdkey)+strlen(cdkey)+1,MAX_OWNER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY packet (missing or too long owner)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_cdkey(c,cdkey);
	conn_set_owner(c,owner);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_cdkeyreply));
	     packet_set_type(rpacket,SERVER_CDKEYREPLY);
	     bn_int_set(&rpacket->u.server_cdkeyreply.message,SERVER_CDKEYREPLY_MESSAGE_OK);
	     packet_append_string(rpacket,owner);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
#if 0 /* Blizzard used this to track down pirates, should only be accepted by old clients */
   if ((rpacket = packet_create(packet_class_bnet)))
	    {
	       packet_set_size(rpacket,sizeof(t_server_regsnoopreq));
	       packet_set_type(rpacket,SERVER_REGSNOOPREQ);
	       bn_int_set(&rpacket->u.server_regsnoopreq.unknown1,SERVER_REGSNOOPREQ_UNKNOWN1); /* sequence num */
	       bn_int_set(&rpacket->u.server_regsnoopreq.hkey,SERVER_REGSNOOPREQ_HKEY_CURRENT_USER);
	       packet_append_string(rpacket,SERVER_REGSNOOPREQ_REGKEY);
	       packet_append_string(rpacket,SERVER_REGSNOOPREQ_REGVALNAME);
	       queue_push_packet(conn_get_out_queue(c),rpacket);
	       packet_del_ref(rpacket);
	    }
#endif
   return 0;
}

static int _client_cdkey2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_cdkey2))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_cdkey2),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * owner;
	
	if (!(owner = packet_get_str_const(packet,sizeof(t_client_cdkey2),MAX_OWNER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY2 packet (missing or too long owner)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_owner(c,owner);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_cdkeyreply2));
	     packet_set_type(rpacket,SERVER_CDKEYREPLY2);
	     bn_int_set(&rpacket->u.server_cdkeyreply2.message,SERVER_CDKEYREPLY2_MESSAGE_OK);
	     packet_append_string(rpacket,owner);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_cdkey3(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_cdkey3))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY3 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_cdkey2),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * owner;
	
	if (!(owner = packet_get_str_const(packet,sizeof(t_client_cdkey3),MAX_OWNER_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CDKEY3 packet (missing or too long owner)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_owner(c,owner);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_cdkeyreply3));
	     packet_set_type(rpacket,SERVER_CDKEYREPLY3);
	     bn_int_set(&rpacket->u.server_cdkeyreply3.message,SERVER_CDKEYREPLY3_MESSAGE_OK);
	     packet_append_string(rpacket,""); /* FIXME: owner, message, ??? */
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_udpok(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_udpok))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad UDPOK packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_udpok),packet_get_size(packet));
	return -1;
     }
   /* we could check the contents but there really isn't any point */
   conn_set_udpok(c);
   
   return 0;
}

static int _client_fileinforeq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_fileinforeq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad FILEINFOREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_fileinforeq),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * tosfile;
	
	if (!(tosfile = packet_get_str_const(packet,sizeof(t_client_fileinforeq),MAX_FILENAME_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad FILEINFOREQ packet (missing or too long tosfile)",conn_get_socket(c));
	     return -1;
	  }
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] TOS requested: \"%s\" - type = 0x%02x",conn_get_socket(c),tosfile, bn_int_get(packet->u.client_fileinforeq.type));
	
	/* TODO: if type is TOSFILE make bnetd to send default tosfile if selected is not found */ 
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_fileinforeply));
	     packet_set_type(rpacket,SERVER_FILEINFOREPLY);
	     bn_int_set(&rpacket->u.server_fileinforeply.type,bn_int_get(packet->u.client_fileinforeq.type));
	     bn_int_set(&rpacket->u.server_fileinforeply.unknown2,bn_int_get(packet->u.client_fileinforeq.unknown2));
	     /* Note from Sherpya:
	      * timestamp -> 0x852b7d00 - 0x01c0e863 b.net send this (bn_int),
	      * I suppose is not a long 
	      * if bnserver-D2DV is bad diablo 2 crashes 
	      * timestamp doesn't work correctly and starcraft 
	      * needs name in client locale or displays hostname
	      */
	     file_to_mod_time(tosfile,&rpacket->u.server_fileinforeply.timestamp);
	     packet_append_string(rpacket,tosfile);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_statsreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_statsreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STATSREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_statsreq),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * name;
	char const * key;
	unsigned int name_count;
	unsigned int key_count;
	unsigned int i,j;
	unsigned int name_off;
	unsigned int keys_off;
	unsigned int key_off;
	t_account *  account;
	char const * tval;
	char const * tname;
	
	name_count = bn_int_get(packet->u.client_statsreq.name_count);
	key_count = bn_int_get(packet->u.client_statsreq.key_count);
	
	for (i=0,name_off=sizeof(t_client_statsreq);
	     i<name_count && (name = packet_get_str_const(packet,name_off,UNCHECKED_NAME_STR));
	     i++,name_off+=strlen(name)+1);
	if (i<name_count)
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STATSREQ packet (only %u names of %u)",conn_get_socket(c),i,name_count);
	     return -1;
	  }
	keys_off = name_off;
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_statsreply));
	packet_set_type(rpacket,SERVER_STATSREPLY);
	bn_int_set(&rpacket->u.server_statsreply.name_count,name_count);
	bn_int_set(&rpacket->u.server_statsreply.key_count,key_count);
	bn_int_set(&rpacket->u.server_statsreply.unknown1,bn_int_get(packet->u.client_statsreq.unknown1));
	
	for (i=0,name_off=sizeof(t_client_statsreq);
	     i<name_count && (name = packet_get_str_const(packet,name_off,UNCHECKED_NAME_STR));
	     i++,name_off+=strlen(name)+1)
	  {
	     if (!(account = accountlist_find_account(name)))
	     {
	       if ((account = conn_get_account(c)))
	       {
		 eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] client_statsreply no name, use self \"%s\"",conn_get_socket(c),(tname = account_get_name(account)));
		 account_unget_name(tname);
		       
	       }
	     }
	     
	     for (j=0,key_off=keys_off;
		  j<key_count && (key = packet_get_str_const(packet,key_off,MAX_ATTRKEY_STR));
		  j++,key_off+=strlen(key)+1)
	       if (account && (tval = account_get_strattr(account,key)))
		 {
		    packet_append_string(rpacket,tval);
		    account_unget_strattr(tval);
		 }
	     else
	       {
		  packet_append_string(rpacket,""); /* FIXME: what should really happen here? */
		  if (account && key[0]!='\0')
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] no entry \"%s\" in account \"%s\"",conn_get_socket(c),key,(tname = account_get_name(account)));
		       account_unget_name(tname);
		    }
	       }
	  }
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_loginreq1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_loginreq1))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LOGINREQ1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_loginreq1),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  account;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_loginreq1),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LOGINREQ1 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_loginreply1));
	packet_set_type(rpacket,SERVER_LOGINREPLY1);
	
	// too many logins? [added by NonReal]
	if (prefs_get_max_concurrent_logins()>0) {
	   if (prefs_get_max_concurrent_logins()<=connlist_login_get_length()) {
	      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] login denied, too many concurrent logins. max: %d. current: %d.",conn_get_socket(c),prefs_get_max_concurrent_logins(),connlist_login_get_length());
	      bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	      return -1;
	   }
	}
		
	/* already logged in */
	if (connlist_find_connection_by_accountname(username) &&
	    prefs_get_kick_old_login()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (already logged in)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	  }
	else
#ifdef WITH_BITS
	  if (!bits_master)
	    {
	       t_query * q;
	       
	       if (account_name_is_unknown(username)) {
		  if (bits_va_lock_account(username)<0) {
		     eventlog(eventlog_level_error,"handle_bnet_packet","bits_va_lock_account failed");
		     packet_del_ref(rpacket);
		     return -1;
		  }
	       }
	       account = accountlist_find_account(username);
	       
	       if (!account_is_ready_for_use(account)) {
		  q = query_create(bits_query_type_client_loginreq_1);
		  if (!q)
		    eventlog(eventlog_level_error,"handle_bnet_packet","could not create bits query");
		  query_attach_conn(q,"client",c);
		  query_attach_packet_const(q,"request",packet);
		  query_attach_account(q,"account",account);
		  packet_del_ref(rpacket);
		  return -1;
	       }
	       /* Account is ready for use now */
	    }
#endif
	/* fail if no account */
	if (!(account = accountlist_find_account(username)))
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (no such account)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	  }
	else if (account_get_auth_bnetlogin(account)==0) /* default to true */
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (no bnet access)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	  }
	else if (account_get_auth_lock(account)==1) /* default to false */
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (this account is locked)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	  }
	else if (conn_get_sessionkey(c)!=bn_int_get(packet->u.client_loginreq1.sessionkey))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] login for \"%s\" refused (expected session key 0x%08x, got 0x%08x)",conn_get_socket(c),username,conn_get_sessionkey(c),bn_int_get(packet->u.client_loginreq1.sessionkey));
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
	  }
	else
	  {
	     struct
	       {
		  bn_int   ticks;
		  bn_int   sessionkey;
		  bn_int   passhash1[5];
	       }            temp;
	     char const * oldstrhash1;
	     t_hash       oldpasshash1;
	     t_hash       oldpasshash2;
	     t_hash       trypasshash2;
	     char const * tname;
	     
	     if(!strcmp(account_get_pass(account),"a") && account_get_w3_acctpass(account)) {
		char lpass[20];
		unsigned int i;
		
		/* convert plaintext password to lowercase */
		strncpy(lpass,account_get_w3_acctpass(account),16);
		lpass[16] = 0;
		for (i=0; i<strlen(lpass); i++)
		  if (isascii((int)lpass[i]) && isupper((int)lpass[i]))
		    lpass[i] = tolower((int)lpass[i]);
		
		bnet_hash(&oldpasshash1, strlen(lpass), lpass);
		account_set_pass(account, hash_get_str(oldpasshash1));
	     }
	     
	     
	     if ((oldstrhash1 = account_get_pass(account)))
	       {
		  bn_int_set(&temp.ticks,bn_int_get(packet->u.client_loginreq1.ticks));
		  bn_int_set(&temp.sessionkey,bn_int_get(packet->u.client_loginreq1.sessionkey));
		  if (hash_set_str(&oldpasshash1,oldstrhash1)<0)
		    {
		       account_unget_pass(oldstrhash1);
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (corrupted passhash1?)",conn_get_socket(c),(tname = account_get_name(account)));
		       account_unget_name(tname);
		       bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
		    }
		  else
		    {
		       account_unget_pass(oldstrhash1);
		       hash_to_bnhash((t_hash const *)&oldpasshash1,temp.passhash1); /* avoid warning */
		       
		       bnet_hash(&oldpasshash2,sizeof(temp),&temp); /* do the double hash */
		       bnhash_to_hash(packet->u.client_loginreq1.password_hash2,&trypasshash2);
		       
		       if (hash_eq(trypasshash2,oldpasshash2)==1)
			 {
#ifdef WITH_BITS
			    int rc = 0;
			    
			    if (bits_master) {
			       bn_int ct;
			       int sid;
			       
			       if (conn_get_clienttag(c))
				 memcpy(&ct,conn_get_clienttag(c),4);
			       else
				 bn_int_set(&ct,0);
			       tname = account_get_name(account);
			       rc = bits_loginlist_add(account_get_uid(account),
						       BITS_ADDR_MASTER,
						       (sid = bits_va_loginlist_get_free_sessionid()),
						       ct,
						       conn_get_game_addr(c),
						       conn_get_game_port(c),
						       tname);
			       account_unget_name(tname);
			       conn_set_sessionid(c,sid);
			       /* Invoke the playerinfo update logic */
			       conn_get_playerinfo(c);
			       /* Maybe it was already ready for use before: check if we have to create a query */
			    } else if ((!query_current)||(query_get_type(query_current) != bits_query_type_client_loginreq_2)) { /* slave/client server */
			       t_query * q;
			       
			       q = query_create(bits_query_type_client_loginreq_2);
			       if (!q)
				 eventlog(eventlog_level_error,"handle_bnet_packet","could not create bits query");
			       query_attach_conn(q,"client",c);
			       query_attach_packet_const(q,"request",packet);
			       query_attach_account(q,"account",account);
			       packet_del_ref(rpacket);
			       send_bits_va_loginreq(query_get_qid(q),
						     account_get_uid(account),
						     conn_get_game_addr(c),
						     conn_get_game_port(c),
						     conn_get_clienttag(c));
			       return -1;
			    }
			    if (rc == 0) {
#endif
			       conn_set_account(c,account);
			       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" logged in (correct password)",conn_get_socket(c),(tname = conn_get_username(c)));
			       conn_unget_username(c,tname);
			       bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
#ifdef WITH_BITS
			    } else {
			       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (bits_loginlist_add returned %d)",conn_get_socket(c),(tname = account_get_name(account)),rc);
			       account_unget_name(tname);
			       bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
			    }
#endif
			 }
		       else
			 {
			    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (wrong password)",conn_get_socket(c),(tname = account_get_name(account)));
			    account_unget_name(tname);
			    bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
			 }
		    }
	       }
	     else
	       {
		  conn_set_account(c,account);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" logged in (no password)",conn_get_socket(c),(tname = account_get_name(account)));
		  account_unget_name(tname);
		  bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
		  
	       }
	  }
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_loginreq2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_loginreq2))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LOGINREQ2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_loginreq2),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  account;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_loginreq2),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LOGINREQ2 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_loginreply2));
	packet_set_type(rpacket,SERVER_LOGINREPLY2);
	
	// too many logins? [added by NonReal]
	if (prefs_get_max_concurrent_logins()>0) {
	   if (prefs_get_max_concurrent_logins()<=connlist_login_get_length()) {
	      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] login denied, too many concurrent logins. max: %d. current: %d.",conn_get_socket(c),prefs_get_max_concurrent_logins(),connlist_login_get_length());
	      bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
	      return -1;
	   }
	}
	
	/* already logged in */
	if (connlist_find_connection_by_accountname(username) &&
	    prefs_get_kick_old_login()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (already logged in)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_ALREADY);
	  }
	else
#ifdef WITH_BITS
	  if (!bits_master)
	    {
	       if (account_name_is_unknown(username)) {
		  if (bits_va_lock_account(username)<0) {
		     eventlog(eventlog_level_error,"handle_bnet_packet","bits_va_lock_account failed");
		     packet_del_ref(rpacket);
		     return -1;
		  }
	       }
	       account = accountlist_find_account(username);
	    }
#endif
	/* fail if no account */
	if (!(account = accountlist_find_account(username)))
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (no such account)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
	  }
	else if (account_get_auth_bnetlogin(account)==0) /* default to true */
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (no bnet access)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
	  }
	else if (account_get_auth_lock(account)==1) /* default to false */
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (this account is locked)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
	  }
	else if (conn_get_sessionkey(c)!=bn_int_get(packet->u.client_loginreq2.sessionkey))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] login for \"%s\" refused (expected session key 0x%08x, got 0x%08x)",conn_get_socket(c),username,conn_get_sessionkey(c),bn_int_get(packet->u.client_loginreq2.sessionkey));
	     bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
	  }
	else
	  {
	     struct
	       {
		  bn_int   ticks;
		  bn_int   sessionkey;
		  bn_int   passhash1[5];
	       }            temp;
	     char const * oldstrhash1;
	     t_hash       oldpasshash1;
	     t_hash       oldpasshash2;
	     t_hash       trypasshash2;
	     char const * tname;
	     
	     if ((oldstrhash1 = account_get_pass(account)))
	       {
		  bn_int_set(&temp.ticks,bn_int_get(packet->u.client_loginreq2.ticks));
		  bn_int_set(&temp.sessionkey,bn_int_get(packet->u.client_loginreq2.sessionkey));
		  if (hash_set_str(&oldpasshash1,oldstrhash1)<0)
		    {
		       account_unget_pass(oldstrhash1);
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (corrupted passhash1?)",conn_get_socket(c),(tname = account_get_name(account)));
		       account_unget_name(tname);
		       bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
		    }
		  else
		    {
		       account_unget_pass(oldstrhash1);
		       hash_to_bnhash((t_hash const *)&oldpasshash1,temp.passhash1); /* avoid warning */
		       
		       bnet_hash(&oldpasshash2,sizeof(temp),&temp); /* do the double hash */
		       bnhash_to_hash(packet->u.client_loginreq2.password_hash2,&trypasshash2);
		       
		       if (hash_eq(trypasshash2,oldpasshash2)==1)
			 {
#ifdef WITH_BITS
			    int rc = 0;
			    
			    if (bits_master) {
			       bn_int ct;
			       int sid;
			       
			       if (conn_get_clienttag(c))
				 memcpy(&ct,conn_get_clienttag(c),4);
			       else
				 bn_int_set(&ct,0);
			       tname = account_get_name(account);
			       rc = bits_loginlist_add(account_get_uid(account),
						       BITS_ADDR_MASTER,
						       (sid = bits_va_loginlist_get_free_sessionid()),
						       ct,
						       conn_get_game_addr(c),
						       conn_get_game_port(c),
						       tname);
			       account_unget_name(tname);
			       conn_set_sessionid(c,sid);
			       /* Invoke the playerinfo update logic */
			       conn_get_playerinfo(c);				    
			       /* Maybe it was already ready for use before: check if we have to create a query */
			    } else if ((!query_current)||(query_get_type(query_current) != bits_query_type_client_loginreq_2)) { /* slave/client server */
			       t_query * q;
			       
			       q = query_create(bits_query_type_client_loginreq_2);
			       if (!q)
				 eventlog(eventlog_level_error,"handle_bnet_packet","could not create bits query");
			       query_attach_conn(q,"client",c);
			       query_attach_packet_const(q,"request",packet);
			       query_attach_account(q,"account",account);
			       packet_del_ref(rpacket);
			       send_bits_va_loginreq(query_get_qid(q),
						     account_get_uid(account),
						     conn_get_game_addr(c),
						     conn_get_game_port(c),
						     conn_get_clienttag(c));
			       return -1;
			    }
			    if (rc == 0) {
#endif
			       conn_set_account(c,account);
			       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" logged in (correct password)",conn_get_socket(c),(tname = conn_get_username(c)));
			       conn_unget_username(c,tname);
			       bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
#ifdef WITH_BITS
			    } else {
			       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (bits_loginlist_add returned %d)",conn_get_socket(c),(tname = account_get_name(account)),rc);
			       account_unget_name(tname);
			       bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY1_MESSAGE_FAIL);
			    }
#endif
			 }
		       else
			 {
			    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (wrong password)",conn_get_socket(c),(tname = account_get_name(account)));
			    account_unget_name(tname);
			    bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_BADPASS);
			 }
		    }
	       }
	     else
	       {
		  conn_set_account(c,account);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" logged in (no password)",conn_get_socket(c),(tname = account_get_name(account)));
		  account_unget_name(tname);
		  bn_int_set(&rpacket->u.server_loginreply2.message,SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
	       }
	  }
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_loginreqw3(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_loginreq_w3))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_LOGINREQ_W3 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_loginreq_w3),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  account;
	char const * tname;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_loginreq_w3),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_LOGINREQ_W3 (missing or too long username)",conn_get_socket(c));
	     return -1;
	  } else {
	     eventlog(eventlog_level_trace,"handle_bnet_packet","[%d] got username from CLIENT_LOGINREQ_W3 packet: %s",conn_get_socket(c),username);
	  }
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_loginreply_w3));
	packet_set_type(rpacket,SERVER_LOGINREPLY_W3);
	  {
	     int i;
	     for (i=0; i<15; i++)
	       {
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[i], 0);
	       }
	  }
	
	// too many logins? [added by NonReal]
	if (prefs_get_max_concurrent_logins()>0) {
	   if (prefs_get_max_concurrent_logins()<=connlist_login_get_length()) {
	      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] login denied, too many concurrent logins. max: %d. current: %d.",conn_get_socket(c),prefs_get_max_concurrent_logins(),connlist_login_get_length());
	      bn_int_set(&rpacket->u.server_loginreply_w3.message,SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
	   }
	}
	
	/* already logged in */
	else if (connlist_find_connection_by_accountname(username) &&
		 prefs_get_kick_old_login()==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) login for \"%s\" refused (already logged in)",conn_get_socket(c),username);
	     bn_int_set(&rpacket->u.server_loginreply_w3.message,SERVER_LOGINREPLY_W3_MESSAGE_ALREADY);
	  }
	else
	  {
#ifdef WITH_BITS
	     if (!bits_master)
	       {
		  if (account_name_is_unknown(username)) {
		     if (bits_va_lock_account(username)<0) {
			eventlog(eventlog_level_error,"handle_bnet_packet","bits_va_lock_account failed");
			packet_del_ref(rpacket);
			return -1;
		     }
		  }
		  account = accountlist_find_account(username);
	       }
#endif
	     
	     /* fail if no account */
	     account = accountlist_find_account(username);
	     if (!account)
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) login for \"%s\" refused (no such account)",conn_get_socket(c),username);
		  bn_int_set(&rpacket->u.server_loginreply_w3.message,SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
	       }
	     else if (account_get_auth_bnetlogin(account)==0) /* default to true */
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) login for \"%s\" refused (no bnet access)",conn_get_socket(c),username);
		  bn_int_set(&rpacket->u.server_loginreply_w3.message,SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
	       }
	     else if (account_get_auth_lock(account)==1) /* default to false */
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] login for \"%s\" refused (this account is locked)",conn_get_socket(c),username);
		  bn_int_set(&rpacket->u.server_loginreply1.message,SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
	       }
	     else
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) \"%s\" passed account check",conn_get_socket(c),(tname = account_get_name(account)));
		  conn_set_w3_username(c,tname);
		  account_unget_name(tname);
		  bn_int_set(&rpacket->u.server_loginreply_w3.message,SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
		  
		  
		  // this section based on ]{ain
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 0], 0xdc24d96e);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 1], 0xd104c9b2);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 2], 0x419d2f2c);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 3], 0xfad3cce2);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 4], 0x56cdacfa);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 5], 0xceb6cd85);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 6], 0x150475cf);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 7], 0xdda39a85);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 8], 0x816c594e);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[ 9], 0xf8331b22);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[10], 0xe722a8d7);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[11], 0xc3065ae8);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[12], 0xb2e8f8ba);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[13], 0x6ce0cde1);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[14], 0x3fc7e303);
		  bn_int_set (&rpacket->u.server_loginreply_w3.unknown[15], 0x1fa27892);
	       }
	     
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	     
	  }
	
     }
   
   return 0;
}

static int _client_pingreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_pingreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PINGREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_pingreq),packet_get_size(packet));
	return -1;
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_pingreply));
	packet_set_type(rpacket,SERVER_PINGREPLY);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_logonproofreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_logonproofreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LOGONPROOFREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_logonproofreq),packet_get_size(packet));
	return -1;
     }
   
     {
	char const * username;
	t_account *  account;
	char const * tname;
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] logon proof requested",conn_get_socket(c));
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_logonproofreply));
	packet_set_type(rpacket,SERVER_LOGONPROOFREPLY);
	
	bn_int_set(&rpacket->u.server_logonproofreply.response,SERVER_LOGONPROOFREPLY_RESPONSE_BADPASS);
	
	bn_int_set(&rpacket->u.server_logonproofreply.unknown1,SERVER_LOGONPROOFREPLY_UNKNOWN1);
	
	bn_short_set(&rpacket->u.server_logonproofreply.port0,(short)0x0000);
	
	bn_int_set(&rpacket->u.server_logonproofreply.unknown2,SERVER_LOGONPROOFREPLY_UNKNOWN2);
	
	bn_short_set(&rpacket->u.server_logonproofreply.port1,(short)0x0000);
	
	bn_int_set(&rpacket->u.server_logonproofreply.unknown3,SERVER_LOGONPROOFREPLY_UNKNOWN3);
	bn_int_set(&rpacket->u.server_logonproofreply.unknown4,SERVER_LOGONPROOFREPLY_UNKNOWN4);
	
	if (!(username = conn_get_w3_username(c)))
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) got NULL username, 0x54ff before 0x53ff?",conn_get_socket(c));
	  }
	else if (!(account = accountlist_find_account(username)))
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) login in 0x54ff for \"%s\" refused (no such account)",conn_get_socket(c),username);
	  } else {
	     t_hash serverhash;
	     t_hash clienthash;
	     
	     if (!packet_get_data_const(packet,4,20)) {
		eventlog(eventlog_level_error,"handle_bnet_packet","[%d] (W3) got bad LOGONPROOFREQ packet (missing hash)",conn_get_socket(c));
		return -1;
	     }
	     
	     // endian fix
	     clienthash[0] = bn_int_get(packet->u.client_logonproofreq.password_hash1[0]);
	     clienthash[1] = bn_int_get(packet->u.client_logonproofreq.password_hash1[1]);
	     clienthash[2] = bn_int_get(packet->u.client_logonproofreq.password_hash1[2]);
	     clienthash[3] = bn_int_get(packet->u.client_logonproofreq.password_hash1[3]);
	     clienthash[4] = bn_int_get(packet->u.client_logonproofreq.password_hash1[4]);
	     
	     hash_set_str(&serverhash, account_get_pass(account));
	     if(hash_eq(clienthash,serverhash)) {
		conn_set_account(c,account);
		eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) \"%s\" logged in (right password)",conn_get_socket(c),(tname = account_get_name(account)));
		account_unget_name(tname);
		bn_int_set(&rpacket->u.server_logonproofreply.response,SERVER_LOGONPROOFREPLY_RESPONSE_OK);
		// by amadeo updates the userlist
#ifdef WIN32_GUI
		guiOnUpdateUserList();
#endif
	     } else
	       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] (W3) got wrong password for \"%s\"",conn_get_socket(c),(tname = account_get_name(account)));
	     
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_changegameport(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_changegameport)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad changegameport packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_changegameport),packet_get_size(packet));
      return -1;
   }
     {
	unsigned short port = bn_short_get(packet->u.client_changegameport.port);
	if(port <= 1024 || port >= 32767) {
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] invalid port in changegameport packet: %d",conn_get_socket(c), (int)port);
	   return -1;
	}
	
	eventlog(eventlog_level_trace,"handle_bnet_packet","[%d] changing game port to: %d",conn_get_socket(c), (int)port);
	conn_set_game_port(c, port);
     }
   
   return 0;
}

static int _client_friendslistreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_friendslistreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad FRIENDSLISTREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_friendslistreq),packet_get_size(packet));
	return -1;
     }
     {
	
	char tmp[7];
	char const *friend;
	int i;
	int n = account_get_friendcount(conn_get_account(c));
	
	if(n==0)
	  return -1;
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	
	packet_set_size(rpacket,sizeof(t_server_friendslistreply));
	packet_set_type(rpacket,SERVER_FRIENDSLISTREPLY);
	
	
	tmp[0]=(unsigned char) n;    //set number of friends
	packet_append_data(rpacket, tmp, 1);
	
	for(i=0; i<n; i++) {
	   friend = account_get_friend(conn_get_account(c),i);
	   packet_append_string(rpacket, friend);
	   memset(tmp, 0, 7);
	   if(connlist_find_connection_by_accountname(friend))
	     tmp[1] = FRIENDSTATUS_ONLINE;
	   packet_append_data(rpacket, tmp, 7);
	}
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_friendinforeq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_friendinforeq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad FRIENDINFOREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_friendinforeq),packet_get_size(packet));
      return -1;
   }

     {
	t_connection const * dest_c;
	t_game const * game;
	t_channel const * channel;
	t_account * account;
	char const *friend;
	char const *myusername;
	int n = account_get_friendcount(conn_get_account(c));
	
	myusername = conn_get_username(c);
	if(bn_byte_get(packet->u.client_friendinforeq.friendnum) > n) {
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] bad friend number in FRIENDINFOREQ packet",conn_get_socket(c));
	   return -1;
	}
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	
	packet_set_size(rpacket,sizeof(t_server_friendinforeply));
	packet_set_type(rpacket,SERVER_FRIENDINFOREPLY);
	
	friend = account_get_friend(conn_get_account(c),bn_byte_get(packet->u.client_friendinforeq.friendnum));
	if(!friend) 
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] friend number %d not found",conn_get_socket(c), (int) bn_byte_get(packet->u.client_friendinforeq.friendnum));
	     return -1;
	  }
	
	bn_byte_set(&rpacket->u.server_friendinforeply.friendnum, bn_byte_get(packet->u.client_friendinforeq.friendnum));
	// Updated - THEUNDYING 7/26/02 - Mutual Friends Check - Fixed by me w00t w00t
	account = accountlist_find_account(friend); //Get the friends account
	
	if(!(dest_c = connlist_find_connection_by_accountname(friend))) 
	  {
	     bn_byte_set(&rpacket->u.server_friendinforeply.unknown1, 0);
	     bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_OFFLINE);
	     bn_int_set(&rpacket->u.server_friendinforeply.clienttag, 0);
	     packet_append_string(rpacket, "");
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	     return 0;
	  }
	
	if(account_check_mutual(account,myusername)==0)
	  {
	     eventlog(eventlog_level_trace,"handle_bnet","%s is mutual to %s",myusername,friend);
	     bn_byte_set(&rpacket->u.server_friendinforeply.unknown1, 1); // This is if a mutal friend or not 6/28/02 THEUNDYING 0 = no, 1 = yes
	     
	     if((game = conn_get_game(dest_c))) 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_GAME);
		  bn_int_set(&rpacket->u.server_friendinforeply.clienttag, *((int const *)conn_get_clienttag(dest_c)));
		  packet_append_string(rpacket, game_get_name(game));
	       }
	     else if((channel = conn_get_channel(dest_c))) 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_CHAT);
		  bn_int_set(&rpacket->u.server_friendinforeply.clienttag, *((int const *)conn_get_clienttag(dest_c)));
		  packet_append_string(rpacket, channel_get_name(channel)); // Displays actual channel name now THEUNDYING 5/16/2002
		  
	       }
	     else 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_ONLINE);
		  packet_append_string(rpacket, "");
	       }
	  }
	else
	  {
	     eventlog(eventlog_level_trace,"handle_bnet","%s is not mutual to %s",myusername,friend);
	     bn_byte_set(&rpacket->u.server_friendinforeply.unknown1, 0); // This is if a mutal friend or not 6/28/02 THEUNDYING 0 = no, 1 = yes
	     
	     if((game = conn_get_game(dest_c))) 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_GAME);
		  bn_int_set(&rpacket->u.server_friendinforeply.clienttag, *((int const *)conn_get_clienttag(dest_c)));
		  packet_append_string(rpacket, "");
	       }
	     else if((channel = conn_get_channel(dest_c))) 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_CHAT);
		  bn_int_set(&rpacket->u.server_friendinforeply.clienttag, *((int const *)conn_get_clienttag(dest_c)));
		  packet_append_string(rpacket, "Not Mutual"); // Displays actual channel name now THEUNDYING 5/16/2002
	       }
	     else 
	       {
		  bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_ONLINE);
		  packet_append_string(rpacket, "");
	       }
	     
	  }
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
	
     }

   return 0;
}

static int _client_atfriendscreen(t_connection * c, t_packet const * const packet)
{
   char const *f;
   char const *myusername;
   t_connection * dest_c;
   int i;
   unsigned char f_cnt = 0;
   t_packet * rpacket = NULL;
   
   eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got CLIENT_ARRANGEDTEAM_FRIENDSCREEN packet",conn_get_socket(c));
   
   myusername = conn_get_username(c);
   eventlog(eventlog_level_trace, "handle_bnet", "[%d] AT - Got Username %s", 
	    conn_get_socket(c),
	    myusername);
   
   if (!(rpacket = packet_create(packet_class_bnet))) {
      eventlog(eventlog_level_error, "handle_bnet", 
	       "[%d] AT - can't create friendscreen server packet",
	       conn_get_socket(c));
      return -1;
   }
   
   packet_set_size(rpacket,sizeof(t_server_arrangedteam_friendscreen));
			packet_set_type(rpacket,SERVER_ARRANGEDTEAM_FRIENDSCREEN);
   
   // cycle through friends, taking those who is currently available
   for(i = 0; i < account_get_friendcount(conn_get_account(c)); i++) 
     {
	f = account_get_friend(conn_get_account(c), i);
	
	if(!(dest_c = connlist_find_connection_by_accountname(f))) 
	  continue; // if user is offline, then continue to next friend
	
	if(account_check_mutual(conn_get_account(dest_c), myusername) == 0)
	  {		
	     if(conn_get_dndstr(dest_c)) 
	       continue; // user is dnd
	     if(conn_get_game(dest_c)) 
	       continue; // user is some game
	     if (!conn_get_channel(dest_c))
	       continue;
	     
	     eventlog(eventlog_level_trace,"handle_bnet","AT - Friend: %s is available for a AT Game.", f);
	     f_cnt++;
	     packet_append_string(rpacket, f);
	  }
     }
   // <- end of cycle
		
   if(!f_cnt)
     {
	eventlog(eventlog_level_info, "handle_bnet", "AT - no friends available for AT game.");
     } else {
	bn_byte_set(&rpacket->u.server_arrangedteam_friendscreen.f_count, f_cnt);
	queue_push_packet(conn_get_out_queue(c), rpacket);
     }
   
   packet_del_ref(rpacket);
   
   return 0;
}

static int _client_atinvitefriend(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_arrangedteam_invite_friend)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ARRANGEDTEAM_INVITE_FRIEND packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_arrangedteam_invite_friend),packet_get_size(packet));
      return -1;
   }
   
     {
	int count_to_invite = bn_byte_get(packet->u.client_arrangedteam_invite_friend.numfriends);
	char const *invited_usernames[8];
	char const *invited_usernames2[8];
	char atmembers_usernames[255];
	int i, n, offset,teamnumber,teammemcount;
	t_connection * dest_c;
	
	teammemcount = count_to_invite;
	teammemcount++;
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got ARRANGEDTEAM INVITE packet",conn_get_socket(c));
	offset = sizeof(t_client_arrangedteam_invite_friend);
	
	for (i = 0; i < count_to_invite; i++) 
	  {
	     if (!(invited_usernames[i] = packet_get_str_const(packet, offset, USER_NAME_MAX))) 
	       {
		  eventlog(eventlog_level_error, "handle_bnet", "Could not get username from invite packet");
	       } 
	     else 
	       {
		  offset += strlen(invited_usernames[i]) + 1;
		  eventlog(eventlog_level_debug,"handle_bnet","Added user %s to invite array.", invited_usernames[i]);
	       }
	  }
	//add names to another array
	n=teammemcount-1;
	for (i=0;i<teammemcount;i++)
	  {
	     if(i==n)
	       invited_usernames2[i] = conn_get_username(c);
	     else
	       invited_usernames2[i] = invited_usernames[i];
	  }
	//--> start of stat shit
	qsort((char *)invited_usernames2, teammemcount, sizeof(char *), compar); //sort the array				
	
	for (i=0;i<teammemcount;i++)
	  {
	     if(i==0)
	       strcpy(atmembers_usernames,invited_usernames2[i]);
	     else
	       {
		  strcat(atmembers_usernames," ");
		  strcat(atmembers_usernames,invited_usernames2[i]);
	       }
	  }		
	
	eventlog(eventlog_level_debug,"handle_bnet","All AT Team Members: %s",atmembers_usernames);
	
	//check see if inviter has ever played AT before
	if(account_check_team(conn_get_account(c),atmembers_usernames)<0)
	  {
	     if(account_set_currentatteam(conn_get_account(c),account_create_newteam(conn_get_account(c),atmembers_usernames,count_to_invite))<0)
	       eventlog(eventlog_level_debug,"handle_bnet","Unable to properly create team info for user. No Stats will be saved");
	     if(account_set_new_at_team(conn_get_account(c),1)<0)
	       eventlog(eventlog_level_error,"handle_bnet","Unable to set flag new_at_team to 1 for YES");
	     
	  }
	else
	  {
	     teamnumber = account_check_team(conn_get_account(c),atmembers_usernames);
	     eventlog(eventlog_level_debug,"handle_bnet","AT Team has played before! - Team number %d",teamnumber);
	     account_set_currentatteam(conn_get_account(c),teamnumber);
	     account_set_atteamsize(conn_get_account(c),teamnumber,count_to_invite);
	     account_set_new_at_team(conn_get_account(c),0); //avoid warning
	  }
	//<--- end of stat saving shit
	
	//Create the packet to send to each of the users you wanted to invite
	// [quetzal] 20020822 - set inviter's atid
	conn_set_atid(c, bn_int_get(packet->u.client_arrangedteam_invite_friend.id));
	conn_set_channel(c,NULL);
	//if new team set flag
	
	for(i=0; i < count_to_invite; i++)
	  {
	     if(!(dest_c = connlist_find_connection_by_accountname(invited_usernames[i]))) 
	       continue;
	     
	     // [quetzal] 20020822 - set player's atid
	     conn_set_atid(dest_c, bn_int_get(packet->u.client_arrangedteam_invite_friend.id));
	     
	     if (!(rpacket = packet_create(packet_class_bnet)))
	       return -1;
	     packet_set_size(rpacket,sizeof(t_server_arrangedteam_send_invite));
	     packet_set_type(rpacket, SERVER_ARRANGEDTEAM_SEND_INVITE);
	     
	     bn_int_set(&rpacket->u.server_arrangedteam_send_invite.count,bn_int_get(packet->u.client_arrangedteam_invite_friend.count));
	     bn_int_set(&rpacket->u.server_arrangedteam_send_invite.id,bn_int_get(packet->u.client_arrangedteam_invite_friend.id));
	     bn_int_set(&rpacket->u.server_arrangedteam_send_invite.inviterip, htonl(conn_get_addr(c)));
	     bn_short_set(&rpacket->u.server_arrangedteam_send_invite.port, conn_get_game_port(c));
	     bn_byte_set(&rpacket->u.server_arrangedteam_send_invite.numfriends,count_to_invite);
	     packet_append_string(rpacket,conn_get_username(c));
	     
	     conn_set_atid(c, bn_int_get(packet->u.client_arrangedteam_invite_friend.id));
	     conn_set_channel(c,NULL);
	     
	     //attach all the invited to the packet - but dont include the invitee's name
	     for(n=0; n < count_to_invite; n++) 
	       {
		  if (n != i) packet_append_string(rpacket, invited_usernames[n]);
	       }
	     
	     //now send packet
	     queue_push_packet(conn_get_out_queue(dest_c),rpacket);
	     packet_del_ref(rpacket);
	     
	     //start of stat shit for each invitee -->
	     if(account_check_team(conn_get_account(dest_c),atmembers_usernames)<0)
	       {
		  if(account_set_currentatteam(conn_get_account(dest_c),account_create_newteam(conn_get_account(dest_c),atmembers_usernames,count_to_invite))<0)
		    eventlog(eventlog_level_debug,"handle_bnet","Unable to properly create team info for user. No Stats will be saved");
		  if(account_set_new_at_team(conn_get_account(dest_c),1)<0)
		    eventlog(eventlog_level_error,"handle_bnet","Unable to set flag new_at_team to 1 for YES");
	       }
	     else
	       {
		  teamnumber = account_check_team(conn_get_account(dest_c),atmembers_usernames);
		  eventlog(eventlog_level_debug,"handle_bnet","AT Team has played before! - Team number %d",teamnumber);
		  account_set_currentatteam(conn_get_account(dest_c),teamnumber);
		  account_set_atteamsize(conn_get_account(dest_c),teamnumber,count_to_invite);
		  account_set_new_at_team(conn_get_account(dest_c),0); //avoid warning
	       }
	     //<--- end of stat saving shit
	     
	  }	
	
	//now send a ACK to the inviter 
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_arrangedteam_invite_friend_ack));
	packet_set_type(rpacket,SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK);
	
	bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.count,bn_int_get(packet->u.client_arrangedteam_invite_friend.count));
	bn_short_set(&rpacket->u.server_arrangedteam_invite_friend_ack.unknown1,bn_short_get(packet->u.client_arrangedteam_invite_friend.unknown1));
	bn_short_set(&rpacket->u.server_arrangedteam_invite_friend_ack.unknown2,0x0001);
	bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.timestamp,get_ticks());
	bn_byte_set(&rpacket->u.server_arrangedteam_invite_friend_ack.teamsize,count_to_invite++);
	
	for(i = 0; i < 5; i++) bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.unknown3[i], 0);
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
	
     }

   return 0;
}

static int _client_atacceptdeclineinvite(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_arrangedteam_accept_decline_invite)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ARRANGEDTEAM_ACCEPT_DECLINE_INVITE packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_arrangedteam_accept_decline_invite),packet_get_size(packet));
      return -1;
   }
   
     {
	char const *inviter;
	t_connection * dest_c;
	
	if(account_get_new_at_team(conn_get_account(c))==1)
	  {
	     int temp;
	     temp = account_get_atteamcount(conn_get_account(c));
	     temp = temp-1;
	     account_set_atteamcount(conn_get_account(c),temp);
	     account_set_new_at_team(conn_get_account(c),0);
	  }
	
	//if user declined the invitation then
	if(bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.option)==CLIENT_ARRANGEDTEAM_DECLINE)
	  {
	     inviter = packet_get_str_const(packet,sizeof(t_client_arrangedteam_accept_decline_invite),USER_NAME_MAX);
	     dest_c = connlist_find_connection_by_accountname(inviter);
	     
	     eventlog(eventlog_level_info,"handle_bnet","%s declined a arranged team game with %s",conn_get_username(c),inviter);
	     
	     if (!(rpacket = packet_create(packet_class_bnet)))
	       return -1;
	     packet_set_size(rpacket,sizeof(t_server_arrangedteam_member_decline));
	     packet_set_type(rpacket,SERVER_ARRANGEDTEAM_MEMBER_DECLINE);
	     
	     bn_int_set(&rpacket->u.server_arrangedteam_member_decline.count,bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.count));
	     bn_int_set(&rpacket->u.server_arrangedteam_member_decline.action,SERVER_ARRANGEDTEAM_DECLINE);
	     packet_append_string(rpacket,conn_get_username(c));
	     
	     queue_push_packet(conn_get_out_queue(dest_c),rpacket);
	     packet_del_ref(rpacket);
	     
	  }
     }

   return 0;
}

static int _client_findanongame(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (bn_byte_get(packet->u.client_findanongame.option)==CLIENT_FINDANONGAME_PROFILE)
     {
	char const * username;
	int Count;
	int temp;
	t_account * account; 
	
	Count = bn_int_get(packet->u.client_findanongame.count);
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got a FINDANONGAME PROFILE packet",conn_get_socket(c));
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_findanongame_profile),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad FINDANONGAME_PROFILE (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	
	//If no account is found then break
	if (!(account = accountlist_find_account(username)))
	  {				
	     eventlog(eventlog_level_error,"handle_bnet","Could not get account - PROFILE");
	     return -1;
	  }										
	
	eventlog(eventlog_level_error,"handle_bnet_packet","Looking up %s's WAR3 Stats.",username);
			
	if (account_get_sololevel(account)==0 && account_get_teamlevel(account)==0 && account_get_atteamcount(account)==0)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","%s does not have WAR3 Stats.",username);
	     if (!(rpacket = packet_create(packet_class_bnet)))
	       return -1;
	     packet_set_size(rpacket,sizeof(t_server_findanongame_profile2));
	     packet_set_type(rpacket,SERVER_FINDANONGAME_PROFILE);
	     bn_byte_set(&rpacket->u.server_findanongame_profile2.option,SERVER_FINDANONGAME_PROFILE_OPTION);
	     bn_int_set(&rpacket->u.server_findanongame_profile2.count,Count);
	     bn_int_set(&rpacket->u.server_findanongame_profile2.unknown1,SERVER_FINDANONGAME_PROFILE_UNKNOWN2);
	     bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount,0);
	     temp=0;
	     packet_append_data(rpacket,&temp,2); 
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);				
	  }
	else // If they do have a profile then:
	  {
	     int solowins=account_get_solowin(account); 
	     int sololoss=account_get_sololoss(account);
	     int soloxp=account_get_soloxp(account);
	     int sololevel=account_get_sololevel(account);
	     int solorank=account_get_solorank(account);
	     
	     int teamwins=account_get_teamwin(account);
	     int teamloss=account_get_teamloss(account);
	     int teamxp=account_get_teamxp(account);
	     int teamlevel=account_get_teamlevel(account);
	     int teamrank=account_get_teamrank(account);
	     
	     int ffawins=account_get_ffawin(account);
	     int ffaloss=account_get_ffaloss(account);
	     int ffaxp=account_get_ffaxp(account);
	     int ffalevel=account_get_ffalevel(account);
	     int ffarank=account_get_ffarank(account);
	     
	     int humanwins=account_get_racewin(account,1);
	     int humanlosses=account_get_raceloss(account,1);
	     int orcwins=account_get_racewin(account,2);
	     int orclosses=account_get_raceloss(account,2);
	     int undeadwins=account_get_racewin(account,8);
	     int undeadlosses=account_get_raceloss(account,8);
	     int nightelfwins=account_get_racewin(account,4);
	     int nightelflosses=account_get_raceloss(account,4);
	     int randomwins=account_get_racewin(account,0);
	     int randomlosses=account_get_raceloss(account,0);
	     
	     unsigned char rescount;
	     
	     if (!(rpacket = packet_create(packet_class_bnet)))
	       return -1;
	     packet_set_size(rpacket,sizeof(t_server_findanongame_profile2));
	     packet_set_type(rpacket,SERVER_FINDANONGAME_PROFILE);
	     bn_byte_set(&rpacket->u.server_findanongame_profile2.option,SERVER_FINDANONGAME_PROFILE_OPTION);
	     bn_int_set(&rpacket->u.server_findanongame_profile2.count,Count); //job count
	     bn_int_set(&rpacket->u.server_findanongame_profile2.unknown1,account_get_icon_profile(account));
	     rescount = 0;
	     if (sololevel) {
		temp=0x534F4C4F; // SOLO backwards
		packet_append_data(rpacket,&temp,4);
		packet_append_data(rpacket,&solowins,2); //SOLO WINS
		packet_append_data(rpacket,&sololoss,2); // SOLO LOSSES
		packet_append_data(rpacket,&sololevel,1); // SOLO LEVEL
		temp=account_get_profile_calcs(account,soloxp,sololevel);
		packet_append_data(rpacket,&temp,1); // SOLO PROFILE CALC
		packet_append_data(rpacket,&soloxp,2); // SOLO XP
		packet_append_data(rpacket,&solorank,4); // SOLO LADDER RANK
		rescount++;
	     }
	     
	     if (teamlevel) {
		//below is for team records. Add this after 2v2,3v3,4v4 are done
		temp=0x5445414D; 
		packet_append_data(rpacket,&temp,4);
		packet_append_data(rpacket,&teamwins,2);
		packet_append_data(rpacket,&teamloss,2);
		packet_append_data(rpacket,&teamlevel,1);
		temp=account_get_profile_calcs(account,teamxp,teamlevel);
		packet_append_data(rpacket,&temp,1);
		packet_append_data(rpacket,&teamxp,2);
		packet_append_data(rpacket,&teamrank,4);
		//done of team game stats
		rescount++;
	     }
	     //FFA stats
	     if (ffalevel) {
		temp=0x46464120;
		packet_append_data(rpacket,&temp,4);
		packet_append_data(rpacket,&ffawins,2);
		packet_append_data(rpacket,&ffaloss,2);
		packet_append_data(rpacket,&ffalevel,1);
		temp=account_get_profile_calcs(account,ffaxp,ffalevel);
		packet_append_data(rpacket,&temp,1);
		packet_append_data(rpacket,&ffaxp,2);
		packet_append_data(rpacket,&ffarank,4);
		//End of FFA Stats
		rescount++;
	     }
	     /* set result count */
	     bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount,rescount);
	     
	     temp=0x06; //start of race stats
	     packet_append_data(rpacket,&temp,1);
	     packet_append_data(rpacket,&randomwins,2); //random wins
	     packet_append_data(rpacket,&randomlosses,2); //random losses
	     packet_append_data(rpacket,&humanwins,2); //human wins
	     packet_append_data(rpacket,&humanlosses,2); //human losses
	     packet_append_data(rpacket,&orcwins,2); //orc wins
	     packet_append_data(rpacket,&orclosses,2); //orc losses
	     packet_append_data(rpacket,&undeadwins,2); //undead wins
	     packet_append_data(rpacket,&undeadlosses,2); //undead losses
	     packet_append_data(rpacket,&nightelfwins,2); //elf wins
	     packet_append_data(rpacket,&nightelflosses,2); //elf losses
	     temp=0;
	     packet_append_data(rpacket,&temp,4);
	     //end of normal stats - Start of AT stats
	     temp=account_get_atteamcount(account);
	     if (temp > 6) temp = 6; //byte (num of AT teams) 
	     
	     packet_append_data(rpacket,&temp,1); 
	     
	     if(account_get_atteamcount(account)>0)
	       {
		  // [quetzal] 20020827 - partially rewritten AT part
		  int i, j, lvl, highest_lvl[6], cnt = account_get_atteamcount(account);
		  // allocate array for teamlevels
		  int *teamlevels = malloc(cnt * sizeof(int));
		  
		  // populate our array
		  for (i = 0; i < cnt; i++) {
		     teamlevels[i] = account_get_atteamlevel(account, i+1);
		  }
		  
		  // now lets pick indices of 6 highest levels
		  for (j = 0; j < cnt && j < 6; j++) {
		     lvl = -1;
		     for (i = 0; i < cnt; i++) {
			if (teamlevels[i] > lvl) {
			   lvl = teamlevels[i];
			   highest_lvl[j] = i;
			}
		     }
		     teamlevels[highest_lvl[j]] = -1;
		     eventlog(eventlog_level_debug, "handle_bnet_packet", 
			      "profile/AT - highest level is %d with index %d", lvl, highest_lvl[j]);
		  }
		  // <-- end of picking indices
		  // 
		  free(teamlevels);
		  
		  for(i = 0; i < j; i++)
		    {
		       int n = highest_lvl[i] + 1;
		       int teamsize = account_get_atteamsize(account, n);
		       char * teammembers = NULL, *self = NULL, *p2, *p3;
		       int teamtype[] = {0, 0x32565332, 0x33565333, 0x34565334, 0x35565335, 0x36565336};
		       
		       // [quetzal] 20020907 - sometimes we get NULL string, even if
		       // teamcount is not 0. that should prevent crash.
		       if (!account_get_atteammembers(account, n) || teamsize < 1 || teamsize > 5) continue;
		       
		       p2 = p3 = teammembers = strdup(account_get_atteammembers(account, n));
		       
		       eventlog(eventlog_level_debug, "handle_bnet_packet", 
				"profile/AT - processing team %d", n);
		       
		       temp = teamtype[teamsize];
		       packet_append_data(rpacket,&temp,4);
		       temp=account_get_atteamwin(account,n); //at team wins
		       packet_append_data(rpacket,&temp,2);
		       temp=account_get_atteamloss(account,n); //at team losses
		       packet_append_data(rpacket,&temp,2);
		       temp=account_get_atteamlevel(account,n); 
		       packet_append_data(rpacket,&temp,1);
		       temp=account_get_profile_calcs(account,account_get_atteamxp(account,n),account_get_atteamlevel(account,n)); // xp bar calc
		       packet_append_data(rpacket,&temp,1);
		       temp=account_get_atteamxp(account,n);
		       packet_append_data(rpacket,&temp,2);
		       temp=0; //rank on AT ladder
		       packet_append_data(rpacket,&temp,4);
		       temp=0;
		       packet_append_data(rpacket,&temp,4); //some unknown packet? random shit
		       temp=0;
		       packet_append_data(rpacket,&temp,4); //another unknown packet..random shit
		       packet_append_data(rpacket,&teamsize,1);
		       //now attach the names to the packet - not including yourself
		       
		       
		       // [quetzal] 20020826
		       self = strstr(teammembers, account_get_name(account));
		       
		       while (p2)
			 {
			    p3 = strchr(p2, ' ');
			    if (p3) *p3++ = 0;
			    if (self != p2) packet_append_string(rpacket, p2);
			    p2 = p3;
			 }
		       
		       free(teammembers);
		    }
		  
	       }
	     
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);				
	     
	     eventlog(eventlog_level_info,"handle_bnet_packet","Sent %s's WAR3 Stats to requestor.",username);
	  }
     } 
   else if (bn_byte_get(packet->u.client_findanongame.option) == CLIENT_FINDANONGAME_CANCEL) 
     {
	// [quetzal] 20020809 - added a_count, so we dont refer to already destroyed anongame
	t_anongame *a = conn_get_anongame(c);
	int a_count;
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got FINDANONGAME CANCEL packet", conn_get_socket(c));
	
	if(!a)
	  return -1;
	
	a_count = anongame_get_count(a);
	
	// anongame_unqueue_player(c, anongame_get_type(a)); 
	// -- already doing unqueue in anongame_destroy_anongame
	conn_set_routeconn(c, NULL);
	conn_destroy_anongame(c);
	
	if (!(rpacket = packet_create(packet_class_bnet))) return -1;
	packet_set_size(rpacket,sizeof(t_server_findanongame_playgame_cancel));
	packet_set_type(rpacket,SERVER_FINDANONGAME_PLAYGAME_CANCEL);
	bn_byte_set(&rpacket->u.server_findanongame_playgame_cancel.cancel,SERVER_FINDANONGAME_PLAYGAME_CANCEL_CANCEL);
	bn_int_set(&rpacket->u.server_findanongame_playgame_cancel.count, a_count);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
	
     }
   else if (bn_byte_get(packet->u.client_findanongame.option)==CLIENT_FINDANONGAME_SEARCH)
     {
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got FINDANONGAME SEARCH packet",conn_get_socket(c));
	handle_anongame_search(c, packet);
     }
   else if (bn_byte_get(packet->u.client_findanongame.option)==CLIENT_FINDANONGAME_AT_INVITER_SEARCH)
     {
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got FINDANONGAME Arranged Team SEARCH packet by inviter",conn_get_socket(c));
	handle_anongame_search(c, packet);
     }
   else if (bn_byte_get(packet->u.client_findanongame.option)==CLIENT_FINDANONGAME_AT_SEARCH)
     {
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] got FINDANONGAME Arranged Team SEARCH packet",conn_get_socket(c));
	handle_anongame_search(c, packet);
     }
   
   return 0;
}

static int _client_motdw3(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_motd_w3))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_MOTD_W3 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_motd_w3),packet_get_size(packet));
	return -1;
     }
   
     {
	time_t news_time;
	
	// default news time is right now
	news_time=time(NULL);
	
	// News
	  {
	     char const * filename;
	     FILE *       fp;
	     char * buff;
	     char * big_buffer;
	     int file_size;
	     struct stat st;
	     int last_news_time;
	     
	     if ((filename = prefs_get_newsfile())) {
		if (stat(filename,&st)==0) {
		   news_time=st.st_mtime;
		   file_size=st.st_size;
		   last_news_time=bn_int_get(packet->u.client_motd_w3.last_news_time);
		   
		   //eventlog(eventlog_level_trace,"news_time %d > last_news_time %d",news_time,last_news_time);
		   if (news_time>last_news_time) {
		      if ((fp = fopen(filename,"r"))!=NULL)
			{
			   
			   if (!(rpacket = packet_create(packet_class_bnet)))
			     return -1;
			   packet_set_size(rpacket,sizeof(t_server_motd_w3));
			   packet_set_type(rpacket,SERVER_MOTD_W3);
			   
			   bn_int_set(&rpacket->u.server_motd_w3.unknown4,SERVER_MOTD_W3_UNKNOWN4);
			   bn_byte_set(&rpacket->u.server_motd_w3.msgtype,SERVER_MOTD_W3_MSGTYPE);
			   bn_int_set(&rpacket->u.server_motd_w3.first_news_time,news_time);
			   bn_int_set(&rpacket->u.server_motd_w3.last_news_time,news_time);
			   bn_int_set(&rpacket->u.server_motd_w3.timestamp,news_time);
			   
			   // Find file size
			   /*fseek(fp,0,SEEK_END);
			    file_size=ftell(fp);
			    fseek(fp,0,SEEK_SET);*/
			   
			   // Allocate a buffer large enough
			   // "large enough" means 2x because every byte _could_ be a \n
			   file_size*=2;
			   big_buffer=malloc(file_size+1);
			   if (big_buffer==NULL) {
			      eventlog(eventlog_level_error,"handle_bnet_packet","(W3) could not allocate %d bytes for news storage",file_size+1);
			   } else {
			      eventlog(eventlog_level_trace,"handle_bnet_packet","(W3) allocated %d bytes for news storage",file_size+1);
			      big_buffer[0]=0; // clear first byte
			      
			      // Load news into buffer
			      while ((buff = (char *)file_get_line(fp))) /* FIXED BY UNDYING SOULZZ 4/9/02 */
				{
				   strncat(big_buffer,buff,file_size-strlen(big_buffer));
				   strncat(big_buffer,"\r\n",file_size-strlen(big_buffer));
				   free(buff);
				}
			      
			      // Append news to packet
			      packet_append_string(rpacket,big_buffer);
			      eventlog(eventlog_level_trace,"handle_bnet_packet","(W3) %d bytes were used to store news",strlen(big_buffer));
			      free(big_buffer);
			      
			      // Send news packet
			      queue_push_packet(conn_get_out_queue(c),rpacket);
			   }
			   
			   packet_del_ref(rpacket);

			   if (fclose(fp)<0)
			     eventlog(eventlog_level_error,"handle_bnet_packet","(W3) could not close news file \"%s\" after reading",filename);
			} /* fopen */
		      else
			{
			   eventlog(eventlog_level_error,"handle_bnet_packet","(W3) could not open news file \"%s\" for reading",filename);
			}
		   } /* is new news */
		} else {
		   eventlog(eventlog_level_error,"handle_bnet_packet","(W3) stat(%s) failed",filename);
		}
	     }
	  }
	
	// Welcome Message
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_motd_w3));
	packet_set_type(rpacket,SERVER_MOTD_W3);
	
        //bn_int_set(&rpacket->u.server_motd_w3.ticks,get_ticks());
	bn_int_set(&rpacket->u.server_motd_w3.unknown4,SERVER_MOTD_W3_UNKNOWN4);
	bn_byte_set(&rpacket->u.server_motd_w3.msgtype,SERVER_MOTD_W3_MSGTYPE);
	bn_int_set(&rpacket->u.server_motd_w3.first_news_time,news_time);
	bn_int_set(&rpacket->u.server_motd_w3.last_news_time,news_time);
	bn_int_set(&rpacket->u.server_motd_w3.timestamp,SERVER_MOTD_W3_WELCOME);
	
	/* MODIFIED BY THE UNDYING SOULZZ 4/7/02 */
	  {
	     char serverinfo[512];
	     sprintf(serverinfo, "This server is powered by PvPGN Version: %s\r\nThere are currently %d users online, playing %d games and chatting in %d channels\r\n\r\n%s\r\n",
		     PVPGN_VERSION,
		     connlist_login_get_length(),
		     gamelist_get_length(),
		     channellist_get_length(),
		     prefs_get_server_info());
	     
	     packet_append_string(rpacket,serverinfo);
	  }
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
	
	// Set welcomed flag so we don't send MOTD with the old format
	conn_set_welcomed(c,1);
	
     }
   
   return 0;
}

static int _client_realmlistreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_realmlistreq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMLISTREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_realmlistreq),packet_get_size(packet));
      return -1;
   }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	t_elem const *               curr;
	t_realm const *              realm;
	t_server_realmlistreply_data realmdata;
	unsigned int                 count;
	
	packet_set_size(rpacket,sizeof(t_server_realmlistreply));
	packet_set_type(rpacket,SERVER_REALMLISTREPLY);
	bn_int_set(&rpacket->u.server_realmlistreply.unknown1,SERVER_REALMLISTREPLY_UNKNOWN1);
	count = 0;
	LIST_TRAVERSE_CONST(realmlist(),curr)
	  {
	     realm = elem_get_data(curr);
	     if (!realm_get_active(realm))
	       continue;
	     bn_int_set(&realmdata.unknown3,SERVER_REALMLISTREPLY_DATA_UNKNOWN3);
	     bn_int_set(&realmdata.unknown4,SERVER_REALMLISTREPLY_DATA_UNKNOWN4);
	     bn_int_set(&realmdata.unknown5,SERVER_REALMLISTREPLY_DATA_UNKNOWN5);
	     bn_int_set(&realmdata.unknown6,SERVER_REALMLISTREPLY_DATA_UNKNOWN6);
	     bn_int_set(&realmdata.unknown7,SERVER_REALMLISTREPLY_DATA_UNKNOWN7);
	     bn_int_set(&realmdata.unknown8,SERVER_REALMLISTREPLY_DATA_UNKNOWN8);
	     bn_int_set(&realmdata.unknown9,SERVER_REALMLISTREPLY_DATA_UNKNOWN9);
	     packet_append_data(rpacket,&realmdata,sizeof(realmdata));
	     packet_append_string(rpacket,realm_get_name(realm));
	     packet_append_string(rpacket,realm_get_description(realm));
	     count++;
	  }
	bn_int_set(&rpacket->u.server_realmlistreply.count,count);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_realmjoinreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_realmjoinreq))
     {
	eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMJOINREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_realmjoinreq),packet_get_size(packet));
	return -1;
     }
   
     {
	char const *    realmname;
	t_realm const * realm;
	
	if (!(realmname = packet_get_str_const(packet,sizeof(t_client_realmjoinreq),REALM_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMJOINREQ (missing or too long realmname)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(realm = realmlist_find_realm(realmname)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMJOINREQ (missing or too long realmname)",conn_get_socket(c));
	     if ((rpacket = packet_create(packet_class_bnet)))
	       {
		  packet_set_size(rpacket,sizeof(t_server_realmjoinreply));
		  packet_set_type(rpacket,SERVER_REALMJOINREPLY);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown1,bn_int_get(packet->u.client_realmjoinreq.unknown1)); /* should this be copied on error? */
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown2,0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown3,0); /* reg auth */
		  bn_int_set(&rpacket->u.server_realmjoinreply.sessionkey,0);
		  bn_int_nset(&rpacket->u.server_realmjoinreply.addr,0);
		  bn_short_nset(&rpacket->u.server_realmjoinreply.port,0);
		  bn_short_set(&rpacket->u.server_realmjoinreply.unknown6,0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown7,0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown8,0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown9,0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.secret_hash[0],0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.secret_hash[1],0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.secret_hash[2],0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.secret_hash[3],0);
		  bn_int_set(&rpacket->u.server_realmjoinreply.secret_hash[4],0);
		  packet_append_string(rpacket,"");
		  queue_push_packet(conn_get_out_queue(c),rpacket);
		  packet_del_ref(rpacket);
	       }
	  }
	else
	  {
	     unsigned int salt;
	     struct
	       {
		  bn_int   salt;
		  bn_int   sessionkey;
		  bn_int   sessionnum;
		  bn_int   secret;
	       } temp;
	     t_hash       secret_hash;
	     
	     conn_set_realmname(c,realm_get_name(realm)); /* FIXME: should we only set this after they log in to the realm server? */
	     
	     /* "password" to verify auth connection later, not sure what real Battle.net does */
	     salt = rand();
	     bn_int_set(&temp.salt,salt);
	     bn_int_set(&temp.sessionkey,conn_get_sessionkey(c));
	     bn_int_set(&temp.sessionnum,conn_get_sessionnum(c));
	     bn_int_set(&temp.secret,conn_get_secret(c));
	     bnet_hash(&secret_hash,sizeof(temp),&temp);
	     
	     if ((rpacket = packet_create(packet_class_bnet)))
	       {
		  char const * tname;
		  
		  packet_set_size(rpacket,sizeof(t_server_realmjoinreply));
		  packet_set_type(rpacket,SERVER_REALMJOINREPLY);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown1,bn_int_get(packet->u.client_realmjoinreq.unknown1));
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown2,SERVER_REALMJOINREPLY_UNKNOWN2);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown3,SERVER_REALMJOINREPLY_UNKNOWN3); /* reg auth */
		  bn_int_set(&rpacket->u.server_realmjoinreply.sessionkey,conn_get_sessionkey(c));
		  bn_int_nset(&rpacket->u.server_realmjoinreply.addr,realm_get_showip(realm));
		  bn_short_nset(&rpacket->u.server_realmjoinreply.port,realm_get_showport(realm));
		  bn_short_set(&rpacket->u.server_realmjoinreply.unknown6,SERVER_REALMJOINREPLY_UNKNOWN6);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown7,SERVER_REALMJOINREPLY_UNKNOWN7);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown8,SERVER_REALMJOINREPLY_UNKNOWN8);
		  bn_int_set(&rpacket->u.server_realmjoinreply.unknown9,salt);
		  hash_to_bnhash((t_hash const *)&secret_hash,rpacket->u.server_realmjoinreply.secret_hash); /* avoid warning */
		  packet_append_string(rpacket,(tname = conn_get_username(c)));
		  conn_unget_username(c,tname);
		  queue_push_packet(conn_get_out_queue(c),rpacket);
		  packet_del_ref(rpacket);
	       }
	  }
     }
   
   return 0;
}

static int _client_realmjoinreq109(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_realmjoinreq_109)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMJOINREQ_109 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_realmjoinreq_109),packet_get_size(packet));
      return -1;
   }
	    
     {
	char const *    realmname;
	t_realm * realm;
	
	if (!(realmname = packet_get_str_const(packet,sizeof(t_client_realmjoinreq_109),REALM_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad REALMJOINREQ_109 (missing or too long realmname)",conn_get_socket(c));
	     return -1;
	  }
	
	if ((realm = realmlist_find_realm(realmname)))
	  {
	     unsigned int salt;
	     struct
	       {
		  bn_int   salt;
		  bn_int   sessionkey;
		  bn_int   sessionnum;
		  bn_int   secret;
		  bn_int   passhash[5];
	       } temp;
	     char const * pass_str;
	     t_hash       secret_hash;
	     t_hash       passhash;
	     char const * prev_realm;
	     
	     /* FIXME: should we only set this after they log in to the realm server? */
	     prev_realm = conn_get_realmname(c);
	     if (prev_realm)
	       {
		  if (strcasecmp(prev_realm,realm_get_name(realm)))
		    {
		       realm_add_player_number(realm,1);
		       realm_add_player_number(realmlist_find_realm(prev_realm),-1);
		       conn_set_realmname(c,realm_get_name(realm));
		    }
	       }
	     else
	       {
		  realm_add_player_number(realm,1);
		  conn_set_realmname(c,realm_get_name(realm));
	       }
	     
	     if ((pass_str = account_get_pass(conn_get_account(c))))
	       {
		  if (hash_set_str(&passhash,pass_str)==0)
		    {
		       account_unget_pass(pass_str);
		       hash_to_bnhash((t_hash const *)&passhash,temp.passhash);
		       salt = bn_int_get(packet->u.client_realmjoinreq_109.seqno);
		       bn_int_set(&temp.salt,salt);
		       bn_int_set(&temp.sessionkey,conn_get_sessionkey(c));
		       bn_int_set(&temp.sessionnum,conn_get_sessionnum(c));
		       bn_int_set(&temp.secret,conn_get_secret(c));
		       bnet_hash(&secret_hash,sizeof(temp),&temp);
		       
		       if ((rpacket = packet_create(packet_class_bnet)))
			 {
			    char const * tname;
			    
			    packet_set_size(rpacket,sizeof(t_server_realmjoinreply_109));
			    packet_set_type(rpacket,SERVER_REALMJOINREPLY_109);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno,salt);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.u1,0x0);
			    bn_short_set(&rpacket->u.server_realmjoinreply_109.u3,0x0); /* reg auth */
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1,0x0);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum,conn_get_sessionnum(c));
			    bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr,realm_get_ip(realm));
			    bn_short_nset(&rpacket->u.server_realmjoinreply_109.port,realm_get_port(realm));
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey,conn_get_sessionkey(c));
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.u5,0);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.u6,0);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2,0);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.u7,0);
			    bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid,conn_get_versionid(c));
			    bn_int_tag_set(&rpacket->u.server_realmjoinreply_109.clienttag,conn_get_clienttag(c));
			    hash_to_bnhash((t_hash const *)&secret_hash,rpacket->u.server_realmjoinreply_109.secret_hash); /* avoid warning */
			    packet_append_string(rpacket,(tname = conn_get_username(c)));
			    conn_unget_username(c,tname);
			    queue_push_packet(conn_get_out_queue(c),rpacket);
			    packet_del_ref(rpacket);
			 }
		       return 0;
		    }
		  else
		    {
		       char const * tname;
		       
		       eventlog(eventlog_level_info,"handle_bnet_packet","[%d] realm join for \"%s\" failed (unable to hash password)",conn_get_socket(c),(tname = account_get_name(conn_get_account(c))));
		       account_unget_name(tname);
		       account_unget_pass(pass_str);
		    }
	       }
	     else
	       {
		  char const * tname;
		  
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] realm join for \"%s\" failed (no password)",conn_get_socket(c),(tname = account_get_name(conn_get_account(c))));
		  account_unget_name(tname);
	       }
	  }
	else
	  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] could not find active realm \"%s\"",conn_get_socket(c),realmname);
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_realmjoinreply_109));
	     packet_set_type(rpacket,SERVER_REALMJOINREPLY_109);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno,bn_int_get(packet->u.client_realmjoinreq_109.seqno));
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.u1,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum,0);
	     bn_short_set(&rpacket->u.server_realmjoinreply_109.u3,0);
	     bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr,0);
	     bn_short_nset(&rpacket->u.server_realmjoinreply_109.port,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.u5,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.u6,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.u7,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.clienttag,0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[0],0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[1],0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[2],0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[3],0);
	     bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[4],0);
	     packet_append_string(rpacket,"");
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_charlistreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_unknown_37)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad UNKNOWN_37 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_unknown_37),packet_get_size(packet));
      return -1;
   }
   /*
    0x0070:                           83 80 ff ff ff ff ff 2f    t,taran,......./
    0x0080: ff ff ff ff ff ff ff ff   ff ff 03 ff ff ff ff ff    ................
    0x0090: ff ff ff ff ff ff ff ff   ff ff ff 07 80 80 80 80    ................
    0x00a0: ff ff ff 00
    */
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	char const * charlist;
	char *       temp;
	
	packet_set_size(rpacket,sizeof(t_server_unknown_37));
	packet_set_type(rpacket,SERVER_UNKNOWN_37);
	bn_int_set(&rpacket->u.server_unknown_37.unknown1,SERVER_UNKNOWN_37_UNKNOWN1);
	bn_int_set(&rpacket->u.server_unknown_37.unknown2,SERVER_UNKNOWN_37_UNKNOWN2);
	
	if (!(charlist = account_get_closed_characterlist(conn_get_account(c),conn_get_clienttag(c),conn_get_realmname(c))))
	  {
	     bn_int_set(&rpacket->u.server_unknown_37.count,0);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	     return 0;
	  }
	if (!(temp = strdup(charlist)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unable to allocate memory for characterlist",conn_get_socket(c));
	     bn_int_set(&rpacket->u.server_unknown_37.count,0);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	     account_unget_closed_characterlist(conn_get_account(c),charlist);
	     return 0;
	  }
	account_unget_closed_characterlist(conn_get_account(c),charlist);
	
	  {
	     char const *        tok1;
	     char const *        tok2;
	     t_character const * ch;
	     unsigned int        count;
	     
	     count = 0;
	     tok1 = (char const *)strtok(temp,","); /* strtok modifies the string it is passed */
	     tok2 = strtok(NULL,",");
	     while (tok1)
	       {
		  if (!tok2)
		    {
		       char const * tname;
		       
		       eventlog(eventlog_level_error,"handle_bnet_packet","[%d] account \"%s\" has bad character list \"%s\"",conn_get_socket(c),(tname = conn_get_username(c)),temp);
		       conn_unget_username(c,tname);
		       break;
		    }
		  
		  if ((ch = characterlist_find_character(tok1,tok2)))
		    {
		       packet_append_ntstring(rpacket,character_get_realmname(ch));
		       packet_append_ntstring(rpacket,",");
		       packet_append_string(rpacket,character_get_name(ch));
		       packet_append_string(rpacket,character_get_playerinfo(ch));
		       packet_append_string(rpacket,character_get_guildname(ch));
		       count++;
		    }
		  else
		    eventlog(eventlog_level_error,"handle_bnet_packet","[%d] character \"%s\" is missing",conn_get_socket(c),tok2);
		  tok1 = strtok(NULL,",");
		  tok2 = strtok(NULL,",");
	       }
	     free(temp);
	     
	     bn_int_set(&rpacket->u.server_unknown_37.count,count);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_unknown39(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_unknown_39)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad UNKNOWN_39 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_unknown_39),packet_get_size(packet));
      return -1;
   }
   return 0;
}

static int _client_adreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_adreq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ADREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_adreq),packet_get_size(packet));
      return -1;
   }
	    
     {
	t_adbanner * ad;
	
	if (!(ad = adbanner_pick(c,bn_int_get(packet->u.client_adreq.prev_adid))))
	  return -1;
	
	/*		    eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] picking ad file=\"%s\" id=0x%06x tag=%u",conn_get_socket(c),adbanner_get_filename(ad),adbanner_get_id(ad),adbanner_get_extensiontag(ad));*/
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_adreply));
	     packet_set_type(rpacket,SERVER_ADREPLY);
	     bn_int_set(&rpacket->u.server_adreply.adid,adbanner_get_id(ad));
	     bn_int_set(&rpacket->u.server_adreply.extensiontag,adbanner_get_extensiontag(ad));
	     file_to_mod_time(adbanner_get_filename(ad),&rpacket->u.server_adreply.timestamp);
	     packet_append_string(rpacket,adbanner_get_filename(ad));
	     packet_append_string(rpacket,adbanner_get_link(ad));
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_adack(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_adack)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ADACK packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_adack),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * tname;
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] ad acknowledgement for adid 0x%04x from \"%s\"",conn_get_socket(c),bn_int_get(packet->u.client_adack.adid),(tname = conn_get_chatname(c)));
	conn_unget_chatname(c,tname);
     }
   
   return 0;
}

static int _client_adclick(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_adclick)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ADCLICK packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_adclick),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * tname;
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] ad click for adid 0x%04x from \"%s\"",
		 conn_get_socket(c),bn_int_get(packet->u.client_adclick.adid),(tname = conn_get_username(c)));
	conn_unget_username(c,tname);
     }
   
   return 0;
}

static int _client_adclick2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_adclick2)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad ADCLICK2 packet (expected %u bytes, got %u)",conn_get_socket(c), sizeof(t_client_adclick2),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * tname;
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] ad click2 for adid 0x%04hx from \"%s\"", conn_get_socket(c),bn_int_get(packet->u.client_adclick2.adid),(tname = conn_get_username(c)));
	conn_unget_username(c,tname);
     }
   
     {
	t_adbanner * ad;
	
	if (!(ad = adbanner_pick(c,bn_int_get(packet->u.client_adclick2.adid))))
	  return -1;
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     packet_set_size(rpacket,sizeof(t_server_adclickreply2));
	     packet_set_type(rpacket,SERVER_ADCLICKREPLY2);
	     bn_int_set(&rpacket->u.server_adclickreply2.adid,adbanner_get_id(ad));
	     packet_append_string(rpacket,adbanner_get_link(ad));
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
     }
   
   return 0;
}

static int _client_statsupdate(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_statsupdate)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STATSUPDATE packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_statsupdate),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * name;
	char const * key;
	char const * val;
	unsigned int name_count;
	unsigned int key_count;
	unsigned int i,j;
	unsigned int name_off;
	unsigned int keys_off;
	unsigned int key_off;
	unsigned int vals_off;
	unsigned int val_off;
	t_account *  account;
	
	name_count = bn_int_get(packet->u.client_statsupdate.name_count);
	key_count = bn_int_get(packet->u.client_statsupdate.key_count);
	
	if (name_count!=1)
	  eventlog(eventlog_level_warn,"handle_bnet_packet","[%d] got suspicious STATSUPDATE packet (name_count=%u)",conn_get_socket(c),name_count);
	
	for (i=0,name_off=sizeof(t_client_statsupdate);
	     i<name_count && (name = packet_get_str_const(packet,name_off,UNCHECKED_NAME_STR));
	     i++,name_off+=strlen(name)+1);
	if (i<name_count)
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STATSUPDATE packet (only %u names of %u)",conn_get_socket(c),i,name_count);
	     return -1;
	  }
	keys_off = name_off;
	
	for (i=0,key_off=keys_off;
	     i<key_count && (key = packet_get_str_const(packet,key_off,MAX_ATTRKEY_STR));
	     i++,key_off+=strlen(key)+1);
	if (i<key_count)
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STATSUPDATE packet (only %u keys of %u)",conn_get_socket(c),i,key_count);
	     return -1;
	  }
	vals_off = key_off;
	
	if ((account = conn_get_account(c)))
	  {
	     char const * tname;
	     
	     if (account_get_auth_changeprofile(account)==0) /* default to true */
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] stats update for \"%s\" refused (no change profile access)",conn_get_socket(c),(tname = conn_get_username(c)));
		  conn_unget_username(c,tname);
		  return -1;
	       }
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] updating player profile for \"%s\"",conn_get_socket(c),(tname = conn_get_username(c)));
	     conn_unget_username(c,tname);
	     
	     for (i=0,name_off=sizeof(t_client_statsupdate);
		  i<name_count && (name = packet_get_str_const(packet,name_off,UNCHECKED_NAME_STR));
		  i++,name_off+=strlen(name)+1)
	       for (j=0,key_off=keys_off,val_off=vals_off;
		    j<key_count && (key = packet_get_str_const(packet,key_off,MAX_ATTRKEY_STR)) && (val = packet_get_str_const(packet,val_off,MAX_ATTRVAL_STR));
		    j++,key_off+=strlen(key)+1,val_off+=strlen(val)+1)
		 if (strlen(key)<9 || strncasecmp(key,"profile\\",8)!=0)
		   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got STATSUPDATE with suspicious key \"%s\" value \"%s\"",conn_get_socket(c),key,val);
	     else
	       account_set_strattr(account,key,val);
	  }
     }
   
   return 0;
}

static int _client_playerinforeq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_playerinforeq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PLAYERINFOREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_playerinforeq),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * username;
	char const * info;
	t_account *  account;
	
	if (!(username = packet_get_str_const(packet,sizeof(t_client_playerinforeq),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PLAYERINFOREQ (missing or too long username)",conn_get_socket(c));
	     return -1;
	  }
	if (!(info = packet_get_str_const(packet,sizeof(t_client_playerinforeq)+strlen(username)+1,MAX_PLAYERINFO_STR)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PLAYERINFOREQ (missing or too long info)",conn_get_socket(c));
	     return -1;
	  }
	
	if (info[0]!='\0')
	  conn_set_playerinfo(c,info);
	
	account = conn_get_account(c);
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_playerinforeply));
	packet_set_type(rpacket,SERVER_PLAYERINFOREPLY);
	
	if (account)
	  {
	     char const * tname;
	     char const * str;
	     
	     if (!(tname = account_get_name(account)))
	       str = username;
	     else
	       str = tname;
	     packet_append_string(rpacket,str);
	     packet_append_string(rpacket,conn_get_playerinfo(c));
	     packet_append_string(rpacket,str);
	     if (tname)
	       account_unget_name(tname);
	  }
	else
	  {
	     packet_append_string(rpacket,"");
	     packet_append_string(rpacket,"");
	     packet_append_string(rpacket,"");
	  }
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_progident2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_progident2)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad PROGIDENT2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_progident2),packet_get_size(packet));
      return -1;
   }
   
   /* d2 uses this packet with clienttag = 0 to request the channel list */
   if (bn_int_get(packet->u.client_progident2.clienttag))
     {
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] CLIENT_PROGIDENT2 clienttag=0x%08x",conn_get_socket(c),bn_int_get(packet->u.client_progident2.clienttag));
	
	/* Hmm... no archtag.  Hope we get it in CLIENT_AUTHREQ1 (but we won't if we use the shortcut) */
	
	if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_STARCRAFT)==0)
	  conn_set_clienttag(c,CLIENTTAG_STARCRAFT);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_BROODWARS)==0)
	  conn_set_clienttag(c,CLIENTTAG_BROODWARS);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_SHAREWARE)==0)
	  conn_set_clienttag(c,CLIENTTAG_SHAREWARE);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_DIABLORTL)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLORTL);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_DIABLOSHR)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLOSHR);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_WARCIIBNE)==0)
	  conn_set_clienttag(c,CLIENTTAG_WARCIIBNE);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_DIABLO2DV)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLO2DV);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_DIABLO2XP)==0)
	  conn_set_clienttag(c,CLIENTTAG_DIABLO2XP);
	else if (bn_int_tag_eq(packet->u.client_progident2.clienttag,CLIENTTAG_WARCRAFT3)==0)
	  conn_set_clienttag(c,CLIENTTAG_WARCRAFT3);
	else
	  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unknown client program type 0x%08x, don't expect this to work",conn_get_socket(c),bn_int_get(packet->u.client_progident2.clienttag));
     }
   
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_channellist));
	packet_set_type(rpacket,SERVER_CHANNELLIST);
	  {
	     t_channel *    ch;
	     t_elem const * curr;
	     
	     LIST_TRAVERSE_CONST(channellist(),curr)
	       {
		  ch = elem_get_data(curr);
		  if ((!prefs_get_hide_temp_channels() || channel_get_permanent(ch)) &&
		      (!channel_get_clienttag(ch) || strcmp(channel_get_clienttag(ch),conn_get_clienttag(c))==0))
		    packet_append_string(rpacket,channel_get_name(ch));
	       }
	  }
	packet_append_string(rpacket,"");
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket); 
     }
   
   return 0;
}

static int _client_joinchannel(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_joinchannel)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad JOINCHANNEL packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_joinchannel),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * cname;
	int          found=1;
	
	/* ADDED BY UNDYING SOULZZ 4/7/02 AND MODIFIED BY DJP FOR RANDOM FIX & WINS RACE FEATURE*/
	unsigned int 	acctlevel = 0;
	
	char		    tempplayerinfo[40];
	char		    raceicon; /* appeared in 1.03 */
	unsigned int	raceiconnumber;
	unsigned int    wins;
	
	acctlevel = account_get_highestladderlevel(conn_get_account(c));
	account_get_raceicon(conn_get_account(c), &raceicon, &raceiconnumber, &wins);
	if(acctlevel==0)
	  {
	     sprintf( tempplayerinfo, "3RAW 3RAW 0 0");
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] 3RAW 3RAW",conn_get_socket(c));
	  }
	else if (wins==0)
	  {
	     sprintf( tempplayerinfo, "3RAW 3RAW 0 0");
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] 3RAW 3RAW",conn_get_socket(c));
	  }
	else 
	  {
	     sprintf(tempplayerinfo, "3RAW %1u%c3W %u %u", raceiconnumber, raceicon, acctlevel, wins); 
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] 3RAW %1u%c3W %u",conn_get_socket(c), raceiconnumber, raceicon, acctlevel);
	  }
	
	if (!(cname = packet_get_str_const(packet,sizeof(t_client_joinchannel),CHANNEL_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad JOINCHANNEL (missing or too long cname)",conn_get_socket(c));
	     return -1;
	  }
	//ADDED THEUNDYING - UPDATED 7/31/02
	if(account_get_auth_admin(conn_get_account(c))>0)
	  conn_set_flags( c, MF_BLIZZARD );
	else if(account_get_auth_operator(conn_get_account(c),cname)>0)
	  conn_set_flags( c, MF_BNET );
	else
	  conn_set_flags( c, W3_ICON_SET );
	
	conn_set_w3_playerinfo( c, tempplayerinfo ); 
	
	if(account_get_new_at_team(conn_get_account(c))==1)
	  {
	     int temp;
	     temp = account_get_atteamcount(conn_get_account(c));
	     temp = temp-1;
	     account_set_atteamcount(conn_get_account(c),temp);
	     account_set_new_at_team(conn_get_account(c),0);
	  }
	
	switch (bn_int_get(packet->u.client_joinchannel.channelflag))
	  {
	   case CLIENT_JOINCHANNEL_NORMAL:
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_JOINCHANNEL_NORMAL channel \"%s\"",conn_get_socket(c),cname);
	     
	     if (channellist_find_channel_by_name(cname,conn_get_country(c),conn_get_realmname(c)))
	       break; /* just join it */
	     else if (prefs_get_ask_new_channel())
	       {
		  found=0;
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] didn't find channel \"%s\" to join",conn_get_socket(c),cname);
		  message_send_text(c,message_type_channeldoesnotexist,c,cname);
	       }
	     break;
	   case CLIENT_JOINCHANNEL_GENERIC:
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_JOINCHANNEL_GENERIC channel \"%s\"",conn_get_socket(c),cname);
	     /* don't have to do anything here */
	     break;
	   case CLIENT_JOINCHANNEL_CREATE:
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_JOINCHANNEL_CREATE channel \"%s\"",conn_get_socket(c),cname);
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_JOINCHANNEL_CREATE channel \"%s\"",conn_get_socket(c),cname);
	     /* don't have to do anything here */
	     break;
	  }
	
	if (found && conn_set_channel(c,cname)<0)
	  conn_set_channel(c,CHANNEL_NAME_BANNED); /* should not fail */
     }	
   
   if(conn_get_motd_loggedin(c)==0)
     {
	char msgtemp[255];
	
	if(strcasecmp(conn_get_clienttag(c),CLIENTTAG_WARCRAFT3)==0)
	  {
	     sprintf(msgtemp,"Welcome to the PvPGN Realm");
	     message_send_text(c,message_type_info,c,msgtemp);
	     sprintf(msgtemp,"This Server is hosted by: %s",prefs_get_contact_name());
	     message_send_text(c,message_type_info,c,msgtemp);
	     sprintf(msgtemp,"There are currently %u users online, playing %u games of Warcraft III,",
		     connlist_login_get_length(),
		     gamelist_get_length());
	     message_send_text(c,message_type_info,c,msgtemp);
	     sprintf(msgtemp,"and Chatting in %u channels.",channellist_get_length());
	     message_send_text(c,message_type_info,c,msgtemp);
	     
	  }
	
	conn_set_motd_loggedin(c);
     }
   
   return 0;
}

static int _client_message(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_message)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MESSAGE packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_message),packet_get_size(packet));
      return -1;
   }
   
     {
	char const *      text;
	t_channel const * channel;
	
	if (!(text = packet_get_str_const(packet,sizeof(t_client_message),MAX_MESSAGE_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MESSAGE (missing or too long text)",conn_get_socket(c));
	     return -1;
	  }
	
	conn_set_idletime(c);
	
	if ((channel = conn_get_channel(c)))
	  channel_message_log(channel,c,1,text);
	/* we don't log game commands currently */
	
	
	
	if (text[0]=='/')
	  handle_command(c,text);	
	else
	  if (channel && !conn_quota_exceeded(c,text))
	    channel_message_send(channel,message_type_talk,c,text);
	/* else discard */
     }
   
   return 0;
}

static int _client_gamelistreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_gamelistreq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad GAMELISTREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_gamelistreq),packet_get_size(packet));
      return -1;
   }
	    
     {
	char const *                gamename;
	unsigned short              bngtype;
	t_game_type                 gtype;
#ifndef WITH_BITS
	t_game *                    game;
	t_server_gamelistreply_game glgame;
	unsigned int                addr;
	unsigned short              port;
#endif
	
	if (!(gamename = packet_get_str_const(packet,sizeof(t_client_gamelistreq),GAME_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad GAMELISTREQ (missing or too long gamename)",conn_get_socket(c));
	     return -1;
	  }
	
	bngtype = bn_short_get(packet->u.client_gamelistreq.gametype);
	gtype = bngreqtype_to_gtype(conn_get_clienttag(c),bngtype);
#ifdef WITH_BITS
	/* FIXME: what about maxgames? */
	bits_game_handle_client_gamelistreq(c,gtype,0,gamename);
#else		
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_gamelistreply));
	packet_set_type(rpacket,SERVER_GAMELISTREPLY);
	
	// re added by bbf (yak)
	bn_int_set(&rpacket->u.server_gamelistreply.unknown,0); // yak
	
	/* specific game requested? */
	if (gamename[0]!='\0')
	  {
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] GAMELISTREPLY looking for specific game tag=\"%s\" bngtype=0x%08x gtype=%d name=\"%s\"",conn_get_socket(c),conn_get_clienttag(c),bngtype,(int)gtype,gamename);
	     if ((game = gamelist_find_game(gamename,gtype)))
	       {
		  //	removed by bbf (yak)
		  //			bn_int_set(&glgame.unknown7,SERVER_GAMELISTREPLY_GAME_UNKNOWN7); // not in yak
													   bn_short_set(&glgame.gametype,gtype_to_bngtype(game_get_type(game)));
		  bn_short_set(&glgame.unknown1,SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
		  bn_short_set(&glgame.unknown3,SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
		  addr = game_get_addr(game);
		  port = game_get_port(game);
		  gametrans_net(conn_get_addr(c),conn_get_port(c),conn_get_local_addr(c),conn_get_local_port(c),&addr,&port);
		  bn_short_nset(&glgame.port,port);
		  bn_int_nset(&glgame.game_ip,addr);
		  bn_int_set(&glgame.unknown4,SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
		  bn_int_set(&glgame.unknown5,SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
		  switch (game_get_status(game))
		    {
		     case game_status_started:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_STARTED);
		       break;
		     case game_status_full:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_FULL);
		       break;
		     case game_status_open:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
		       break;
		     case game_status_done:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_DONE);
		       break;
		     default:
		       eventlog(eventlog_level_warn,"handle_bnet_packet","[%d] game \"%s\" has bad status %d",conn_get_socket(c),game_get_name(game),game_get_status(game));
		       bn_int_set(&glgame.status,0);
		    }
		  bn_int_set(&glgame.unknown6,SERVER_GAMELISTREPLY_GAME_UNKNOWN6);
		  
		  packet_append_data(rpacket,&glgame,sizeof(glgame));
		  packet_append_string(rpacket,game_get_name(game));
		  packet_append_string(rpacket,game_get_pass(game));
		  packet_append_string(rpacket,game_get_info(game));
		  bn_int_set(&rpacket->u.server_gamelistreply.gamecount,1);
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] GAMELISTREPLY found it",conn_get_socket(c));
	       }
	     else
	       {
		  bn_int_set(&rpacket->u.server_gamelistreply.gamecount,0);
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] GAMELISTREPLY doesn't seem to exist",conn_get_socket(c));
	       }
	  }
	else /* list all public games of this type */
	  {
	     unsigned int   counter=0;
	     unsigned int   tcount;
	     t_elem const * curr;
	     bn_int game_spacer = { 1, 0 ,0 ,0 };
	     
	     if (gtype==game_type_all)
	       eventlog(eventlog_level_debug,"handle_bnet_packet","GAMELISTREPLY looking for public games tag=\"%s\" bngtype=0x%08x gtype=all",conn_get_clienttag(c),bngtype);
	     else
	       eventlog(eventlog_level_debug,"handle_bnet_packet","GAMELISTREPLY looking for public games tag=\"%s\" bngtype=0x%08x gtype=%d",conn_get_clienttag(c),bngtype,(int)gtype);
	     
	     counter = 0;
	     tcount = 0;
	     LIST_TRAVERSE_CONST(gamelist(),curr)
	       {
		  game = elem_get_data(curr);
		  tcount++;
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] considering listing game=\"%s\", pass=\"%s\" clienttag=\"%s\" gtype=%d",conn_get_socket(c),game_get_name(game),game_get_pass(game),game_get_clienttag(game),(int)game_get_type(game));
		  
		  if (prefs_get_hide_pass_games() && strcmp(game_get_pass(game),"")!=0)
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] not listing because game is private",conn_get_socket(c));
		       continue;
		    }
		  if (prefs_get_hide_pass_games() && game_get_flag_private(game))
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] not listing because game has private flag",conn_get_socket(c));
		       continue;
		    }
		  if (prefs_get_hide_started_games() && game_get_status(game)!=game_status_open)
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] not listing because game is started",conn_get_socket(c));
		       continue;
		    }
		  if (strcmp(game_get_clienttag(game),conn_get_clienttag(c))!=0)
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] not listing because game is for a different client",conn_get_socket(c));
		       continue;
		    }
		  if (gtype!=game_type_all && game_get_type(game)!=gtype)
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] not listing because game is wrong type",conn_get_socket(c));
		       continue;
		    }
		  
		  // removed by bbf (yak)			
		  //			bn_int_set(&glgame.unknown7,SERVER_GAMELISTREPLY_GAME_UNKNOWN7); // not in yak
													   bn_short_set(&glgame.gametype,gtype_to_bngtype(game_get_type(game)));
		  bn_short_set(&glgame.unknown1,SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
		  bn_short_set(&glgame.unknown3,SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
		  addr = game_get_addr(game);
		  port = game_get_port(game);
		  gametrans_net(conn_get_addr(c),conn_get_port(c),conn_get_local_addr(c),conn_get_local_port(c),&addr,&port);
		  bn_short_nset(&glgame.port,port);
		  bn_int_nset(&glgame.game_ip,addr);
		  bn_int_set(&glgame.unknown4,SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
		  bn_int_set(&glgame.unknown5,SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
		  switch (game_get_status(game))
		    {
		     case game_status_started:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_STARTED);
		       break;
		     case game_status_full:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_FULL);
		       break;
		     case game_status_open:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
		       break;
		     case game_status_done:
		       bn_int_set(&glgame.status,SERVER_GAMELISTREPLY_GAME_STATUS_DONE);
		       break;
		     default:
		       eventlog(eventlog_level_warn,"handle_bnet_packet","[%d] game \"%s\" has bad status=%d",conn_get_socket(c),game_get_name(game),(int)game_get_status(game));
		       bn_int_set(&glgame.status,0);
		    }
		  bn_int_set(&glgame.unknown6,SERVER_GAMELISTREPLY_GAME_UNKNOWN6);
		  
		  if (packet_get_size(rpacket)+
		      sizeof(glgame)+
		      strlen(game_get_name(game))+1+
		      strlen(game_get_pass(game))+1+
		      strlen(game_get_info(game))+1>MAX_PACKET_SIZE)
		    {
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] out of room for games",conn_get_socket(c));
		       break; /* no more room */
		    }
		  
		  if( counter > 0 )
		    {
		       packet_append_data(rpacket, &game_spacer, sizeof(game_spacer)); // bbf -> yak
		    }
		  
		  packet_append_data(rpacket,&glgame,sizeof(glgame));
		  packet_append_string(rpacket,game_get_name(game));
		  packet_append_string(rpacket,game_get_pass(game));
		  packet_append_string(rpacket,game_get_info(game));
		  counter++;
	       }
	     
	     /*
	      removed again... bbf (yak)
	      if (counter==0)
	      {
	      // nok
	      unsigned int magictemp=1;
	      packet_append_data(rpacket,&magictemp,sizeof(magictemp));
	      }// - yakz's does not use this, but it seems to be required
	      */		    
	     bn_int_set(&rpacket->u.server_gamelistreply.gamecount,counter);
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] GAMELISTREPLY sent %u of %u games",conn_get_socket(c),counter,tcount);
	  }
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
#endif  /* WITH_BITS */
     }
   
   return 0;
}

static int _client_joingame(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_join_game)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad JOIN_GAME packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_join_game),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * gamename;
	char const * gamepass;
	char const * tname;
#ifndef WITH_BITS
	t_game *     game;
#else
	t_bits_game * game;
#endif
	t_game_type  gtype = game_type_none;
	
	if (!(gamename = packet_get_str_const(packet,sizeof(t_client_join_game),GAME_NAME_LEN))) {
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_JOIN_GAME (missing or too long gamename)",conn_get_socket(c));
	   return -1;
	}
	
	if (!(gamepass = packet_get_str_const(packet,sizeof(t_client_join_game)+strlen(gamename)+1,GAME_PASS_LEN))) {
	   eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad CLIENT_JOIN_GAME packet (missing or too long gamepass)",conn_get_socket(c));
	   return -1;
	}
		
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] trying to join game \"%s\" pass=\"%s\"",conn_get_socket(c),gamename,gamepass);
	if(!strcmp(gamename, "BNet"))
	  {
	     handle_anongame_join(c);
	     //to prevent whispering over and over that user joined channel
	     if(conn_get_joingamewhisper_ack(c)==0)
	       {
		  watchlist_notify_event(conn_get_account(c),gamename,watch_event_joingame);
		  conn_set_joingamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
	       }
	     
	     gtype = game_type_anongame;
	  }
	else 
	  {
	     //to prevent whispering over and over that user joined channel
	     if(conn_get_joingamewhisper_ack(c)==0)
	       {
		  if(watchlist_notify_event(conn_get_account(c),gamename,watch_event_joingame)==0)
		    eventlog(eventlog_level_info,"handle_bnet","Told Mutual Friends your in game %s",gamename);
		  
		  conn_set_joingamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
	       }
	     
#ifndef WITH_BITS
	     if (!(game = gamelist_find_game(gamename,game_type_all)))
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unable to find game \"%s\" for user to join",conn_get_socket(c),gamename);
		  return -1;
	       }
	     gtype = game_get_type(game);
	     gamename = game_get_name(game);
#else /* WITH_BITS */
	     if (!(game = bits_gamelist_find_game(gamename,game_type_all)))
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] unable to find game \"%s\" for user to join",conn_get_socket(c),gamename);
		  return -1;
	       }
	     gtype = bits_game_get_type(game);
	     gamename = bits_game_get_name(game);
#endif /* WITH_BITS */
	     /* bits: since the connection needs the account it should already be loaded so there should be no problem */		
	     if ((gtype==game_type_ladder && account_get_auth_joinladdergame(conn_get_account(c))==0) || /* default to true */
		 (gtype!=game_type_ladder && account_get_auth_joinnormalgame(conn_get_account(c))==0)) /* default to true */
	       {
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game join for \"%s\" to \"%s\" refused (no authority)",conn_get_socket(c),(tname = conn_get_username(c)),gamename);
		  conn_unget_username(c,tname);
		  /* If the user is not in a game, then map authorization
		   will fail and keep them from playing. */
		  return -1;
	       }
	  }
	
	if (conn_set_game(c,gamename,gamepass,"",gtype,STARTVER_UNKNOWN)<0)
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" joined game \"%s\", but could not be recorded on server",conn_get_socket(c),(tname = conn_get_username(c)),gamename);
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] \"%s\" joined game \"%s\"",conn_get_socket(c),(tname = conn_get_username(c)),gamename);
	conn_unget_username(c,tname);
	
	// W2 hack to part channel on game join
	if (conn_get_channel(c))
	  conn_set_channel(c,NULL);
	
     }
   
   return 0;
}

static int _client_startgame1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_startgame1)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_startgame1),packet_get_size(packet));
      return -1;
   }
   
     {
	char const *   gamename;
	char const *   gamepass;
	char const *   gameinfo;
	unsigned short bngtype;
	unsigned int   status;
#ifndef WITH_BITS
	t_game *       currgame;
#endif
	
	if (!(gamename = packet_get_str_const(packet,sizeof(t_client_startgame1),GAME_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME1 packet (missing or too long gamename)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gamepass = packet_get_str_const(packet,sizeof(t_client_startgame1)+strlen(gamename)+1,GAME_PASS_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME1 packet (missing or too long gamepass)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gameinfo = packet_get_str_const(packet,sizeof(t_client_startgame1)+strlen(gamename)+1+strlen(gamepass)+1,GAME_INFO_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME1 packet (missing or too long gameinfo)",conn_get_socket(c));
	     return -1;
	  }
	if(conn_get_joingamewhisper_ack(c)==0)
	  {
	     if(watchlist_notify_event(conn_get_account(c),gamename,watch_event_joingame)==0)
	       eventlog(eventlog_level_info,"handle_bnet","Told Mutual Friends your in game %s",gamename);
	     
	     conn_set_joingamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
	  }
	
	
	bngtype = bn_short_get(packet->u.client_startgame1.gametype);
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got startgame1 status for game \"%s\" is 0x%08x (gametype = 0x%04hx)",conn_get_socket(c),gamename,bn_int_get(packet->u.client_startgame1.status),bngtype);
	status = bn_int_get(packet->u.client_startgame1.status)&CLIENT_STARTGAME1_STATUSMASK;
	
#ifdef WITH_BITS
	bits_game_handle_startgame1(c,gamename,gamepass,gameinfo,bngtype,status);
#else
	if ((currgame = conn_get_game(c)))
	  {
	     switch (status)
	       {
		case CLIENT_STARTGAME1_STATUS_STARTED:
		  game_set_status(currgame,game_status_started);
		  break;
		case CLIENT_STARTGAME1_STATUS_FULL:
		  game_set_status(currgame,game_status_full);
		  break;
		case CLIENT_STARTGAME1_STATUS_OPEN:
		  game_set_status(currgame,game_status_open);
		  break;
		case CLIENT_STARTGAME1_STATUS_DONE:
		  game_set_status(currgame,game_status_done);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game \"%s\" is finished",conn_get_socket(c),gamename);
		  break;
	       }
	  }
	else
	  if (status!=CLIENT_STARTGAME1_STATUS_DONE)
	    {
	       t_game_type gtype;
	       
	       gtype = bngtype_to_gtype(conn_get_clienttag(c),bngtype);
	       if ((gtype==game_type_ladder && account_get_auth_createladdergame(conn_get_account(c))==0) || /* default to true */
		   (gtype!=game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c))==0)) /* default to true */
		 {
		    char const * tname;
		    
		    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game start for \"%s\" refused (no authority)",conn_get_socket(c),(tname = conn_get_username(c)));
		    conn_unget_username(c,tname);
		 }
	       else
		 conn_set_game(c,gamename,gamepass,gameinfo,gtype,STARTVER_GW1);
	       
	       if ((rpacket = packet_create(packet_class_bnet)))
		 {
		    packet_set_size(rpacket,sizeof(t_server_startgame1_ack));
		    packet_set_type(rpacket,SERVER_STARTGAME1_ACK);
		    
		    if (conn_get_game(c))
		      bn_int_set(&rpacket->u.server_startgame1_ack.reply,SERVER_STARTGAME1_ACK_OK);
		    else
		      bn_int_set(&rpacket->u.server_startgame1_ack.reply,SERVER_STARTGAME1_ACK_NO);
		    
		    queue_push_packet(conn_get_out_queue(c),rpacket);
		    packet_del_ref(rpacket);
		 }
	    }
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client tried to set game status to DONE",conn_get_socket(c));
#endif /* !WITH_BITS */
     }
   
   return 0;
}

static int _client_startgame3(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_startgame3)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME3 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_startgame3),packet_get_size(packet));
      return -1;
   }
   
     {
	char const *   gamename;
	char const *   gamepass;
	char const *   gameinfo;
	unsigned short bngtype;
	unsigned int   status;
#ifndef WITH_BITS
	t_game *       currgame;
#endif
	
	if (!(gamename = packet_get_str_const(packet,sizeof(t_client_startgame3),GAME_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME3 packet (missing or too long gamename)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gamepass = packet_get_str_const(packet,sizeof(t_client_startgame3)+strlen(gamename)+1,GAME_PASS_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME3 packet (missing or too long gamepass)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gameinfo = packet_get_str_const(packet,sizeof(t_client_startgame3)+strlen(gamename)+1+strlen(gamepass)+1,GAME_INFO_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME3 packet (missing or too long gameinfo)",conn_get_socket(c));
	     return -1;
	  }
	if(conn_get_joingamewhisper_ack(c)==0)
	  {
	     if(watchlist_notify_event(conn_get_account(c),gamename,watch_event_joingame)==0)
	       eventlog(eventlog_level_info,"handle_bnet","Told Mutual Friends your in game %s",gamename);
	     
	     conn_set_joingamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
	  }
	bngtype = bn_short_get(packet->u.client_startgame3.gametype);
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got startgame3 status for game \"%s\" is 0x%08x (gametype = 0x%04hx)",conn_get_socket(c),gamename,bn_int_get(packet->u.client_startgame3.status),bngtype);
	status = bn_int_get(packet->u.client_startgame3.status)&CLIENT_STARTGAME3_STATUSMASK;
	
#ifdef WITH_BITS
	bits_game_handle_startgame3(c,gamename,gamepass,gameinfo,bngtype,status);
#else
	if ((currgame = conn_get_game(c)))
	  {
	     switch (status)
	       {
		case CLIENT_STARTGAME3_STATUS_STARTED:
		  game_set_status(currgame,game_status_started);
		  break;
		case CLIENT_STARTGAME3_STATUS_FULL:
		  game_set_status(currgame,game_status_full);
		  break;
		case CLIENT_STARTGAME3_STATUS_OPEN1:
		case CLIENT_STARTGAME3_STATUS_OPEN:
		  game_set_status(currgame,game_status_open);
		  break;
		case CLIENT_STARTGAME3_STATUS_DONE:
		  game_set_status(currgame,game_status_done);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game \"%s\" is finished",conn_get_socket(c),gamename);
		  break;
	       }
	  }
	else
	  if (status!=CLIENT_STARTGAME3_STATUS_DONE)
	    {
	       t_game_type gtype;
	       
	       gtype = bngtype_to_gtype(conn_get_clienttag(c),bngtype);
	       if ((gtype==game_type_ladder && account_get_auth_createladdergame(conn_get_account(c))==0) ||
		   (gtype!=game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c))==0))
		 {
		    char const * tname;
		    
		    eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game start for \"%s\" refused (no authority)",conn_get_socket(c),(tname = conn_get_username(c)));
		    conn_unget_username(c,tname);
		 }
	       else
		 conn_set_game(c,gamename,gamepass,gameinfo,gtype,STARTVER_GW3);
	       
	       if ((rpacket = packet_create(packet_class_bnet)))
		 {
		    packet_set_size(rpacket,sizeof(t_server_startgame3_ack));
		    packet_set_type(rpacket,SERVER_STARTGAME3_ACK);
		    
		    if (conn_get_game(c))
		      bn_int_set(&rpacket->u.server_startgame3_ack.reply,SERVER_STARTGAME3_ACK_OK);
		    else
		      bn_int_set(&rpacket->u.server_startgame3_ack.reply,SERVER_STARTGAME3_ACK_NO);
		    
		    queue_push_packet(conn_get_out_queue(c),rpacket);
		    packet_del_ref(rpacket);
		 }
	    }
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client tried to set game status to DONE",conn_get_socket(c));
#endif /* !WITH_BITS */
     }
   
   return 0;
}

static int _client_startgame4(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_startgame4)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME4 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_startgame4),packet_get_size(packet));
      return -1;
   }
   
   // Quick hack to make W3 part channels when creating a game
   if (conn_get_channel(c))
     conn_set_channel(c,NULL);
   
     {
	char const *   gamename;
	char const *   gamepass;
	char const *   gameinfo;
	unsigned short bngtype;
	unsigned int   status;
	unsigned int   flag;
	unsigned short option;
	
	
#ifndef WITH_BITS
	t_game *       currgame;
#endif
	
	if (!(gamename = packet_get_str_const(packet,sizeof(t_client_startgame4),GAME_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME4 packet (missing or too long gamename)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gamepass = packet_get_str_const(packet,sizeof(t_client_startgame4)+strlen(gamename)+1,GAME_PASS_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME4 packet (missing or too long gamepass)",conn_get_socket(c));
	     return -1;
	  }
	if (!(gameinfo = packet_get_str_const(packet,sizeof(t_client_startgame4)+strlen(gamename)+1+strlen(gamepass)+1,GAME_INFO_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad STARTGAME4 packet (missing or too long gameinfo)",conn_get_socket(c));
	     return -1;
	  }
	if(conn_get_joingamewhisper_ack(c)==0)
	  {
	     if(watchlist_notify_event(conn_get_account(c),gamename,watch_event_joingame)==0)
	       eventlog(eventlog_level_info,"handle_bnet","Told Mutual Friends your in game %s",gamename);
	     
	     conn_set_joingamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
	  }
	bngtype = bn_short_get(packet->u.client_startgame4.gametype);
	option = bn_short_get(packet->u.client_startgame4.option);
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got startgame4 status for game \"%s\" is 0x%08x (gametype=0x%04hx option=0x%04x)",conn_get_socket(c),gamename,bn_int_get(packet->u.client_startgame4.status),bngtype,option);
	//status = bn_int_get(packet->u.client_startgame4.status)&CLIENT_STARTGAME4_STATUSMASK;
	
	status = bn_short_get(packet->u.client_startgame4.status); //&CLIENT_STARTGAME4_STATUSMASK;
	flag = bn_short_get(packet->u.client_startgame4.flag);
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] startgame4 status: %02x", conn_get_socket(c), status);
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] startgame4 flag: %02x", conn_get_socket(c), flag);
	
#ifdef WITH_BITS
	bits_game_handle_startgame4(c,gamename,gamepass,gameinfo,bngtype,status,option);
	// W3_FIXME: private games won't work with BITS
#else
	/* MODIFIED BY UNDYING SOULZZ 4/3/02 */
	if ((currgame = conn_get_game(c)))
	  {
	     switch (status)
	       {
		case CLIENT_STARTGAME4_STATUS_STARTED:
		  /* Dizzy : my war3 is sending different status when started as shown bellow:
		   Aug 08 01:54:42 handle_bnet_packet: [10] got startgame4 status for game "test2" is 0x00000008 (gametype=0x0009 option=0x0000)
		   Aug 08 01:54:42 handle_bnet_packet: [10] startgame4 status: 08
		   Aug 08 01:54:42 handle_bnet_packet: [10] startgame4 flag: 00
		   Aug 08 01:54:42 handle_bnet_packet: [10] unknown startgame4 status 8 */
		case CLIENT_STARTGAME4_STATUS_STARTED_W3:
		  game_set_status(currgame,game_status_started);
		  break;
		case CLIENT_STARTGAME4_STATUS_FULL1:
		case CLIENT_STARTGAME4_STATUS_FULL2:
		  game_set_status(currgame,game_status_full);
		  break;
		case CLIENT_STARTGAME4_STATUS_INIT:
		case CLIENT_STARTGAME4_STATUS_OPEN1:
		case CLIENT_STARTGAME4_STATUS_OPEN2:
		case CLIENT_STARTGAME4_STATUS_OPEN3:
		  
		case CLIENT_STARTGAME4_STATUS_OPEN1_W3:
		case CLIENT_STARTGAME4_STATUS_OPEN2_W3:
		  
		  game_set_status(currgame,game_status_open);
		  break;
		case CLIENT_STARTGAME4_STATUS_DONE1:
		case CLIENT_STARTGAME4_STATUS_DONE2:
		  game_set_status(currgame,game_status_done);
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game \"%s\" is finished",conn_get_socket(c),gamename);
		  break;
		default:
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] unknown startgame4 status %d",conn_get_socket(c),status);
	       }
	  }
	if (status!=CLIENT_STARTGAME4_STATUS_DONE1 &&
	    status!=CLIENT_STARTGAME4_STATUS_DONE2)
	  {
	     t_game_type gtype;
	     
	     gtype = bngtype_to_gtype(conn_get_clienttag(c),bngtype);
	     if ((gtype==game_type_ladder && account_get_auth_createladdergame(conn_get_account(c))==0) ||
		 (gtype!=game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c))==0))
	       {
		  char const * tname;
		  
		  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] game start for \"%s\" refused (no authority)",conn_get_socket(c),(tname = conn_get_username(c)));
		  conn_unget_username(c,tname);
	       }
	     // [quetzal] 20020831 - _OPEN2_W3 game status code
	     // is only sent by game creator => no need to do conn_set_game for him
	     else if (status != CLIENT_STARTGAME4_STATUS_OPEN2_W3 &&
		      conn_set_game(c,gamename,gamepass,gameinfo,gtype,STARTVER_GW4)==0) {
		game_set_option(conn_get_game(c),bngoption_to_goption(conn_get_clienttag(c),gtype,option));
		
		if (flag & 0x0001) {
		   eventlog(eventlog_level_debug,"handle_bnet_packet","game created with private flag");
		   game_set_flag_private(conn_get_game(c),1);
		}
	     }
	     
	     if ((rpacket = packet_create(packet_class_bnet)))
	       {
		  packet_set_size(rpacket,sizeof(t_server_startgame4_ack));
		  packet_set_type(rpacket,SERVER_STARTGAME4_ACK);
		  
		  if (conn_get_game(c))
		    bn_int_set(&rpacket->u.server_startgame4_ack.reply,SERVER_STARTGAME4_ACK_OK);
		  else
		    bn_int_set(&rpacket->u.server_startgame4_ack.reply,SERVER_STARTGAME4_ACK_NO);
		  queue_push_packet(conn_get_out_queue(c),rpacket);
		  packet_del_ref(rpacket);
	       }
	  }
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client tried to set game status to DONE",conn_get_socket(c));
#endif /* !WITH_BITS */
     }
   
   /* First, send an ECHO_REQ */
   if ((rpacket = packet_create(packet_class_bnet)))
     {
	packet_set_size(rpacket,sizeof(t_server_echoreq));
	packet_set_type(rpacket,SERVER_ECHOREQ);
	bn_int_set(&rpacket->u.server_echoreq.ticks,get_ticks());
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_closegame(t_connection * c, t_packet const * const packet)
{
   eventlog(eventlog_level_info,"handle_bnet_packet","[%d] client closing game",conn_get_socket(c));
   conn_set_game(c,NULL,NULL,NULL,game_type_none,0);

   //to prevent whispering over and over that user joined channel
   if(conn_get_leavegamewhisper_ack(c)==0)
     {
       watchlist_notify_event(conn_get_account(c),NULL,watch_event_leavegame);
       conn_set_leavegamewhisper_ack(c,1); //1 = already whispered. We reset this each time user joins a channel
     }

   return 0;
}

static int _client_gamereport(t_connection * c, t_packet const * const packet)
{
   if (packet_get_size(packet)<sizeof(t_client_game_report)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad GAME_REPORT packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_game_report),packet_get_size(packet));
      return -1;
   }
   
#ifdef WITH_BITS
   if (!bits_master) {
      /* send the packet to the master server */
      send_bits_game_report(c,packet);
   } else
#endif	    
     {
	t_account *                         my_account;
	t_account *                         other_account;
	t_game_result                       my_result=game_result_none;
	t_game *                            game;
	unsigned int                        player_count;
	unsigned int                        i;
	t_client_game_report_result const * result_data;
	unsigned int                        result_off;
	t_game_result                       result;
	char const *                        player;
	unsigned int                        player_off;
	char const *                        tname;
	
	player_count = bn_int_get(packet->u.client_gamerep.count);
	
	if (!(game = conn_get_game(c)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got GAME_REPORT when not in a game for user \"%s\"",conn_get_socket(c),(tname = conn_get_username(c)));
	     conn_unget_username(c,tname);
	     return -1;
	  }
	
	eventlog(eventlog_level_info,"handle_bnet_packet","[%d] CLIENT_GAME_REPORT: %s (%u players)",conn_get_socket(c),(tname = conn_get_username(c)),player_count);
	my_account = conn_get_account(c);
	
	for (i=0,result_off=sizeof(t_client_game_report),player_off=sizeof(t_client_game_report)+player_count*sizeof(t_client_game_report_result);
	     i<player_count;
	     i++,result_off+=sizeof(t_client_game_report_result),player_off+=strlen(player)+1)
	  {
	     if (!(result_data = packet_get_data_const(packet,result_off,sizeof(t_client_game_report_result))))
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got corrupt GAME_REPORT packet (missing results %u-%u)",conn_get_socket(c),i+1,player_count);
		  break;
	       }
	     if (!(player = packet_get_str_const(packet,player_off,USER_NAME_MAX)))
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got corrupt GAME_REPORT packet (missing players %u-%u)",conn_get_socket(c),i+1,player_count);
		  break;
	       }
	     
	     result = bngresult_to_gresult(bn_int_get(result_data->result));
	     eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got player %d (\"%s\") result 0x%08x",conn_get_socket(c),i,player,result);
	     
	     if (player[0]=='\0') /* empty slots have empty player name */
	       continue;
	     
	     if (!(other_account = accountlist_find_account(player)))
	       {
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got GAME_REPORT with unknown player \"%s\"",conn_get_socket(c),player);
		  break;
	       }
	     
	     if (my_account==other_account)
	       my_result = result;
	     else
	       if (game_check_result(game,other_account,result)<0)
		 break;
	  }
	
	conn_unget_username(c,tname);
	
	if (i==player_count) /* if everything checked out... */
	  {
	     char const * head;
	     char const * body;
	     
	     if (!(head = packet_get_str_const(packet,player_off,MAX_GAMEREP_HEAD_STR)))
	       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got GAME_REPORT with missing or too long report head",conn_get_socket(c));
	     else
	       {
		  player_off += strlen(head)+1;
		  if (!(body = packet_get_str_const(packet,player_off,MAX_GAMEREP_BODY_STR)))
		    eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] got GAME_REPORT with missing or too long report body",conn_get_socket(c));
		  else
		    game_set_result(game,my_account,my_result,head,body); /* finally we can report the info */
	       }
	  }
	
	eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] finished parsing result... now leaving game",conn_get_socket(c));
	conn_set_game(c,NULL,NULL,NULL,game_type_none,0);
     }
   
   return 0;
}

static int _client_leavechannel(t_connection * c, t_packet const * const packet)
{
   /* If this user in a channel, notify everyone that the user has left */
   if (conn_get_channel(c))
     conn_set_channel(c,NULL);
   return 0;
}

static int _client_ladderreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   
   if (packet_get_size(packet)<sizeof(t_client_ladderreq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LADDERREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_ladderreq),packet_get_size(packet));
      return -1;
   }
   
     {
	t_ladder_entry entry;
	unsigned int   i;
	unsigned int   type;
	unsigned int   start;
	unsigned int   count;
	unsigned int   idnum;
	t_account *    account;
	char const *   clienttag;
	char const *   tname;
	char const *   timestr;
	t_bnettime     bt;
	t_ladder_id    id;
	
	clienttag = conn_get_clienttag(c);
	
	type = bn_int_get(packet->u.client_ladderreq.type);
	start = bn_int_get(packet->u.client_ladderreq.startplace);
	count = bn_int_get(packet->u.client_ladderreq.count);
	idnum = bn_int_get(packet->u.client_ladderreq.id);
	
	/* eventlog(eventlog_level_debug,"handle_bnet_packet","got LADDERREQ type=%u start=%u count=%u id=%u",type,start,count,id); */
	
	switch (idnum)
	  {
	   case CLIENT_LADDERREQ_ID_STANDARD:
	     id = ladder_id_normal;
	     break;
	   case CLIENT_LADDERREQ_ID_IRONMAN:
	     id = ladder_id_ironman;
	     break;
	   default:
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got unknown ladder ladderreq.id=0x%08x",conn_get_socket(c),idnum);
	     id = ladder_id_normal;
	  }
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_ladderreply));
	packet_set_type(rpacket,SERVER_LADDERREPLY);
	
	bn_int_tag_set(&rpacket->u.server_ladderreply.clienttag,clienttag);
	bn_int_set(&rpacket->u.server_ladderreply.id,idnum);
	bn_int_set(&rpacket->u.server_ladderreply.type,type);
	bn_int_set(&rpacket->u.server_ladderreply.startplace,start);
	bn_int_set(&rpacket->u.server_ladderreply.count,count);
	
	for (i=start; i<start+count; i++)
	  {
	     switch (type)
	       {
		case CLIENT_LADDERREQ_TYPE_HIGHESTRATED:
		  if (!(account = ladder_get_account_by_rank(i+1,ladder_sort_highestrated,ladder_time_active,clienttag,id)))
		    account = ladder_get_account_by_rank(i+1,ladder_sort_highestrated,ladder_time_current,clienttag,id);
		  break;
		case CLIENT_LADDERREQ_TYPE_MOSTWINS:
		  if (!(account = ladder_get_account_by_rank(i+1,ladder_sort_mostwins,ladder_time_active,clienttag,id)))
		    account = ladder_get_account_by_rank(i+1,ladder_sort_mostwins,ladder_time_current,clienttag,id);
		  break;
		case CLIENT_LADDERREQ_TYPE_MOSTGAMES:
		  if (!(account = ladder_get_account_by_rank(i+1,ladder_sort_mostgames,ladder_time_active,clienttag,id)))
		    account = ladder_get_account_by_rank(i+1,ladder_sort_mostgames,ladder_time_current,clienttag,id);
		  break;
		default:
		  account = NULL;
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got unknown value for ladderreq.type=%u",conn_get_socket(c),type);
	       }
	     
	     if (account)
	       {
		  bn_int_set(&entry.active.wins,account_get_ladder_active_wins(account,clienttag,id));
		  bn_int_set(&entry.active.loss,account_get_ladder_active_losses(account,clienttag,id));
		  bn_int_set(&entry.active.disconnect,account_get_ladder_active_disconnects(account,clienttag,id));
		  bn_int_set(&entry.active.rating,account_get_ladder_active_rating(account,clienttag,id));
		  bn_int_set(&entry.active.unknown,10); /* FIXME: rank,draws,?! */
		  if (!(timestr = account_get_ladder_active_last_time(account,clienttag,id)))
		    timestr = BNETD_LADDER_DEFAULT_TIME;
		  bnettime_set_str(&bt,timestr);
		  bnettime_to_bn_long(bt,&entry.lastgame_active);
		  
		  bn_int_set(&entry.current.wins,account_get_ladder_wins(account,clienttag,id));
		  bn_int_set(&entry.current.loss,account_get_ladder_losses(account,clienttag,id));
		  bn_int_set(&entry.current.disconnect,account_get_ladder_disconnects(account,clienttag,id));
		  bn_int_set(&entry.current.rating,account_get_ladder_rating(account,clienttag,id));
		  bn_int_set(&entry.current.unknown,5); /* FIXME: rank,draws,?! */
		  if (!(timestr = account_get_ladder_last_time(account,clienttag,id)))
		    timestr = BNETD_LADDER_DEFAULT_TIME;
		  bnettime_set_str(&bt,timestr);
		  bnettime_to_bn_long(bt,&entry.lastgame_current);
	       }
	     else
	       {
		  bn_int_set(&entry.active.wins,0);
		  bn_int_set(&entry.active.loss,0);
		  bn_int_set(&entry.active.disconnect,0);
		  bn_int_set(&entry.active.rating,0);
		  bn_int_set(&entry.active.unknown,0);
		  bn_long_set_a_b(&entry.lastgame_active,0,0);
		  
		  bn_int_set(&entry.current.wins,0);
		  bn_int_set(&entry.current.loss,0);
		  bn_int_set(&entry.current.disconnect,0);
		  bn_int_set(&entry.current.rating,0);
		  bn_int_set(&entry.current.unknown,0);
		  bn_long_set_a_b(&entry.lastgame_current,0,0);
	       }
	     
	     bn_int_set(&entry.ttest[0],0); /* FIXME: what is ttest? */
	     bn_int_set(&entry.ttest[1],0);
	     bn_int_set(&entry.ttest[2],0);
	     bn_int_set(&entry.ttest[3],0);
	     bn_int_set(&entry.ttest[4],0);
	     bn_int_set(&entry.ttest[5],0);
	     
	     packet_append_data(rpacket,&entry,sizeof(entry));
	     
	     if (account)
	       {
		  packet_append_string(rpacket,(tname = account_get_name(account)));
		  account_unget_name(tname);
	       }
	     else
	       packet_append_string(rpacket," "); /* use a space so the client won't show the user's own account when double-clicked on */
	  }
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_laddersearchreq(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_laddersearchreq)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LADDERSEARCHREQ packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_laddersearchreq),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * playername;
	t_account *  account;
	unsigned int idnum;
	unsigned int rank; /* starts at zero */
	t_ladder_id  id;
	
	idnum = bn_int_get(packet->u.client_laddersearchreq.id);
	
	switch (idnum)
	  {
	   case CLIENT_LADDERREQ_ID_STANDARD:
	     id = ladder_id_normal;
	     break;
	   case CLIENT_LADDERREQ_ID_IRONMAN:
	     id = ladder_id_ironman;
	     break;
	   default:
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got unknown ladder laddersearchreq.id=0x%08x",conn_get_socket(c),idnum);
	     id = ladder_id_normal;
	  }
	
	if (!(playername = packet_get_str_const(packet,sizeof(t_client_laddersearchreq),USER_NAME_MAX)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad LADDERSEARCHREQ packet (missing or too long playername)",conn_get_socket(c));
	     return -1;
	  }
	
	if (!(account = accountlist_find_account(playername)))
	  rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
	else
	  {
	     switch (bn_int_get(packet->u.client_laddersearchreq.type))
	       {
		case CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED:
		  if ((rank = ladder_get_rank_by_account(account,ladder_sort_highestrated,ladder_time_active,conn_get_clienttag(c),id))==0)
		    {
		       rank = ladder_get_rank_by_account(account,ladder_sort_highestrated,ladder_time_current,conn_get_clienttag(c),id);
		       if (ladder_get_account_by_rank(rank,ladder_sort_highestrated,ladder_time_active,conn_get_clienttag(c),id))
			 rank = 0;
		    }
		  break;
		case CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS:
		  if ((rank = ladder_get_rank_by_account(account,ladder_sort_mostwins,ladder_time_active,conn_get_clienttag(c),id))==0)
		    {
		       rank = ladder_get_rank_by_account(account,ladder_sort_mostwins,ladder_time_current,conn_get_clienttag(c),id);
		       if (ladder_get_account_by_rank(rank,ladder_sort_mostwins,ladder_time_active,conn_get_clienttag(c),id))
			 rank = 0;
		    }
		  break;
		case CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES:
		  if ((rank = ladder_get_rank_by_account(account,ladder_sort_mostgames,ladder_time_active,conn_get_clienttag(c),id))==0)
		    {
		       rank = ladder_get_rank_by_account(account,ladder_sort_mostgames,ladder_time_current,conn_get_clienttag(c),id);
		       if (ladder_get_account_by_rank(rank,ladder_sort_mostgames,ladder_time_active,conn_get_clienttag(c),id))
			 rank = 0;
		    }
		  break;
		default:
		  rank = 0;
		  eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got unknown ladder search type %u",conn_get_socket(c),bn_int_get(packet->u.client_laddersearchreq.type));
	       }
	     
	     if (rank==0)
	       rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
	     else
	       rank--;
	  }
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	  return -1;
	packet_set_size(rpacket,sizeof(t_server_laddersearchreply));
	packet_set_type(rpacket,SERVER_LADDERSEARCHREPLY);
	bn_int_set(&rpacket->u.server_laddersearchreply.rank,rank);
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
     }
   
   return 0;
}

static int _client_mapauthreq1(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;
   
   if (packet_get_size(packet)<sizeof(t_client_mapauthreq1)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MAPAUTHREQ1 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_mapauthreq1),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * mapname;
	t_game *     game;
	
	if (!(mapname = packet_get_str_const(packet,sizeof(t_client_mapauthreq1),MAP_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MAPAUTHREQ1 packet (missing or too long mapname)",conn_get_socket(c));
	     return -1;
	  }
	
#ifndef WITH_BITS
	game = conn_get_game(c);
#else
	game = bits_game_create_temp(conn_get_bits_game(c));
	/* Hope this works --- Typhoon */
#endif
	
	if (game)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] map auth requested for map \"%s\" gametype \"%s\"",conn_get_socket(c),mapname,game_type_get_str(game_get_type(game)));
	     game_set_mapname(game,mapname);
	  }
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] map auth requested when not in a game",conn_get_socket(c));
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     unsigned int val;
	     
	     if (!game)
	       {
		  val = SERVER_MAPAUTHREPLY1_NO;
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] map authorization denied (not in a game)",conn_get_socket(c));
	       }
	     else
	       if (strcasecmp(game_get_mapname(game),mapname)!=0)
		 {
		    val = SERVER_MAPAUTHREPLY1_NO;
		    eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] map authorization denied (map name \"%s\" does not match game map name \"%s\")",conn_get_socket(c),mapname,game_get_mapname(game));
		 }
	     else
	       {
		  game_set_status(game,game_status_started);
		  
		  if (game_get_type(game)==game_type_ladder)
		    {
		       val = SERVER_MAPAUTHREPLY1_LADDER_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map ladder authorization (in a ladder game)",conn_get_socket(c));
		    }
		  else if (ladder_check_map(game_get_mapname(game),game_get_maptype(game),conn_get_clienttag(c)))
		    {
		       val = SERVER_MAPAUTHREPLY1_LADDER_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map ladder authorization (is a ladder map)",conn_get_socket(c));
		    }
		  else
		    {
		       val = SERVER_MAPAUTHREPLY1_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map normal authorization",conn_get_socket(c));
		    }
	       }
	     
	     packet_set_size(rpacket,sizeof(t_server_mapauthreply1));
	     packet_set_type(rpacket,SERVER_MAPAUTHREPLY1);
	     bn_int_set(&rpacket->u.server_mapauthreply1.response,val);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
#ifdef WITH_BITS
	if (game) bits_game_destroy_temp(game);
#endif		
     }
   
   return 0;
}

static int _client_mapauthreq2(t_connection * c, t_packet const * const packet)
{
   t_packet * rpacket = NULL;

   if (packet_get_size(packet)<sizeof(t_client_mapauthreq2)) {
      eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MAPAUTHREQ2 packet (expected %u bytes, got %u)",conn_get_socket(c),sizeof(t_client_mapauthreq2),packet_get_size(packet));
      return -1;
   }
   
     {
	char const * mapname;
	t_game *     game;
	
	if (!(mapname = packet_get_str_const(packet,sizeof(t_client_mapauthreq2),MAP_NAME_LEN)))
	  {
	     eventlog(eventlog_level_error,"handle_bnet_packet","[%d] got bad MAPAUTHREQ2 packet (missing or too long mapname)",conn_get_socket(c));
	     return -1;
	  }
	
#ifndef WITH_BITS
	game = conn_get_game(c);
#else
	game = bits_game_create_temp(conn_get_bits_game(c));
	/* Hope this works --- Typhoon */
#endif
	
	if (game)
	  {
	     eventlog(eventlog_level_info,"handle_bnet_packet","[%d] map auth requested for map \"%s\" gametype \"%s\"",conn_get_socket(c),mapname,game_type_get_str(game_get_type(game)));
	     game_set_mapname(game,mapname);
	  }
	else
	  eventlog(eventlog_level_info,"handle_bnet_packet","[%d] map auth requested when not in a game",conn_get_socket(c));
	
	if ((rpacket = packet_create(packet_class_bnet)))
	  {
	     unsigned int val;
	     
	     if (!game)
	       {
		  val = SERVER_MAPAUTHREPLY2_NO;
		  eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] map authorization denied (not in a game)",conn_get_socket(c));
	       }
	     else
	       if (strcasecmp(game_get_mapname(game),mapname)!=0)
		 {
		    val = SERVER_MAPAUTHREPLY2_NO;
		    eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] map authorization denied (map name \"%s\" does not match game map name \"%s\")",conn_get_socket(c),mapname,game_get_mapname(game));
		 }
	     else
	       {
		  /* MODIFIED BY UNDYING SOULZZ 4/14/02 */
		  /*			    game_set_status(game,game_status_started); */
		  if ( strcmp( CLIENTTAG_WARCRAFT3, conn_get_clienttag(c) ) == 0)
		    game_set_status(game,game_status_open );
		  else
		    game_set_status(game,game_status_started );
		  
		  if (game_get_type(game)==game_type_ladder)
		    {
		       val = SERVER_MAPAUTHREPLY2_LADDER_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map ladder authorization (in a ladder game)",conn_get_socket(c));
		    }
		  else if (ladder_check_map(game_get_mapname(game),game_get_maptype(game),conn_get_clienttag(c)))
		    {
		       val = SERVER_MAPAUTHREPLY2_LADDER_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map ladder authorization (is a ladder map)",conn_get_socket(c));
		    }
		  else
		    {
		       val = SERVER_MAPAUTHREPLY2_OK;
		       eventlog(eventlog_level_debug,"handle_bnet_packet","[%d] giving map normal authorization",conn_get_socket(c));
		    }
	       }
	     
	     packet_set_size(rpacket,sizeof(t_server_mapauthreply2));
	     packet_set_type(rpacket,SERVER_MAPAUTHREPLY2);
	     bn_int_set(&rpacket->u.server_mapauthreply2.response,val);
	     queue_push_packet(conn_get_out_queue(c),rpacket);
	     packet_del_ref(rpacket);
	  }
#ifdef WITH_BITS
	if (game) bits_game_destroy_temp(game);
#endif		
     }
   
   return 0;
}

