/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
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
#define CONNECTION_INTERNAL_ACCESS
#include "common/setup_before.h"
#include <stdio.h>
// amadeo
#ifdef WIN32_GUI
#include <win32/winmain.h>
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#include "compat/strtoul.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strchr.h"
#include "compat/strrchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include <errno.h>
#include "compat/strerror.h"
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include "compat/difftime.h"
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include "compat/socket.h"
#include "compat/psock.h"
#include "common/eventlog.h"
#include "common/addr.h"
#include "account.h"
#include "realm.h"
#include "channel.h"
#include "game.h"
#include "common/queue.h"
#include "tick.h"
#include "common/packet.h"
#include "common/tag.h"
#include "common/bn_type.h"
#include "message.h"
#include "common/version.h"
#include "prefs.h"
#include "common/util.h"
#include "common/list.h"
#include "watch.h"
#include "timer.h"
#include "irc.h"
#include "ipban.h"
#include "game_conv.h"
#include "udptest_send.h"
#include "character.h"
#include "versioncheck.h"
#include "common/bnet_protocol.h"
#include "common/field_sizes.h"
#include "anongame.h"
#include "clan.h"
#include "connection.h"
#include "topic.h"
#include "common/fdwatch.h"
#include "clienttag.h"
#include "common/setup_after.h"


static int      totalcount=0;
static t_list * conn_head=NULL;
static t_list * conn_dead=NULL;

static void conn_send_welcome(t_connection * c);
static void conn_send_issue(t_connection * c);


static void conn_send_welcome(t_connection * c)
{
    char const * filename;
    FILE *       fp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
	return;
    }
    
    if (c->protocol.cflags & conn_flags_welcomed)
	return;
    if (conn_get_class(c)==conn_class_irc)
    {
	c->protocol.cflags|= conn_flags_welcomed;
	return;
    }
    if ((filename = prefs_get_motdfile()))
    {
	if ((fp = fopen(filename,"r")))
	{
	    message_send_file(c,fp);
	    if (fclose(fp)<0)
	      { eventlog(eventlog_level_error,__FUNCTION__,"could not close MOTD file \"%s\" after reading (fopen: %s)",filename,strerror(errno)); }
	}
	else
	  { eventlog(eventlog_level_error,__FUNCTION__,"could not open MOTD file \"%s\" for reading (fopen: %s)",filename,strerror(errno)); }
    }
    c->protocol.cflags|= conn_flags_welcomed;
}


static void conn_send_issue(t_connection * c)
{
    char const * filename;
    FILE *       fp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_send_issue","got NULL connection");
	return;
    }
    
    if ((filename = prefs_get_issuefile()))
	if ((fp = fopen(filename,"r")))
	{
	    message_send_file(c,fp);
	    if (fclose(fp)<0)
		eventlog(eventlog_level_error,"conn_send_issue","could not close issue file \"%s\" after reading (fopen: %s)",filename,strerror(errno));
	}
	else
	    eventlog(eventlog_level_error,"conn_send_issue","could not open issue file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
    else
	eventlog(eventlog_level_debug,"conn_send_issue","no issue file");
}

// [zap-zero] 20020629
extern void conn_shutdown(t_connection * c, time_t now, t_timer_data foo)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_shutdown","got NULL connection");
        return;
    }

    if (now==(time_t)0) /* zero means user logged out before expiration */
    {
	eventlog(eventlog_level_trace,"conn_shutdown","[%d] connection allready closed",conn_get_socket(c));
	return;
    }

    eventlog(eventlog_level_trace,"conn_shutdown","[%d] closing connection",conn_get_socket(c));

    conn_set_state(c, conn_state_destroy);
}

extern void conn_test_latency(t_connection * c, time_t now, t_timer_data delta)
{
    t_packet * packet;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_test_latency","got NULL connection");
        return;
    }


    if (now==(time_t)0) /* zero means user logged out before expiration */
	return;

    if (conn_get_state(c)==conn_state_destroy)	// [zap-zero] 20020910
    	return;					// state_destroy: do nothing

    
    if (conn_get_class(c)==conn_class_irc) {
    	/* We should start pinging the client after we received the first line ... */
    	/* NOTE: RFC2812 only suggests that PINGs are being sent 
    	 * if no other activity is detected. However it explecitly 
    	 * allows PINGs to be sent if there is activity on this 
    	 * connection. In other words we just don't care :)
    	 */
	if (conn_get_ircping(c)!=0) {
	    eventlog(eventlog_level_warn,"conn_test_latency","[%d] ping timeout (closing connection)",conn_get_socket(c));
	    conn_set_latency(c,0); 
	    conn_set_state(c,conn_state_destroy);
	}
	irc_send_ping(c);
    } else if(conn_get_class(c)==conn_class_w3route) {
        if(!(packet = packet_create(packet_class_w3route))) {
	    eventlog(eventlog_level_error,"handle_w3route_packet","[%d] packet_create failed",conn_get_socket(c));
	} else {
    	    packet_set_size(packet,sizeof(t_server_w3route_echoreq));
	    packet_set_type(packet,SERVER_W3ROUTE_ECHOREQ);
	    bn_int_set(&packet->u.server_w3route_echoreq.ticks,get_ticks());
	    conn_push_outqueue(c, packet);
	    packet_del_ref(packet);
	}
    } else {

    	/* FIXME: I think real Battle.net sends these even before login */
    	if (!conn_get_game(c))
	{
            if ((packet = packet_create(packet_class_bnet)))
	    {
	    	packet_set_size(packet,sizeof(t_server_echoreq));
	    	packet_set_type(packet,SERVER_ECHOREQ);
	    	bn_int_set(&packet->u.server_echoreq.ticks,get_ticks());
	    	conn_push_outqueue(c,packet);
	    	packet_del_ref(packet);
	    }
	    else
	      { eventlog(eventlog_level_error,"conn_test_latency","could not create packet"); }
	}
    }
    
    if (timerlist_add_timer(c,now+(time_t)delta.n,conn_test_latency,delta)<0)
	eventlog(eventlog_level_error,"conn_test_latency","could not add timer");
}


static void conn_send_nullmsg(t_connection * c, time_t now, t_timer_data delta)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_send_nullmsg","got NULL connection");
        return;
    }
   
    if (now==(time_t)0) /* zero means user logged out before expiration */
        return;
   
    message_send_text(c,message_type_null,c,NULL);
    
    if (timerlist_add_timer(c,now+(time_t)delta.n,conn_send_nullmsg,delta)<0)
        eventlog(eventlog_level_error,"conn_send_nullmsg","could not add timer");
}


extern char const * conn_class_get_str(t_conn_class class)
{
    switch (class)
    {
    case conn_class_init:
	return "init";
    case conn_class_defer: /* either bnet or auth... wait to see */
	return "defer";
    case conn_class_bnet:
	return "bnet";
    case conn_class_file:
	return "file";
    case conn_class_bot:
	return "bot";
    case conn_class_d2cs_bnetd:
	return "d2cs_bnetd";
    case conn_class_auth:
	return "auth";
    case conn_class_telnet:
	return "telnet";
    case conn_class_irc:
	return "irc";
    case conn_class_none:
	return "none";
	case conn_class_w3route:
	return "w3route";
    default:
	return "UNKNOWN";
    }
}


extern char const * conn_state_get_str(t_conn_state state)
{
    switch (state)
    {
    case conn_state_empty:
	return "empty";
    case conn_state_initial:
	return "initial";
    case conn_state_connected:
	return "connected";
    case conn_state_bot_username:
	return "bot_username";
    case conn_state_bot_password:
	return "bot_password";
    case conn_state_loggedin:
	return "loggedin";
    case conn_state_destroy:
	return "destroy";
    case conn_state_untrusted:
	return "untrusted";
    case conn_state_pending_raw:
	return "pending_raw";
    default:
	return "UNKNOWN";
    }
}


extern t_connection * conn_create(int tsock, int usock, unsigned int real_local_addr, unsigned short real_local_port, unsigned int local_addr, unsigned short local_port, unsigned int addr, unsigned short port)
{
    static unsigned int sessionnum;
    t_connection * temp;
	
    
    if (tsock<0)
    {
        eventlog(eventlog_level_error,"conn_create","got bad TCP socket %d",tsock);
        return NULL;
    }
    if (usock<-1) /* -1 is allowed for some connection classes like bot, irc, and telnet */
    {
        eventlog(eventlog_level_error,"conn_create","got bad UDP socket %d",usock);
        return NULL;
    }
    
    if (!(temp = malloc(sizeof(t_connection))))
    {
        eventlog(eventlog_level_error,"conn_create","could not allocate memory for temp");
	return NULL;
    }
    
    temp->socket.tcp_sock               = tsock;
    temp->socket.tcp_addr               = addr;
    temp->socket.tcp_port               = port;
    temp->socket.udp_sock               = usock;
    temp->socket.udp_addr               = addr; /* same for now but client can request it to be different */
    temp->socket.local_addr             = local_addr;
    temp->socket.local_port             = local_port;
    temp->socket.real_local_addr        = real_local_addr;
    temp->socket.real_local_port        = real_local_port;
    temp->socket.udp_port               = port;
    temp->protocol.class                = conn_class_init;
    temp->protocol.state                = conn_state_initial;
    temp->protocol.sessionkey           = ((unsigned int)rand())^((unsigned int)time(NULL)+(unsigned int)real_local_port);
    temp->protocol.sessionnum           = sessionnum++;
    temp->protocol.secret               = ((unsigned int)rand())^(totalcount+((unsigned int)time(NULL)));
    temp->protocol.flags                = MF_PLUG;
    temp->protocol.latency              = 0;
    temp->protocol.chat.dnd                      = NULL;
    temp->protocol.chat.away                     = NULL;
    temp->protocol.chat.ignore_list              = NULL;
    temp->protocol.chat.ignore_count             = 0;
    temp->protocol.chat.quota.totcount           = 0;
    if (!(temp->protocol.chat.quota.list = list_create()))
    {
	free(temp);
        eventlog(eventlog_level_error,"conn_create","could not create quota list");
	return NULL;
    }
    temp->protocol.client.versionid              = 0;
    temp->protocol.client.gameversion            = 0;
    temp->protocol.client.checksum               = 0;
    temp->protocol.client.archtag                = NULL;
    temp->protocol.client.clienttag              = 0;
    temp->protocol.client.clientver              = NULL;
    temp->protocol.client.gamelang               = 0;
    temp->protocol.client.country                = NULL;
    temp->protocol.client.tzbias                 = 0;
    temp->protocol.client.host                   = NULL;
    temp->protocol.client.user                   = NULL;
    temp->protocol.client.clientexe              = NULL;
    temp->protocol.client.owner                  = NULL;
    temp->protocol.client.cdkey                  = NULL;
    temp->protocol.client.versioncheck           = NULL;
    temp->protocol.account                       = NULL;
    temp->protocol.chat.channel                  = NULL;
    temp->protocol.chat.last_message             = time(NULL);
    temp->protocol.chat.lastsender               = NULL;
    temp->protocol.chat.irc.ircline              = NULL;
    temp->protocol.chat.irc.ircping              = 0;
    temp->protocol.chat.irc.ircpass              = NULL;
    temp->protocol.chat.tmpOP_channel		 = NULL;
    temp->protocol.chat.tmpVOICE_channel	 = NULL;
    temp->protocol.game                     = NULL;
    temp->protocol.queues.outqueue               = NULL;
    temp->protocol.queues.outsize                = 0;
    temp->protocol.queues.outsizep               = 0;
    temp->protocol.queues.inqueue                = NULL;
    temp->protocol.queues.insize                 = 0;
    temp->protocol.loggeduser                    = NULL;
    temp->protocol.d2.realmname                  = NULL;
    temp->protocol.d2.character                  = NULL;
    temp->protocol.d2.realminfo                  = NULL;
    temp->protocol.d2.charname                   = NULL;
    temp->protocol.w3.w3_playerinfo              = NULL;
    temp->protocol.w3.routeconn                  = NULL;
    temp->protocol.w3.anongame                   = NULL;
    temp->protocol.w3.anongame_search_starttime  = 0;
    temp->protocol.bound                         = NULL;
    temp->protocol.cr_time                       = time(NULL);
    temp->protocol.passfail_count                = 0;


    temp->protocol.cflags                        = 0;
	
    if (list_prepend_data(conn_head,temp)<0)
    {
	list_destroy(temp->protocol.chat.quota.list);
	free(temp);
	eventlog(eventlog_level_error,"conn_create","could not prepend temp");
	return NULL;
    }
    
    eventlog(eventlog_level_info,"conn_create","[%d][%d] sessionkey=0x%08x sessionnum=0x%08x",temp->socket.tcp_sock,temp->socket.udp_sock,temp->protocol.sessionkey,temp->protocol.sessionnum);
    
    return temp;
}


extern t_anongame * conn_create_anongame(t_connection *c)
{
    t_anongame * temp;
    int i;

    if(c->protocol.w3.anongame) {
        eventlog(eventlog_level_error,"conn_create_anongame","anongame already allocated");
	return c->protocol.w3.anongame;
    }

    if (!(temp = malloc(sizeof(t_anongame))))
    {
        eventlog(eventlog_level_error,"conn_create_anongame","could not allocate memory for temp");
	return NULL;
    }

    temp->count		= 0;
    temp->id		= 0;
    temp->tid		= 0;
    
    for (i=0; i < ANONGAME_MAX_GAMECOUNT/2; i++)
	temp->tc[i]	= NULL;
    
    temp->race		= 0;
    temp->playernum	= 0;
    temp->handle	= 0;
    temp->addr		= 0;
    temp->loaded	= 0;
    temp->joined	= 0;
    temp->map_prefs	= 0xffffffff;
    temp->type		= 0;
    temp->gametype	= 0;
    temp->queue		= 0;
    temp->info		= NULL;

    c->protocol.w3.anongame = temp;

    return temp;
}

extern t_anongame * conn_get_anongame(t_connection *c)
{
    if(!c) {
	eventlog(eventlog_level_error,"conn_get_anongame","got NULL connection");
	return NULL;
    }
    return c->protocol.w3.anongame;
}

extern void conn_destroy_anongame(t_connection *c)
{
    t_anongame * a;

    if (!c)
    {
	eventlog(eventlog_level_error,"conn_destroy_anongame","got NULL connection");
	return;
    }

    if (!(a = c->protocol.w3.anongame))
    {
	eventlog(eventlog_level_error,"conn_destroy_anongame","NULL anongame");
	return;
    }

    // delete reference to this connection
    if(a->info) {
    	a->info->player[a->playernum-1] = NULL;
	if(--(a->info->currentplayers) == 0)
	    anongameinfo_destroy(a->info);
    }
	
	// [quetzal] 20020824 
    // unqueue from anongame search list,
	// if we got AT game, unqueue entire team.
	if (anongame_arranged(a->queue)) {
		anongame_unqueue(a->tc[0], a->queue);
	} else {
		anongame_unqueue(c, a->queue);
	}
    free(c->protocol.w3.anongame);
    c->protocol.w3.anongame = NULL;
}

extern void conn_destroy(t_connection * c)
{
    char const * classstr;
    
    
    if (c == NULL) {
	eventlog(eventlog_level_error, "conn_destroy", "got NULL connection");
	return;
    }

    classstr = conn_class_get_str(c->protocol.class);

    if (list_remove_data(conn_head,c)<0)
    {
	eventlog(eventlog_level_error,"conn_destroy","could not remove item from list");
	return;
    }

    if (c->protocol.class==conn_class_d2cs_bnetd)
    {
        t_realm * realm;

        realm=realmlist_find_realm_by_sock(conn_get_socket(c));
        if (realm)
             realm_deactive(realm);
        else
        {
             eventlog(eventlog_level_error,"conn_destroy","could not find realm for d2cs connection");
        }
    }
    else if (c->protocol.class == conn_class_w3route && c->protocol.w3.routeconn && c->protocol.w3.routeconn->protocol.w3.anongame)
    {
	anongame_stats(c);
	conn_destroy_anongame(c->protocol.w3.routeconn);  // [zap-zero] destroy anongame too when game connection is invalid
    }

    if (c->protocol.d2.realmname) {
        realm_add_player_number(realmlist_find_realm(conn_get_realmname(c)),-1);
    }

    
    /* free the memory with user quota */
    {
	t_qline * qline;
	t_elem *  curr;
	
	LIST_TRAVERSE(c->protocol.chat.quota.list,curr)
	{
	    qline = elem_get_data(curr);
	    free(qline);
	    list_remove_elem(c->protocol.chat.quota.list,curr);
	}
	list_destroy(c->protocol.chat.quota.list);
    }

    /* if this user in a channel, notify everyone that the user has left */
    if (c->protocol.chat.channel)
	channel_del_connection(c->protocol.chat.channel,c);

   if ((c->protocol.game) && (c->protocol.account))
   {
   	if (game_get_status(c->protocol.game)==game_status_started)
	{
            game_set_self_report(c->protocol.game,c->protocol.account,game_result_disconnect);
            game_set_report(c->protocol.game,c->protocol.account,"disconnect","disconnect");
	}
   }

    conn_set_game(c,NULL,NULL,NULL,game_type_none,0);
    c->protocol.state = conn_state_empty;

    watchlist_del_all_events(c);
    timerlist_del_all_timers(c);
    
    clanmember_set_offline(c);

    if(c->protocol.account)
	watchlist_notify_event(c->protocol.account,NULL,c->protocol.client.clienttag,watch_event_logout);

    if (c->protocol.client.versioncheck)
	versioncheck_destroy((void *)c->protocol.client.versioncheck); /* avoid warning */

    if (c->protocol.chat.lastsender)
	free((void *)c->protocol.chat.lastsender); /* avoid warning */
    
    if (c->protocol.chat.away)
	free((void *)c->protocol.chat.away); /* avoid warning */
    if (c->protocol.chat.dnd)
	free((void *)c->protocol.chat.dnd); /* avoid warning */
    if (c->protocol.chat.tmpOP_channel)
	free((void *)c->protocol.chat.tmpOP_channel); /* avoid warning */
    if (c->protocol.chat.tmpVOICE_channel)
	free((void *)c->protocol.chat.tmpVOICE_channel); /* avoid warning */
    
    if (c->protocol.client.archtag)
	free((void *)c->protocol.client.archtag); /* avoid warning */
    if (c->protocol.client.clientver)
	free((void *)c->protocol.client.clientver); /* avoid warning */
    if (c->protocol.client.country)
	free((void *)c->protocol.client.country); /* avoid warning */
    if (c->protocol.client.host)
	free((void *)c->protocol.client.host); /* avoid warning */
    if (c->protocol.client.user)
	free((void *)c->protocol.client.user); /* avoid warning */
    if (c->protocol.client.clientexe)
	free((void *)c->protocol.client.clientexe); /* avoid warning */
    if (c->protocol.client.owner)
	free((void *)c->protocol.client.owner); /* avoid warning */
    if (c->protocol.client.cdkey)
	free((void *)c->protocol.client.cdkey); /* avoid warning */
    if (c->protocol.loggeduser)
	free((void *)c->protocol.loggeduser); /* avoid warning */
    if (c->protocol.d2.realmname)
	free((void *)c->protocol.d2.realmname); /* avoid warning */
    if (c->protocol.d2.realminfo)
	free((void *)c->protocol.d2.realminfo); /* avoid warning */
    if (c->protocol.d2.charname)
	free((void *)c->protocol.d2.charname); /* avoid warning */
    if (c->protocol.chat.irc.ircline)
	free((void *)c->protocol.chat.irc.ircline); /* avoid warning */
    if (c->protocol.chat.irc.ircpass)
	free((void *)c->protocol.chat.irc.ircpass); /* avoid warning */

    /* ADDED BY UNDYING SOULZZ 4/8/02 */
    if (c->protocol.w3.w3_playerinfo)
	free((void *)c->protocol.w3.w3_playerinfo); /* avoid warning */ 

    if (c->protocol.bound)
	c->protocol.bound->protocol.bound = NULL;
    
    if (c->protocol.chat.ignore_count>0)
    {
	if (!c->protocol.chat.ignore_list)
	  { eventlog(eventlog_level_error,"conn_destroy","found NULL ignore_list with ignore_count=%u",c->protocol.chat.ignore_count); }
	else
	  { free(c->protocol.chat.ignore_list); }
    }

    if (c->protocol.account)
    {
	char const * tname;
	
	tname = account_get_name(c->protocol.account);
	eventlog(eventlog_level_info,"conn_destroy","[%d] \"%s\" logged out",c->socket.tcp_sock,tname);
	account_unget_name(tname);
	//amadeo
#ifdef WIN32_GUI
						guiOnUpdateUserList();
#endif

	if (account_get_conn(c->protocol.account)==c)  /* make sure you don't set this when allready on new conn (relogin with same account) */
	    account_set_conn(c->protocol.account,NULL);
	c->protocol.account = NULL; /* the account code will free the memory later */
    }

    /* make sure the connection is closed */
    if (c->socket.tcp_sock!=-1) { /* -1 means that the socket was already closed by conn_close() */
	fdwatch_del_fd(c->socket.tcp_sock);
	psock_shutdown(c->socket.tcp_sock,PSOCK_SHUT_RDWR);
	psock_close(c->socket.tcp_sock);
    }
    /* clear out the packet queues */
    queue_clear(&c->protocol.queues.inqueue);
    queue_clear(&c->protocol.queues.outqueue);

    // [zap-zero] 20020601
    if (c->protocol.w3.routeconn) {
	c->protocol.w3.routeconn->protocol.w3.routeconn = NULL;
	if(c->protocol.w3.routeconn->protocol.class == conn_class_w3route)
	    conn_set_state(c->protocol.w3.routeconn, conn_state_destroy);
    }

    if(c->protocol.w3.anongame)
	conn_destroy_anongame(c);

    /* delete the conn from the dead list if its there, we dont check for error
     * because connections may be destroyed without first setting state to destroy */
    if (conn_dead) list_remove_data(conn_dead, c);

    eventlog(eventlog_level_info,"conn_destroy","[%d] closed %s connection",c->socket.tcp_sock,classstr);
    
    free(c);
}


extern int conn_match(t_connection const * c, char const * username)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_match","got NULL connection");
        return -1;
    }
    if (!username)
    {
        eventlog(eventlog_level_error,"conn_match","got NULL username");
        return -1;
    }
    
    if (!c->protocol.account)
	return 0;
    
    return account_match(c->protocol.account,username);
}


extern t_conn_class conn_get_class(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_class","got NULL connection");
        return conn_class_none;
    }
    
    return c->protocol.class;
}


extern void conn_set_class(t_connection * c, t_conn_class class)
{
    t_timer_data  data;
    unsigned long delta;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_class","got NULL connection");
        return;
    }
    
    if (c->protocol.class==class)
	return;
    
    c->protocol.class = class;
    
    if (class==conn_class_bnet)
    {
	
	if (prefs_get_udptest_port()!=0)
	    conn_set_game_port(c,(unsigned short)prefs_get_udptest_port());
	udptest_send(c);
	
	delta = prefs_get_latency();
	data.n = delta;
	if (timerlist_add_timer(c,time(NULL)+(time_t)delta,conn_test_latency,data)<0)
	    eventlog(eventlog_level_error,"conn_set_class","could not add timer");

        eventlog(eventlog_level_debug,"conn_set_class","added latency check timer");

    }
    // [zap-zero] 20020629
    else if (class==conn_class_w3route)
    {
	delta = prefs_get_latency();
	data.n = delta;
	if (timerlist_add_timer(c,time(NULL)+(time_t)delta,conn_test_latency,data)<0)
	    eventlog(eventlog_level_error,"conn_set_class","could not add timer");
    }
    else if (class==conn_class_bot || class==conn_class_telnet)
    {
	t_packet * rpacket;
	
	if (class==conn_class_bot)
	{
	    if ((delta = prefs_get_nullmsg())>0)
	    {
		data.n = delta;
		if (timerlist_add_timer(c,time(NULL)+(time_t)delta,conn_send_nullmsg,data)<0)
		    eventlog(eventlog_level_error,"conn_set_class","could not add timer");
	    }
	}
	
	conn_send_issue(c);
	
	if (!(rpacket = packet_create(packet_class_raw)))
	    eventlog(eventlog_level_error,"conn_set_class","could not create rpacket");
	else
	{
	    packet_append_ntstring(rpacket,"Username: ");
	    conn_push_outqueue(c,rpacket);
	    packet_del_ref(rpacket);
	}
    }
}


extern t_conn_state conn_get_state(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_state","got NULL connection");
        return conn_state_empty;
    }
    
    return c->protocol.state;
}


extern void conn_set_state(t_connection * c, t_conn_state state)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return;
    }

    /* special case for destroying connections, add them to conn_dead list */
    if (state == conn_state_destroy && c->protocol.state != conn_state_destroy) {
	if (!conn_dead && !(conn_dead = list_create())) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not initilize conn_dead list");
	    return;
	}
	list_append_data(conn_dead, c);
    }
    else if (state != conn_state_destroy && c->protocol.state == conn_state_destroy)
	if (list_remove_data(conn_dead, c)) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not remove dead connection");
	    return;
	}

    c->protocol.state = state;
}

extern unsigned int conn_get_sessionkey(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_sessionkey","got NULL connection");
        return 0;
    }
    
    return c->protocol.sessionkey;
}


extern unsigned int conn_get_sessionnum(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_sessionnum","got NULL connection");
        return 0;
    }

    return c->protocol.sessionnum;
}


extern unsigned int conn_get_secret(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_secret","got NULL connection");
        return 0;
    }

    return c->protocol.secret;
}


extern unsigned int conn_get_addr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_addr","got NULL connection");
        return 0;
    }
    
    return c->socket.tcp_addr;
}


extern unsigned short conn_get_port(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_port","got NULL connection");
        return 0;
    }
    
    return c->socket.tcp_port;
}


extern unsigned int conn_get_local_addr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_local_addr","got NULL connection");
        return 0;
    }
    
    return c->socket.local_addr;
}


extern unsigned short conn_get_local_port(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_local_port","got NULL connection");
        return 0;
    }
    
    return c->socket.local_port;
}


extern unsigned int conn_get_real_local_addr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_real_local_addr","got NULL connection");
        return 0;
    }
    
    return c->socket.real_local_addr;
}


extern unsigned short conn_get_real_local_port(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_real_local_port","got NULL connection");
        return 0;
    }
    
    return c->socket.real_local_port;
}


extern unsigned int conn_get_game_addr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_game_addr","got NULL connection");
        return 0;
    }
    
    return c->socket.udp_addr;
}


extern int conn_set_game_addr(t_connection * c, unsigned int game_addr)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_game_addr","got NULL connection");
        return -1;
    }
    
    c->socket.udp_addr = game_addr;
    return 0;
}


extern unsigned short conn_get_game_port(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_game_port","got NULL connection");
        return 0;
    }
    
    return c->socket.udp_port;
}


extern int conn_set_game_port(t_connection * c, unsigned short game_port)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_game_port","got NULL connection");
        return -1;
    }
    
    c->socket.udp_port = game_port;
    return 0;
}


extern void conn_set_host(t_connection * c, char const * host)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_host","got NULL connection");
        return;
    }
    if (!host)
    {
        eventlog(eventlog_level_error,"conn_set_host","got NULL host");
        return;
    }
    
    if (c->protocol.client.host)
	free((void *)c->protocol.client.host); /* avoid warning */
    if (!(c->protocol.client.host = strdup(host)))
	eventlog(eventlog_level_error,"conn_set_host","could not allocate memory for c->protocol.client.host");
}


extern void conn_set_user(t_connection * c, char const * user)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_user","got NULL connection");
        return;
    }
    if (!user)
    {
        eventlog(eventlog_level_error,"conn_set_user","got NULL user");
        return;
    }

    if (c->protocol.client.user)
	free((void *)c->protocol.client.user); /* avoid warning */
    if (!(c->protocol.client.user = strdup(user)))
	eventlog(eventlog_level_error,"conn_set_user","could not allocate memory for c->protocol.client.user");
}


extern void conn_set_owner(t_connection * c, char const * owner)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_owner","got NULL connection");
        return;
    }
    if (!owner)
    {
        eventlog(eventlog_level_error,"conn_set_owner","got NULL owner");
        return;
    }
    
    if (c->protocol.client.owner)
	free((void *)c->protocol.client.owner); /* avoid warning */
    if (!(c->protocol.client.owner = strdup(owner)))
	eventlog(eventlog_level_error,"conn_set_owner","could not allocate memory for c->protocol.client.owner");
}

extern const char * conn_get_user(t_connection const * c)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_get_user","got NULL connection");
	return NULL;
    }
    return c->protocol.client.user;
}

extern const char * conn_get_owner(t_connection const * c)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_get_user","got NULL connection");
	return NULL;
    }
    return c->protocol.client.owner;
}

extern void conn_set_cdkey(t_connection * c, char const * cdkey)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_cdkey","got NULL connection");
        return;
    }
    if (!cdkey)
    {
        eventlog(eventlog_level_error,"conn_set_cdkey","got NULL cdkey");
        return;
    }

    if (c->protocol.client.cdkey)
	free((void *)c->protocol.client.cdkey); /* avoid warning */
    if (!(c->protocol.client.cdkey = strdup(cdkey)))
	eventlog(eventlog_level_error,"conn_set_cdkey","could not allocate memory for c->protocol.client.cdkey");
}


extern char const * conn_get_clientexe(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_clientexe","got NULL connection");
        return NULL;
    }

    if (!c->protocol.client.clientexe)
	return "";
    return c->protocol.client.clientexe;
}


extern void conn_set_clientexe(t_connection * c, char const * clientexe)
{
    char const * temp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_clientexe","got NULL connection");
        return;
    }
    if (!clientexe)
    {
        eventlog(eventlog_level_error,"conn_set_clientexe","got NULL clientexe");
        return;
    }
    
    if (!(temp = strdup(clientexe)))
    {
	eventlog(eventlog_level_error,"conn_set_clientexe","unable to allocate memory for clientexe");
	return;
    }
    if (c->protocol.client.clientexe)
	free((void *)c->protocol.client.clientexe); /* avoid warning */
    c->protocol.client.clientexe = temp;
}


extern char const * conn_get_clientver(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_clientver","got NULL connection");
        return NULL;
    }

    if (!c->protocol.client.clientver)
	return "";
    return c->protocol.client.clientver;
}


extern void conn_set_clientver(t_connection * c, char const * clientver)
{
    char const * temp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_clientver","got NULL connection");
        return;
    }
    if (!clientver)
    {
        eventlog(eventlog_level_error,"conn_set_clientver","got NULL clientver");
        return;
    }
    
    if (!(temp = strdup(clientver)))
    {
	eventlog(eventlog_level_error,"conn_set_clientver","unable to allocate memory for clientver");
	return;
    }
    if (c->protocol.client.clientver)
	free((void *)c->protocol.client.clientver); /* avoid warning */
    c->protocol.client.clientver = temp;
}


extern char const * conn_get_archtag(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_archtag","got NULL connection");
        return NULL;
    }
    
    if (c->protocol.class==conn_class_auth && c->protocol.bound)
    {
	if (!c->protocol.bound->protocol.client.archtag)
	    return "UKWN";
	return c->protocol.bound->protocol.client.archtag;
    }
    if (!c->protocol.client.archtag)
	return "UKWN";
    return c->protocol.client.archtag;
}


extern void conn_set_archtag(t_connection * c, char const * archtag)
{
    char const * temp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_archtag","got NULL connection");
        return;
    }
    if (!archtag)
    {
        eventlog(eventlog_level_error,"conn_set_archtag","[%d] got NULL archtag",conn_get_socket(c));
        return;
    }
    if (strlen(archtag)!=4)
    {
        eventlog(eventlog_level_error,"conn_set_archtag","[%d] got bad archtag",conn_get_socket(c));
        return;
    }
    
    if (!c->protocol.client.archtag || strcmp(c->protocol.client.archtag,archtag)!=0)
	eventlog(eventlog_level_info,"conn_set_archtag","[%d] setting client arch to \"%s\"",conn_get_socket(c),archtag);
    
    if (!(temp = strdup(archtag)))
    {
	eventlog(eventlog_level_error,"conn_set_archtag","[%d] unable to allocate memory for archtag",conn_get_socket(c));
	return;
    }
    if (c->protocol.client.archtag)
	free((void *)c->protocol.client.archtag); /* avoid warning */
    c->protocol.client.archtag = temp;
}


extern unsigned int conn_get_gamelang(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__,"got NULL connection");
        return 0;
    }
    
    return c->protocol.client.gamelang;
}


extern void conn_set_gamelang(t_connection * c, unsigned int gamelang)
{
    if (!c)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
        return;
    }

    c->protocol.client.gamelang = gamelang;
}


extern t_clienttag conn_get_clienttag(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_clienttag","got NULL connection");
        return CLIENTTAG_UNKNOWN_UINT;
    }
    
    if (c->protocol.class==conn_class_auth && c->protocol.bound)
    {
	if (!c->protocol.bound->protocol.client.clienttag)
	    return CLIENTTAG_UNKNOWN_UINT;
	return c->protocol.bound->protocol.client.clienttag;
    }
    if (!c->protocol.client.clienttag)
	return CLIENTTAG_UNKNOWN_UINT;
    return c->protocol.client.clienttag;
}


extern char const * conn_get_fake_clienttag(t_connection const * c)
{
    char const * clienttag;
    t_account *  account;

    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_fake_clienttag","got NULL connection");
        return NULL;
    }

    account = conn_get_account(c);
    if (account) /* BITS remote connections don't need to have an account */
	if ((clienttag = account_get_strattr(account,"BNET\\fakeclienttag")))
	    return clienttag;
    return clienttag_uint_to_str(c->protocol.client.clienttag);
}


extern void conn_set_clienttag(t_connection * c, t_clienttag clienttag)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_clienttag","got NULL connection");
        return;
    }
    
    if (c->protocol.client.clienttag!=clienttag)
    {
	eventlog(eventlog_level_info,"conn_set_clienttag","[%d] setting client type to \"%s\"",conn_get_socket(c),clienttag_uint_to_str(clienttag));
        c->protocol.client.clienttag = clienttag;
        if (c->protocol.chat.channel)
	    channel_update_flags(c);

    }
    
}


extern unsigned long conn_get_gameversion(t_connection const * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL connection");
	return 0;
    }

    return c->protocol.client.gameversion;
}


extern int conn_set_gameversion(t_connection * c, unsigned long gameversion)
{
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL connection");
	return -1;
    }
    
    c->protocol.client.gameversion = gameversion;
    return 0;
}


extern unsigned long conn_get_checksum(t_connection const * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL connection");
	return 0;
    }

    return c->protocol.client.checksum;
}


extern int conn_set_checksum(t_connection * c, unsigned long checksum)
{
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__,"got NULL connection");
	return -1;
    }
    
    c->protocol.client.checksum = checksum;
    return 0;
}


extern unsigned long conn_get_versionid(t_connection const * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"conn_get_versionid","got NULL connection");
	return 0;
    }

    return c->protocol.client.versionid;
}


extern int conn_set_versionid(t_connection * c, unsigned long versionid)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"conn_set_versionid","got NULL connection");
	return -1;
    }

    c->protocol.client.versionid = versionid;
    return 0;
}


extern int conn_get_tzbias(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_tzbias","got NULL connection");
        return 0;
    }

    return c->protocol.client.tzbias;
}


extern void conn_set_tzbias(t_connection * c, int tzbias)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_tzbias","got NULL connection");
        return;
    }
    
    c->protocol.client.tzbias = tzbias;
}


extern void conn_set_account(t_connection * c, t_account * account)
{
    t_connection * other;
    unsigned int   now;
    char const *   tname;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_account","got NULL connection");
        return;
    }
    if (!account)
    {
        eventlog(eventlog_level_error,"conn_set_account","got NULL account");
        return;
    }
    now = (unsigned int)time(NULL);
    
    if ((other = connlist_find_connection_by_accountname((tname = account_get_name(account)))))
    {
	eventlog(eventlog_level_info,"conn_set_account","[%d] forcing logout of previous login for \"%s\"",conn_get_socket(c),tname);
	conn_set_state(other, conn_state_destroy);
    }
    account_unget_name(tname);
    
    c->protocol.account = account;
    c->protocol.state = conn_state_loggedin;
    account_set_conn(account,c);
    {
	char const * flagstr;
	
	if ((flagstr = account_get_strattr(account,"BNET\\flags\\initial")))
	    conn_add_flags(c,strtoul(flagstr,NULL,0));
    }
    
    account_set_ll_time(c->protocol.account,now);
    account_set_ll_owner(c->protocol.account,c->protocol.client.owner);
    account_set_ll_clienttag(c->protocol.account,clienttag_uint_to_str(c->protocol.client.clienttag));
    account_set_ll_ip(c->protocol.account,addr_num_to_ip_str(c->socket.tcp_addr));
      
    if (c->protocol.client.host)
    {
	free((void *)c->protocol.client.host); /* avoid warning */
	c->protocol.client.host = NULL;
    }
    if (c->protocol.client.user)
    {
	free((void *)c->protocol.client.user); /* avoid warning */
	c->protocol.client.user = NULL;
    }
    if (c->protocol.client.clientexe)
    {
	free((void *)c->protocol.client.clientexe); /* avoid warning */
	c->protocol.client.clientexe = NULL;
    }
    if (c->protocol.client.owner)
    {
	free((void *)c->protocol.client.owner); /* avoid warning */
	c->protocol.client.owner = NULL;
    }
    if (c->protocol.client.cdkey)
    {
	free((void *)c->protocol.client.cdkey); /* avoid warning */
	c->protocol.client.cdkey = NULL;
    }
    
    if (c->protocol.loggeduser)
    {
	free((void *)c->protocol.loggeduser); /* avoid warning */
	c->protocol.loggeduser = NULL;
    }
    
    clanmember_set_online(c);

    totalcount++;
    
    watchlist_notify_event(c->protocol.account,NULL,c->protocol.client.clienttag,watch_event_login);
    
    return;
}


extern t_account * conn_get_account(t_connection const * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"conn_get_account","got NULL connection");
	return NULL;
    }
    
    if (c->protocol.class==conn_class_auth && c->protocol.bound)
	return c->protocol.bound->protocol.account;
    return c->protocol.account;
}


extern int conn_set_loggeduser(t_connection * c, char const * username)
{
    char const * temp;
    
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
	return -1;
    }
    if (!username)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL username");
	return -1;
    }
    
    if (!(temp = strdup(username)))
    {
	eventlog(eventlog_level_error, __FUNCTION__,"unable to duplicate username");
	return -1;
    }
    if (c->protocol.loggeduser)
	free((void *)c->protocol.loggeduser); /* avoid warning */
    
    c->protocol.loggeduser = temp;
    
    return 0;
}


extern char const * conn_get_loggeduser(t_connection const * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
	return NULL;
    }
    
    return c->protocol.loggeduser;
}


extern unsigned int conn_get_flags(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_flags","got NULL connection");
        return 0;
    }

    return c->protocol.flags;
}


extern int conn_set_flags(t_connection * c, unsigned int flags)
{
    unsigned int oldflags;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_flags","got NULL connection");
        return -1;
    }
    oldflags = c->protocol.flags;
    c->protocol.flags = flags;

    if (oldflags!=c->protocol.flags && c->protocol.chat.channel)
	channel_update_flags(c);
    
    return 0;
}


extern void conn_add_flags(t_connection * c, unsigned int flags)
{
    unsigned int oldflags;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_add_flags","got NULL connection");
        return;
    }
    oldflags = c->protocol.flags;
    c->protocol.flags |= flags;
    
    if (oldflags!=c->protocol.flags && c->protocol.chat.channel)
	channel_update_flags(c);
}


extern void conn_del_flags(t_connection * c, unsigned int flags)
{
    unsigned int oldflags;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_del_flags","got NULL connection");
        return;
    }
    oldflags = c->protocol.flags;
    c->protocol.flags &= ~flags;
    
    if (oldflags!=c->protocol.flags && c->protocol.chat.channel)
	channel_update_flags(c);
}


extern unsigned int conn_get_latency(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_latency","got NULL connection");
        return 0;
    }

    return c->protocol.latency;
}


extern void conn_set_latency(t_connection * c, unsigned int ms)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"conn_set_latency","got NULL connection");
	return;
    }

    c->protocol.latency = ms;

    if (c->protocol.chat.channel)
	channel_update_latency(c);
}


extern char const * conn_get_awaystr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_awaystr","got NULL connection");
        return NULL;
    }

    return c->protocol.chat.away;
}


extern int conn_set_awaystr(t_connection * c, char const * away)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_awaystr","got NULL connection");
        return -1;
    }
    
    if (c->protocol.chat.away)
	free((void *)c->protocol.chat.away); /* avoid warning */
    if (!away)
        c->protocol.chat.away = NULL;
    else
        if (!(c->protocol.chat.away = strdup(away)))
	{
	    eventlog(eventlog_level_error,"conn_set_awaystr","could not allocate away string");
	    return -1;
	}
    
    return 0;
}


extern char const * conn_get_dndstr(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_dndstr","got NULL connection");
        return NULL;
    }
    
    return c->protocol.chat.dnd;
}


extern int conn_set_dndstr(t_connection * c, char const * dnd)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_dndstr","got NULL connection");
        return -1;
    }
    
    if (c->protocol.chat.dnd)
	free((void *)c->protocol.chat.dnd); /* avoid warning */
    if (!dnd)
        c->protocol.chat.dnd = NULL;
    else
        if (!(c->protocol.chat.dnd = strdup(dnd)))
	{
	    eventlog(eventlog_level_error,"conn_set_dndstr","could not allocate dnd string");
	    return -1;
	}
    
    return 0;
}


extern int conn_add_ignore(t_connection * c, t_account * account)
{
    t_account * * newlist;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_add_ignore","got NULL connection");
        return -1;
    }
    if (!account)
    {
        eventlog(eventlog_level_error,"conn_add_ignore","got NULL account");
        return -1;
    }
    
    if (!(newlist = realloc(c->protocol.chat.ignore_list,sizeof(t_account const *)*(c->protocol.chat.ignore_count+1))))
    {
	eventlog(eventlog_level_error,"conn_add_ignore","could not allocate memory for newlist");
	return -1;
    }
    
    newlist[c->protocol.chat.ignore_count++] = account;
    c->protocol.chat.ignore_list = newlist;
    
    return 0;
}


extern int conn_del_ignore(t_connection * c, t_account const * account)
{
    t_account * * newlist;
    t_account *   temp;
    unsigned int  i;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_del_ignore","got NULL connection");
        return -1;
    }
    if (!account)
    {
        eventlog(eventlog_level_error,"conn_del_ignore","got NULL account");
        return -1;
    }
    
    for (i=0; i<c->protocol.chat.ignore_count; i++)
	if (c->protocol.chat.ignore_list[i]==account)
	    break;
    if (i==c->protocol.chat.ignore_count)
	return -1; /* not in list */
    
    /* swap entry to be deleted with last entry */
    temp = c->protocol.chat.ignore_list[c->protocol.chat.ignore_count-1];
    c->protocol.chat.ignore_list[c->protocol.chat.ignore_count-1] = c->protocol.chat.ignore_list[i];
    c->protocol.chat.ignore_list[i] = temp;
    
    if (c->protocol.chat.ignore_count==1) /* some realloc()s are buggy */
    {
	free(c->protocol.chat.ignore_list);
	newlist = NULL;
    }
    else
	newlist = realloc(c->protocol.chat.ignore_list,sizeof(t_account const *)*(c->protocol.chat.ignore_count-1));
    
    c->protocol.chat.ignore_count--;
    c->protocol.chat.ignore_list = newlist;
    
    return 0;
}


extern int conn_add_watch(t_connection * c, t_account * account, char const * clienttag)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_add_watch","got NULL connection");
        return -1;
    }
    
    if (watchlist_add_events(c,account,clienttag,watch_event_login|watch_event_logout|watch_event_joingame|watch_event_leavegame)<0)
	return -1;
    return 0;
}


extern int conn_del_watch(t_connection * c, t_account * account, char const * clienttag)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_del_watch","got NULL connection");
        return -1;
    }
    
    if (watchlist_del_events(c,account,clienttag,watch_event_login|watch_event_logout|watch_event_joingame|watch_event_leavegame)<0)
	return -1;
    return 0;
}


extern int conn_check_ignoring(t_connection const * c, char const * me)
{
    unsigned int i;
    t_account *  temp;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_check_ignoring","got NULL connection");
        return -1;
    }
    
    if (!me || !(temp = accountlist_find_account(me)))
	return -1;
    
    if (c->protocol.chat.ignore_list)
	for (i=0; i<c->protocol.chat.ignore_count; i++)
	    if (c->protocol.chat.ignore_list[i]==temp)
		return 1;
    
    return 0;
}


extern t_channel * conn_get_channel(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_channel","got NULL connection");
        return NULL;
    }
    
    return c->protocol.chat.channel;
}


extern int conn_set_channel_var(t_connection * c, t_channel * channel)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_channel_var","got NULL connection");
        return -1;
    }
    c->protocol.chat.channel = channel;
    return 0;
}


extern int conn_set_channel(t_connection * c, char const * channelname)
{
    t_channel * channel;
    t_channel * oldchannel;
    t_account * acc;
    int clantag=0;
    t_clan * clan = NULL;

    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_channel","got NULL connection");
        return -1;
    }

    acc = c->protocol.account;

    if (!acc)
    {
        eventlog(eventlog_level_error,"conn_set_channel","got NULL account");
        return -1;
    }

    if (channelname)
    {
	unsigned int created;

    oldchannel=c->protocol.chat.channel;

	channel = channellist_find_channel_by_name(channelname,conn_get_country(c),conn_get_realmname(c));

	if(channel && (channel == oldchannel))
		return 0;

	if((strncasecmp(channelname, "clan ", 5)==0)&&(strlen(channelname)<10))
		clantag = str_to_clantag(&channelname[5]);

    if(clantag && ((!(clan = account_get_clan(acc))) || (clan_get_clantag(clan) != clantag)))
    {
        if (!channel)
        {
            char msgtemp[MAX_MESSAGE_LEN];
            sprintf(msgtemp, "Unable to join channel %s, there is no member of that clan in the channel!", channelname);
            message_send_text(c, message_type_error, c, msgtemp);
            return 0;
        }
        else
        {
            t_clan * ch_clan;
            if((ch_clan=clanlist_find_clan_by_clantag(clantag))&&(clan_get_channel_type(ch_clan)==1))
            {
                message_send_text(c, message_type_error, c, "This is a private clan channel, unable to join!");
                return 0;
            }
        }
    }

    if (c->protocol.chat.channel)
    {
	channel_del_connection(c->protocol.chat.channel, c);
	c->protocol.chat.channel = NULL;
    }

    if (channel)
	{
	    if (channel_check_banning(channel,c))
	    {
		message_send_text(c,message_type_error,c,"You are banned from that channel.");
		return -1;
	    }

	    if ((account_get_auth_admin(acc,NULL)!=1) && (account_get_auth_admin(acc,channelname)!=1) &&
		(account_get_auth_operator(acc,NULL)!=1) && (account_get_auth_operator(acc,channelname)!=1) &&
		(channel_get_max(channel) == 0))
	    {
		message_send_text(c,message_type_error,c,"That channel is for Admins/Operators only.");
		return -1;
	    }

	    if ((account_get_auth_admin(acc,NULL)!=1) && (account_get_auth_admin(acc,channelname)!=1) &&
		(account_get_auth_operator(acc,NULL)!=1) && (account_get_auth_operator(acc,channelname)!=1) &&
		(channel_get_max(channel) != -1) && (channel_get_curr(channel)>=channel_get_max(channel)))
	    {
		message_send_text(c,message_type_error,c,"The channel is currently full.");
		return -1;
	    }
	}
    
    if(conn_set_joingamewhisper_ack(c,0)<0)
	eventlog(eventlog_level_error,"conn_set_channel","Unable to reset conn_set_joingamewhisper_ack flag");

    if(conn_set_leavegamewhisper_ack(c,0)<0)
	eventlog(eventlog_level_error,"conn_set_channel","Unable to reset conn_set_leavegamewhisper_ack flag");
    
	//if the last Arranged Team game was cancel, interupted, then we dont save the team at all.
	//then we reset the flag to 0

        if(account_get_new_at_team(acc)==1)
		{
			int temp;
			temp = account_get_atteamcount(acc,clienttag_uint_to_str(c->protocol.client.clienttag));
			temp = temp-1;
			account_set_atteamcount(acc,clienttag_uint_to_str(c->protocol.client.clienttag),temp);
			account_set_new_at_team(acc,0);
		}

	/* if you're entering a channel, make sure they didn't exit a game without telling us */
	if (c->protocol.game)
	{
            game_del_player(conn_get_game(c),c);
	    c->protocol.game = NULL;
	}

	created = 0;

    if (!channel)
	{
	    if(clantag)
		channel = channel_create(channelname,channelname,NULL,0,1,1,prefs_get_chanlog(), NULL, NULL, (prefs_get_maxusers_per_channel() > 0) ? prefs_get_maxusers_per_channel() : -1, 0, 1);
	    else
		channel = channel_create(channelname,channelname,NULL,0,1,1,prefs_get_chanlog(), NULL, NULL, (prefs_get_maxusers_per_channel() > 0) ? prefs_get_maxusers_per_channel() : -1, 0, 0);
	    if (!channel)
	    {
		eventlog(eventlog_level_error,"conn_set_channel","[%d] could not create channel on join \"%s\"",conn_get_socket(c),channelname);
		return -1;
	    }
	    created = 1;
	}

    c->protocol.chat.channel=channel;

    if (channel_add_connection(channel,c)<0)
	{
	    if (created)
		    channel_destroy(channel);
	c->protocol.chat.channel = NULL;
        return -1;
	}

	eventlog(eventlog_level_info,"conn_set_channel","[%d] joined channel \"%s\"",conn_get_socket(c),channel_get_name(c->protocol.chat.channel));
	conn_send_welcome(c);

    if(c->protocol.chat.channel && (channel_get_flags(c->protocol.chat.channel) & channel_flags_thevoid))
	    message_send_text(c,message_type_info,c,"This channel does not have chat privileges.");
    if (clantag && clan && (clan_get_clantag(clan)==clantag))
    {
      char msgtemp[MAX_MESSAGE_LEN];
      sprintf(msgtemp,"%s",clan_get_motd(clan));
      message_send_text(c,message_type_info,c,msgtemp);
    }

    if (channel_get_topic(channel_get_name(c->protocol.chat.channel)) && (conn_get_class(c)!=conn_class_irc))
    {
      char msgtemp[MAX_MESSAGE_LEN];

      sprintf(msgtemp,"%s topic: %s",channel_get_name(c->protocol.chat.channel),channel_get_topic(channel_get_name(c->protocol.chat.channel)));
      message_send_text(c,message_type_info,c,msgtemp);
    }

    if (c->protocol.chat.channel && (channel_get_flags(c->protocol.chat.channel) & channel_flags_moderated))
	message_send_text(c,message_type_error,c,"This channel is moderated.");

    if(c->protocol.chat.channel!=oldchannel)
      clanmember_on_change_status_by_connection(c);
    }
    else
    {
      if (c->protocol.chat.channel)
      {
        channel_del_connection(c->protocol.chat.channel,c);
        c->protocol.chat.channel = NULL;
      }
    }

    return 0;
}


extern t_game * conn_get_game(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_game","got NULL connection");
        return NULL;
    }
    
    return c->protocol.game;
}


extern int conn_set_game(t_connection * c, char const * gamename, char const * gamepass, char const * gameinfo, t_game_type type, int version)
/*
 * If game not exists (create) version != 0 (called in handle_bnet.c, function _client_startgameX())
 * If game exists (join) version == 0 always (called in handle_bnet.c, function _client_joingame())
 * If game exists (join) gameinfo == "" (called in handle_bnet.c, function _client_joingame())
 * [KWS]
 */
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_game","got NULL connection");
        return -1;
    }

    if (c->protocol.game)
    {
        if (gamename) {
	    if (strcasecmp(gamename,game_get_name(c->protocol.game)))
        	eventlog(eventlog_level_error,"conn_set_game","[%d] tried to join a new game \"%s\" while already in a game \"%s\"!",conn_get_socket(c),gamename,game_get_name(c->protocol.game));
	    else return 0;
	}
    	game_del_player(conn_get_game(c),c);
    	c->protocol.game = NULL;
    }
    if (gamename)
    {
	if (!(c->protocol.game = gamelist_find_game(gamename,type)))
	/* version from KWS - commented out to try if this made D2 problems
        { 
	    // setup a new game if it does not allready exist
	    if (!(c->protocol.game = game_create(gamename,gamepass,gameinfo,type,version,conn_get_clienttag(c))))
	       return -1;

	    game_parse_info(c->protocol.game,gameinfo); // only create
            if (conn_get_realmname(c) && conn_get_charname(c))
            {
                  game_set_realmname(c->protocol.game,conn_get_realmname(c));
                  realm_add_game_number(realmlist_find_realm(conn_get_realmname(c)),1);
	    }
	}

	// join new or existing game 
        if (game_add_player(conn_get_game(c),gamepass,version,c)<0)
        {
  	  c->protocol.game = NULL; // bad password or version # 
	  return -1;
	}
	*/
	// original code
	{
	  c->protocol.game = game_create(gamename,gamepass,gameinfo,type,version,clienttag_uint_to_str(c->protocol.client.clienttag),conn_get_gameversion(c));
	    if (c->protocol.game && conn_get_realmname(c) && conn_get_charname(c))
	    {
		game_set_realmname(c->protocol.game,conn_get_realmname(c));
		realm_add_game_number(realmlist_find_realm(conn_get_realmname(c)),1);
	    }
	}
	if (c->protocol.game)
	{
	  if (game_add_player(conn_get_game(c),gamepass,version,c)<0)
	  {
	    c->protocol.game = NULL; // bad password or version #
	    return -1;
	  }
	}
	// end of original code
    }
    else
	c->protocol.game = NULL;
    return 0;
}

extern unsigned int conn_get_tcpaddr(t_connection * c)
{
	if (!c)
	{
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
	return 0;
	}
	
	return c->socket.tcp_addr;
}


extern t_queue * * conn_get_in_queue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_in_queue","got NULL connection");
        return NULL;
    }

    return &c->protocol.queues.inqueue;
}


extern unsigned int conn_get_in_size(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_in_size","got NULL connection");
        return 0;
    }
    
    return c->protocol.queues.insize;
}


extern void conn_set_in_size(t_connection * c, unsigned int size)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_in_size","got NULL connection");
        return;
    }
    
    c->protocol.queues.insize = size;
}


extern t_queue * * conn_get_out_queue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_out_queue","got NULL connection");
        return NULL;
    }
    return &c->protocol.queues.outqueue;
}


extern unsigned int conn_get_out_size(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_out_size","got NULL connection");
        return -1;
    }
    return c->protocol.queues.outsize;
}


extern void conn_set_out_size(t_connection * c, unsigned int size)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_out_size","got NULL connection");
        return;
    }
    
    c->protocol.queues.outsize = size;
}

extern int conn_push_outqueue(t_connection * c, t_packet * packet)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return -1;
    }

    if (!packet)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
        return -1;
    }

    queue_push_packet((t_queue * *)&c->protocol.queues.outqueue, packet);
    if (!c->protocol.queues.outsizep++) fdwatch_update_fd(c->socket.tcp_sock, fdwatch_type_read | fdwatch_type_write);

    return 0;
}

extern t_packet * conn_peek_outqueue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return NULL;
    }

    return queue_peek_packet((t_queue const * const *)&c->protocol.queues.outqueue);
}

extern t_packet * conn_pull_outqueue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return NULL;
    }

    if (c->protocol.queues.outsizep) {
	if (!(--c->protocol.queues.outsizep)) fdwatch_update_fd(c->socket.tcp_sock, fdwatch_type_read);
	return queue_pull_packet((t_queue * *)&c->protocol.queues.outqueue);
    }

    return NULL;
}

extern int conn_push_inqueue(t_connection * c, t_packet * packet)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return -1;
    }

    if (!packet)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
        return -1;
    }

    queue_push_packet((t_queue * *)&c->protocol.queues.inqueue, packet);

    return 0;
}

extern t_packet * conn_peek_inqueue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return NULL;
    }

    return queue_peek_packet((t_queue const * const *)&c->protocol.queues.inqueue);
}

extern t_packet * conn_pull_inqueue(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return NULL;
    }

    return queue_pull_packet((t_queue * *)&c->protocol.queues.inqueue);
}

#ifdef DEBUG_ACCOUNT
extern char const * conn_get_username_real(t_connection const * c,char const * fn,unsigned int ln)
#else
extern char const * conn_get_username(t_connection const * c)
#endif
{
  char const * result;
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_username","got NULL connection");
        return NULL;
    }
    
    if (c->protocol.class==conn_class_auth && c->protocol.bound)
	return account_get_name(c->protocol.bound->protocol.account);
    if(!c->protocol.account)
    {
        eventlog(eventlog_level_error,"conn_get_username","got NULL account");
        return NULL;
    }
    result = account_get_name(c->protocol.account);
    if (result == NULL)
    { 
	eventlog(eventlog_level_error,__FUNCTION__,"returned previous error after being called by %s:%u",fn,ln);

    }
    return result;
}


extern void conn_unget_username(t_connection const * c, char const * name)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_unget_username","got NULL connection");
        return;
    }
    
    account_unget_name(name);
}


extern char const * conn_get_chatname(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_chatname","got NULL connection");
        return NULL;
    }

    if ((c->protocol.class==conn_class_auth || c->protocol.class==conn_class_bnet) && c->protocol.bound)
    {
	if (c->protocol.d2.character)
	    return character_get_name(c->protocol.d2.character);
	if (c->protocol.bound->protocol.d2.character)
	    return character_get_name(c->protocol.bound->protocol.d2.character);
        eventlog(eventlog_level_error,"conn_get_chatname","[%d] got connection class %s bound to class %d without a character",conn_get_socket(c),conn_class_get_str(c->protocol.class),c->protocol.bound->protocol.class);
    }
    if (!c->protocol.account)
	return NULL; /* no name yet */
    return account_get_name(c->protocol.account);
}


extern int conn_unget_chatname(t_connection const * c, char const * name)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_unget_chatname","got NULL connection");
        return -1;
    }
    
    if ((c->protocol.class==conn_class_auth || c->protocol.class==conn_class_bnet) && c->protocol.bound)
	return 0;
    return account_unget_name(name);
}


extern char const * conn_get_chatcharname(t_connection const * c, t_connection const * dst)
{
    char const * accname;
    char *       chatcharname;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_chatcharname","got NULL connection");
        return NULL;
    }

    if (!c->protocol.account)
        return NULL; /* no name yet */

    /* for D2 Users */
    accname = account_get_name(c->protocol.account);
    if (!accname)
        return NULL;

    if (dst && dst->protocol.d2.charname)
    {
	const char *mychar;

	if (c->protocol.d2.charname) mychar = c->protocol.d2.charname;
	else mychar = "";
    	if ((chatcharname = malloc(strlen(accname) + 2 + strlen(mychar))))
    	    sprintf(chatcharname, "%s*%s", mychar, accname);
    } else chatcharname = strdup(accname);

    account_unget_name(accname);
    return chatcharname;
}


extern int conn_unget_chatcharname(t_connection const * c, char const * name)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_unget_chatcharname","got NULL connection");
        return -1;
    }
    if (!name)
    {
	eventlog(eventlog_level_error,"conn_unget_chatcharname","got NULL name");
	return -1;
    }
    
    free((void *)name); /* avoid warning */
    return 0;
}


extern t_message_class conn_get_message_class(t_connection const * c, t_connection const * dst)
{
    if (dst && dst->protocol.d2.charname) /* message to D2 user must be char*account */
	return message_class_charjoin;

    return message_class_normal;
}


extern unsigned int conn_get_userid(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_userid","got NULL connection");
        return 0;
    }

    if(!c->protocol.account)
    {
        eventlog(eventlog_level_error,"conn_get_userid","got NULL account");
        return 0;
    }
    
    return account_get_uid(c->protocol.account);
}


extern int conn_get_socket(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_socket","got NULL connection");
        return -1;
    }
    
    return c->socket.tcp_sock;
}


extern int conn_get_game_socket(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_game_socket","got NULL connection");
        return -1;
    }
    
    return c->socket.udp_sock;
}


extern int conn_set_game_socket(t_connection * c, int usock)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_game_socket","got NULL connection");
        return -1;
    }
    
    c->socket.udp_sock = usock;
    return 0;
}


extern char const * conn_get_playerinfo(t_connection const * c)
{
    t_account *  account;
    static char  playerinfo[MAX_PLAYERINFO_STR];
    char const * clienttag;
    char         revtag[5];
        
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_playerinfo","got NULL connection");
        return NULL;
    }
    if (!(account = conn_get_account(c)))
    {
	eventlog(eventlog_level_error,"conn_get_playerinfo","connection has no account");
	return NULL;
    }
    
    if (!(clienttag = conn_get_fake_clienttag(c)))
    {
	eventlog(eventlog_level_error,"conn_get_playerinfo","connection has NULL fakeclienttag");
	return NULL;
    }
    strncpy(revtag,clienttag,5); revtag[4] = '\0';
    strreverse(revtag);
    
    if (strcmp(clienttag,CLIENTTAG_BNCHATBOT)==0)
    {
	strcpy(playerinfo,revtag); /* FIXME: what to return here? */
	// commented cause i'm sick of seeing this in the logs
	//eventlog(eventlog_level_debug,"conn_get_playerinfo","got CHAT clienttag, using best guess");
    }
    else if (strcmp(clienttag,CLIENTTAG_STARCRAFT)==0)
    {
        if (conn_get_versionid(c)<=0x000000c7)
	{
	  sprintf(playerinfo,"%s %u %u %u %u %u",
		  revtag,
		  account_get_ladder_rating(account,clienttag,ladder_id_normal),
		  account_get_ladder_rank(account,clienttag,ladder_id_normal),
		  account_get_normal_wins(account,clienttag),
		  0,0);
	}
	else
	{
	  sprintf(playerinfo,"%s %u %u %u %u %u %u %u %u %s",
		  revtag,
		  account_get_ladder_rating(account,clienttag,ladder_id_normal),
		  account_get_ladder_rank(account,clienttag,ladder_id_normal),
		  account_get_normal_wins(account,clienttag),
		  0,0,0,0,0,
		  revtag);
	}
    }
    else if (strcmp(clienttag,CLIENTTAG_BROODWARS)==0)
    {
        if (conn_get_versionid(c)<=0x000000c7)
	{
	  sprintf(playerinfo,"%s %u %u %u %u %u",
		  revtag,
		  account_get_ladder_rating(account,clienttag,ladder_id_normal),
		  account_get_ladder_rank(account,clienttag,ladder_id_normal),
		  account_get_normal_wins(account,clienttag),
		  0,0);
	}
	else
	{
	  sprintf(playerinfo,"%s %u %u %u %u %u %u %u %u %s",
		  revtag,
		  account_get_ladder_rating(account,clienttag,ladder_id_normal),
		  account_get_ladder_rank(account,clienttag,ladder_id_normal),
		  account_get_normal_wins(account,clienttag),
		  0,0,0,0,0,
		  revtag);
	}
    }
    else if (strcmp(clienttag,CLIENTTAG_SHAREWARE)==0)
    {
	sprintf(playerinfo,"%s %u %u %u %u %u",
		revtag,
		account_get_ladder_rating(account,clienttag,ladder_id_normal),
		account_get_ladder_rank(account,clienttag,ladder_id_normal),
		account_get_normal_wins(account,clienttag),
		0,0);
    }
    else if (strcmp(clienttag,CLIENTTAG_DIABLORTL)==0)
    {
	sprintf(playerinfo,"%s %u %u %u %u %u %u %u %u %u",
		revtag,
		account_get_normal_level(account,clienttag),
		account_get_normal_class(account,clienttag),
		account_get_normal_diablo_kills(account,clienttag),
		account_get_normal_strength(account,clienttag),
		account_get_normal_magic(account,clienttag),
		account_get_normal_dexterity(account,clienttag),
		account_get_normal_vitality(account,clienttag),
		account_get_normal_gold(account,clienttag),
		0);
    }
    else if (strcmp(clienttag,CLIENTTAG_DIABLOSHR)==0)
    {
	sprintf(playerinfo,"%s %u %u %u %u %u %u %u %u %u",
		revtag,
		account_get_normal_level(account,clienttag),
		account_get_normal_class(account,clienttag),
		account_get_normal_diablo_kills(account,clienttag),
		account_get_normal_strength(account,clienttag),
		account_get_normal_magic(account,clienttag),
		account_get_normal_dexterity(account,clienttag),
		account_get_normal_vitality(account,clienttag),
		account_get_normal_gold(account,clienttag),
		0);
    }
    else if (strcmp(clienttag,CLIENTTAG_WARCIIBNE)==0)
    {
	unsigned int a,b;
	
	a = account_get_ladder_rating(account,clienttag,ladder_id_normal);
	b = account_get_ladder_rating(account,clienttag,ladder_id_ironman);
	
	sprintf(playerinfo,"%s %u %u %u %u %u %u %u %u",
		revtag,
		a,
		account_get_ladder_rank(account,clienttag,ladder_id_normal),
		account_get_normal_wins(account,clienttag),
		0,
		0,
		(a>b) ? a : b,
		b,
		account_get_ladder_rank(account,clienttag,ladder_id_ironman));
    }
    else if ((strcmp(clienttag,CLIENTTAG_DIABLO2DV)==0) || (strcmp(clienttag,CLIENTTAG_DIABLO2XP)==0))
#if 0
    /* FIXME: Was this the old code? Can this stuff vanish? */
    /* Yes, this was the pre-d2close code, however I'm not sure the new code
     * takes care of all the cases. */
    {
	t_character * ch;
	
	if (c->protocol.d2.character)
	    ch = c->protocol.d2.character;
	else
	    if ((c->protocol.class==conn_class_auth || c->protocol.class==conn_class_bnet) && c->protocol.bound && c->protocol.bound->protocol.d2.character)
		ch = c->protocol.bound->protocol.d2.character;
	    else
		ch = NULL;
	    
	if (ch)
	    sprintf(playerinfo,"%s%s,%s,%s",
		    revtag,
		    character_get_realmname(ch),
		    character_get_name(ch),
		    character_get_playerinfo(ch));
	else
	    strcpy(playerinfo,revtag); /* open char */
    }
    else /* FIXME: this used to return the empty string... do some formats actually use that or not? */
	strcpy(playerinfo,revtag); /* best guess... */
   }
#endif
   {
       /* This sets portrait of character */
       if (!conn_get_realmname(c) || !conn_get_realminfo(c))
       {
           bn_int_tag_set((bn_int *)playerinfo,clienttag); /* FIXME: Is this attempting to reverse the tag?  This isn't really correct... why not use the revtag stuff like above or below? */
           playerinfo[strlen(clienttag)]='\0';
       }
       else
       {
           strcpy(playerinfo,conn_get_realminfo(c));
       }
    }
    else
        strcpy(playerinfo,revtag); /* open char */
        
    return playerinfo;
}


extern int conn_set_playerinfo(t_connection const * c, char const * playerinfo)
{
    char const * clienttag;
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_playerinfo","got NULL connection");
        return -1;
    }
    if (!playerinfo)
    {
	eventlog(eventlog_level_error,"conn_set_playerinfo","got NULL playerinfo");
	return -1;
    }
    clienttag = clienttag_uint_to_str(c->protocol.client.clienttag);
    
    if (strcmp(clienttag,CLIENTTAG_DIABLORTL)==0)
    {
	unsigned int level;
	unsigned int class;
	unsigned int diablo_kills;
	unsigned int strength;
	unsigned int magic;
	unsigned int dexterity;
	unsigned int vitality;
	unsigned int gold;
	
	if (sscanf(playerinfo,"LTRD %u %u %u %u %u %u %u %u %*u",
		   &level,
		   &class,
		   &diablo_kills,
		   &strength,
		   &magic,
		   &dexterity,
		   &vitality,
		   &gold)!=8)
	{
	    eventlog(eventlog_level_error,"conn_set_playerinfo","got bad playerinfo");
	    return -1;
	}
	
	account_set_normal_level(conn_get_account(c),clienttag,level);
	account_set_normal_class(conn_get_account(c),clienttag,class);
	account_set_normal_diablo_kills(conn_get_account(c),clienttag,diablo_kills);
	account_set_normal_strength(conn_get_account(c),clienttag,strength);
	account_set_normal_magic(conn_get_account(c),clienttag,magic);
	account_set_normal_dexterity(conn_get_account(c),clienttag,dexterity);
	account_set_normal_vitality(conn_get_account(c),clienttag,vitality);
	account_set_normal_gold(conn_get_account(c),clienttag,gold);
    }
    else if (strcmp(clienttag,CLIENTTAG_DIABLOSHR)==0)
    {
	unsigned int level;
	unsigned int class;
	unsigned int diablo_kills;
	unsigned int strength;
	unsigned int magic;
	unsigned int dexterity;
	unsigned int vitality;
	unsigned int gold;
	
	if (sscanf(playerinfo,"RHSD %u %u %u %u %u %u %u %u %*u",
		   &level,
		   &class,
		   &diablo_kills,
		   &strength,
		   &magic,
		   &dexterity,
		   &vitality,
		   &gold)!=8)
	{
	    eventlog(eventlog_level_error,"conn_set_playerinfo","got bad playerinfo");
	    return -1;
	}
	
	account_set_normal_level(conn_get_account(c),clienttag,level);
	account_set_normal_class(conn_get_account(c),clienttag,class);
	account_set_normal_diablo_kills(conn_get_account(c),clienttag,diablo_kills);
	account_set_normal_strength(conn_get_account(c),clienttag,strength);
	account_set_normal_magic(conn_get_account(c),clienttag,magic);
	account_set_normal_dexterity(conn_get_account(c),clienttag,dexterity);
	account_set_normal_vitality(conn_get_account(c),clienttag,vitality);
	account_set_normal_gold(conn_get_account(c),clienttag,gold);
    }
    else if (strcmp(clienttag,CLIENTTAG_DIABLO2DV)==0)
    {
	/* not much to do */ /* FIXME: get char name here? */
	eventlog(eventlog_level_trace,"conn_set_playerinfo","[%d] playerinfo request for client \"%s\" playerinfo=\"%s\"",conn_get_socket(c),clienttag,playerinfo);
    }
    else if (strcmp(clienttag,CLIENTTAG_DIABLO2XP)==0)
    {
        /* in playerinfo we get strings of the form "Realmname,charname" */
	eventlog(eventlog_level_trace,"conn_set_playerinfo","[%d] playerinfo request for client \"%s\" playerinfo=\"%s\"",conn_get_socket(c),clienttag,playerinfo);
    }
    else
    {
	eventlog(eventlog_level_warn,"conn_set_playerinfo","setting playerinfo for client \"%s\" not supported (playerinfo=\"%s\")",clienttag,playerinfo);
	return -1;
    }
    
    return 0;
}


extern char const * conn_get_realminfo(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_realminfo","got NULL connection");
        return NULL;
    }
    return c->protocol.d2.realminfo;
}


extern int conn_set_realminfo(t_connection * c, char const * realminfo)
{
    char const * temp;

    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_realminfo","got NULL connection");
        return -1;
    }
    
    if (realminfo)
    {
	if (!(temp = strdup(realminfo)))
	{
	    eventlog(eventlog_level_error,"conn_set_realminfo","could not allocate memory for new realminfo");
	    return -1;
	}
    }
    else
      temp = NULL;
    
    if (c->protocol.d2.realminfo) /* if it was set before, free it now */
	free((void *)c->protocol.d2.realminfo); /* avoid warning */
    c->protocol.d2.realminfo = temp;
    return 0;
}


extern char const * conn_get_charname(t_connection const * c)
{
    if (!c)
    {
       eventlog(eventlog_level_error,"conn_get_charname","got NULL connection");
       return NULL;
    }
    return c->protocol.d2.charname;
}


extern int conn_set_charname(t_connection * c, char const * charname)
{
    char const * temp;

    if (!c)
    {
       eventlog(eventlog_level_error,"conn_set_charname","got NULL connection");
       return -1;
    }
    
    if (charname)
    {
	if (!(temp = strdup(charname)))
	{
	    eventlog(eventlog_level_error,"conn_set_charname","could not allocate memory for new charname");
	    return -1;
	}
    }    
    else
	temp = charname;

    if (c->protocol.d2.charname) /* free it, if it was previously set */
       free((void *)c->protocol.d2.charname); /* avoid warning */
    c->protocol.d2.charname = temp;
    return 0;
}


extern int conn_set_idletime(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_idletime","got NULL connection");
        return -1;
    }
    
    c->protocol.chat.last_message = time(NULL);
    return 0;
}


extern unsigned int conn_get_idletime(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_idletime","got NULL connection");
        return 0;
    }
    
    return (unsigned int)difftime(time(NULL),c->protocol.chat.last_message);
}


extern char const * conn_get_realmname(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_realmname","got NULL connection");
        return NULL;
    }
    
    if (c->protocol.class==conn_class_auth && c->protocol.bound)
	return c->protocol.bound->protocol.d2.realmname;
    return c->protocol.d2.realmname;
}


extern int conn_set_realmname(t_connection * c, char const * realmname)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_realmname","got NULL connection");
        return -1;
    }
    
    if (c->protocol.d2.realmname)
	free((void *)c->protocol.d2.realmname); /* avoid warning */
    if (!realmname)
        c->protocol.d2.realmname = NULL;
    else
        if (!(c->protocol.d2.realmname = strdup(realmname)))
	{
	    eventlog(eventlog_level_error,"conn_set_realmname","could not allocate realmname string");
	    return -1;
	}
	else
	    eventlog(eventlog_level_debug,"conn_set_realmname","[%d] set to \"%s\"",conn_get_socket(c),realmname);
    
    return 0;
}


extern int conn_set_character(t_connection * c, t_character * character)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_character","got NULL connection");
        return -1;
    }
    if (!character)
    {
        eventlog(eventlog_level_error,"conn_set_character","got NULL character");
	return -1;
    }
    
    c->protocol.d2.character = character;
    
    return 0;
}


extern void conn_set_country(t_connection * c, char const * country)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_country","got NULL connection");
        return;
    }
    if (!country)
    {
        eventlog(eventlog_level_error,"conn_set_country","got NULL country");
        return;
    }

    if (c->protocol.client.country)
	free((void *)c->protocol.client.country); /* avoid warning */
    if (!(c->protocol.client.country = strdup(country)))
	eventlog(eventlog_level_error,"conn_set_country","could not allocate memory for c->protocol.client.country");
}


extern char const * conn_get_country(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_country","got NULL connection");
        return NULL;
    }
    
    return c->protocol.client.country;
}


extern int conn_bind(t_connection * c1, t_connection * c2)
{
    if (!c1)
    {
        eventlog(eventlog_level_error,"conn_bind","got NULL connection");
        return -1;
    }
    if (!c2)
    {
        eventlog(eventlog_level_error,"conn_bind","got NULL connection");
        return -1;
    }

    c1->protocol.bound = c2;
    c2->protocol.bound = c1;

    return 0;
}


extern int conn_set_ircline(t_connection * c, char const * line)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_set_ircline","got NULL connection");
	return -1;
    }
    if (!line) {
	eventlog(eventlog_level_error,"conn_set_ircline","got NULL line");
	return -1;
    }
    if (c->protocol.chat.irc.ircline)
    	free((void *)c->protocol.chat.irc.ircline); /* avoid warning */
    if (!(c->protocol.chat.irc.ircline = strdup(line)))
	eventlog(eventlog_level_error,"conn_set_ircline","could not allocate memory for c->protocol.chat.irc.ircline");
    return 0;
}


extern char const * conn_get_ircline(t_connection const * c)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_get_ircline","got NULL connection");
	return NULL;
    }
    return c->protocol.chat.irc.ircline;
}


extern int conn_set_ircpass(t_connection * c, char const * pass)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_set_ircpass","got NULL connection");
	return -1;
    }
    if (c->protocol.chat.irc.ircpass)
    	free((void *)c->protocol.chat.irc.ircpass); /* avoid warning */
    if (!pass)
    	c->protocol.chat.irc.ircpass = NULL;
    else
	if (!(c->protocol.chat.irc.ircpass = strdup(pass)))
	    eventlog(eventlog_level_error,"conn_set_ircpass","could not allocate memory for c->protocol.chat.irc.ircpass");
    return 0;
}


extern char const * conn_get_ircpass(t_connection const * c)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_get_ircpass","got NULL connection");
	return NULL;
    }
    return c->protocol.chat.irc.ircpass;
}


extern int conn_set_ircping(t_connection * c, unsigned int ping)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_set_ircping","got NULL connection");
	return -1;
    }
    c->protocol.chat.irc.ircping = ping;
    return 0;
}


extern unsigned int conn_get_ircping(t_connection const * c)
{
    if (!c) {
	eventlog(eventlog_level_error,"conn_get_ircping","got NULL connection");
	return 0;
    }
    return c->protocol.chat.irc.ircping;
}

// NonReal
extern int conn_get_welcomed(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_welcomed","got NULL connection");
        return 0;
    }

    return (c->protocol.cflags & conn_flags_welcomed);
}

// NonReal
extern void conn_set_welcomed(t_connection * c, int welcomed)
{
    
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_welcomed","got NULL connection");
        return;
    }
    c->protocol.cflags |= conn_flags_welcomed;
}

/* ADDED BY UNDYING SOULZZ 4/7/02 */
extern int conn_set_w3_playerinfo( t_connection * c, const char * w3_playerinfo )
{
    const char * temp;

    if (!c)
    {
        eventlog(eventlog_level_error,"conn_set_w3_playerinfo", "got NULL connection");
        return -1;
    }

    temp = strdup( w3_playerinfo );

    if ( c->protocol.w3.w3_playerinfo )
	free((void *)c->protocol.w3.w3_playerinfo);

    c->protocol.w3.w3_playerinfo = temp;

    return 1;
}

extern const char * conn_get_w3_playerinfo( t_connection * c )
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_w3_playerinfo","got NULL connection");
        return NULL;
    }
    return c->protocol.w3.w3_playerinfo; 
}


extern int conn_quota_exceeded(t_connection * con, char const * text)
{
    time_t    now;
    t_qline * qline;
    t_elem *  curr;
    int       needpurge;
    
    if (!prefs_get_quota()) return 0;
    if (strlen(text)>prefs_get_quota_maxline())
    {
	message_send_text(con,message_type_error,con,"Your line length quota has been exceeded!");
	return 1;
    }
    
    now = time(NULL);
    
    needpurge = 0;
    LIST_TRAVERSE(con->protocol.chat.quota.list,curr)
    {
	qline = elem_get_data(curr);
        if (now>=qline->inf+(time_t)prefs_get_quota_time())
	{
	    /* these lines are at least quota_time old */
	    list_remove_elem(con->protocol.chat.quota.list,curr);
	    if (qline->count>con->protocol.chat.quota.totcount)
		eventlog(eventlog_level_error,"conn_quota_exceeded","qline->count=%u but con->protocol.chat.quota.totcount=%u",qline->count,con->protocol.chat.quota.totcount);
	    con->protocol.chat.quota.totcount -= qline->count;
	    free(qline);
	    needpurge = 1;
	}
	else
	    break; /* old items are first, so we know nothing else will match */
    }
    if (needpurge)
	list_purge(con->protocol.chat.quota.list);
    
    if ((qline = malloc(sizeof(t_qline)))==NULL)
    {
	eventlog(eventlog_level_error,"conn_quota_exceeded","could not allocate qline");
	return 0;
    }
    qline->inf = now; /* set the moment */
    if (strlen(text)>prefs_get_quota_wrapline()) /* round up on the divide */
	qline->count = (strlen(text)+prefs_get_quota_wrapline()-1)/prefs_get_quota_wrapline();
    else
	qline->count = 1;
    
    if (list_append_data(con->protocol.chat.quota.list,qline)<0)
    {
	eventlog(eventlog_level_error,"conn_quota_exceeded","could not append to list");
	free(qline);
	return 0;
    }
    con->protocol.chat.quota.totcount += qline->count;
    
    if (con->protocol.chat.quota.totcount>=prefs_get_quota_lines())
    {
	message_send_text(con,message_type_error,con,"Your message quota has been exceeded!");
	if (con->protocol.chat.quota.totcount>=prefs_get_quota_dobae())
	{
	    /* kick out the dobae user for violation of the quota rule */
	    conn_set_state(con,conn_state_destroy);
	    if (con->protocol.chat.channel)
		channel_message_log(con->protocol.chat.channel,con,0,"DISCONNECTED FOR DOBAE ABUSE");
	    return 2;
	}
	return 1;
    }
    
    return 0;
}


extern int conn_set_lastsender(t_connection * c, char const * sender)
{
    if (!c)
    {
	eventlog(eventlog_level_error,"conn_set_lastsender","got NULL conection");
	return -1;
    }
    if (c->protocol.chat.lastsender)
	free((void *)c->protocol.chat.lastsender); /* avoid warning */
    if (!sender)
    {
	c->protocol.chat.lastsender = NULL;
	return 0;
    }
    if (!(c->protocol.chat.lastsender = strdup(sender)))
    {
	eventlog(eventlog_level_error,"conn_set_lastsender","could not allocate memory for c->protocol.chat.lastsender");
	return -1;
    }
    
    return 0;
}


extern char const * conn_get_lastsender(t_connection const * c)
{
    if (!c) 
    {
	eventlog(eventlog_level_error,"conn_get_lastsender","got NULL connection");
	return NULL;
    }
    return c->protocol.chat.lastsender;
}


extern t_versioncheck * conn_get_versioncheck(t_connection * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_versioncheck","got NULL connection");
        return NULL;
    }

    return c->protocol.client.versioncheck;
}


extern int conn_set_versioncheck(t_connection * c, t_versioncheck * versioncheck)
{
    if (!c) 
    {
	eventlog(eventlog_level_error,"conn_set_versioncheck","got NULL connection");
	return -1;
    }
    if (!versioncheck) 
    {
	eventlog(eventlog_level_error,"conn_set_versioncheck","got NULL versioncheck");
	return -1;
    }

    c->protocol.client.versioncheck = versioncheck;

    return 0;
}

extern int conn_get_echoback(t_connection * c)
{
	if (!c)
	{
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
	return 0;
	}

	return (c->protocol.cflags & conn_flags_echoback);
}

extern void conn_set_echoback(t_connection * c, int echoback)
{
	if (!c)
	{
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
	return;
	}
	if (echoback)
	  c->protocol.cflags |=  conn_flags_echoback;
	else
	  c->protocol.cflags &= ~conn_flags_echoback;	  
}

extern int conn_set_udpok(t_connection * c)
{
    if (!c)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
	return -1;
    }
    
    if (!(c->protocol.cflags & conn_flags_udpok))
    {
	c->protocol.cflags|= conn_flags_udpok;
	c->protocol.flags &= ~MF_PLUG;
    }
    
    return 0;
}


extern t_connection * conn_get_routeconn(t_connection const * c)
{
    if (!c)
    {
        eventlog(eventlog_level_error,"conn_get_routeconn","got NULL connection");
        return NULL;
    }

    return c->protocol.w3.routeconn;
}


extern int conn_set_routeconn(t_connection * c, t_connection * rc)
{
    if (!c) {
		eventlog(eventlog_level_error,"conn_set_routeconn","got NULL conection");
		return -1;
    }
	c->protocol.w3.routeconn = rc;
    
    return 0;
}

extern int conn_get_crtime(t_connection *c)
{
	if (!c) 
	{
		eventlog(eventlog_level_error, "conn_get_crtime", "got NULL connection");
		return -1;
	}
	return c->protocol.cr_time;
}

extern int conn_set_joingamewhisper_ack(t_connection * c, unsigned int value)
{
	if (!c) 
	{
		eventlog(eventlog_level_error,__FUNCTION__, "got NULL connection");
		return -1;
	}
	if (value)
		c->protocol.cflags |=  conn_flags_joingamewhisper;
	else
		c->protocol.cflags &= ~conn_flags_joingamewhisper;
	return 0;
}
extern int conn_get_joingamewhisper_ack(t_connection * c)
{
	if (!c) 
	{
		eventlog(eventlog_level_error,__FUNCTION__, "got NULL connection");
		return -1;
	}
	return (c->protocol.cflags & conn_flags_joingamewhisper);
}

extern int conn_set_leavegamewhisper_ack(t_connection * c, unsigned int value)
{
	if (!c) 
	{
		eventlog(eventlog_level_error,__FUNCTION__, "got NULL connection");
		return -1;
	}
	if (value)
		c->protocol.cflags |=  conn_flags_leavegamewhisper;
	else
		c->protocol.cflags &= ~conn_flags_leavegamewhisper;
	return 0;
}
extern int conn_get_leavegamewhisper_ack(t_connection * c)
{
	if (!c) 
	{
		eventlog(eventlog_level_error,__FUNCTION__, "got NULL connection");
		return -1;
	}
	return (c->protocol.cflags & conn_flags_leavegamewhisper);
}

extern int conn_set_anongame_search_starttime(t_connection * c, time_t t)
{
   if (c == NULL) {
      eventlog(eventlog_level_error, "conn_set_anongame_search_starttime", "got NULL connection");
      return -1;
   }
   c->protocol.w3.anongame_search_starttime = t;
   return 0;
}

extern time_t conn_get_anongame_search_starttime(t_connection * c)
{
	if (c == NULL) {
	  eventlog(eventlog_level_error, "conn_set_anongame_search_starttime", "got NULL connection");
      return ((time_t) 0);
    }
  return c->protocol.w3.anongame_search_starttime;
}


extern int conn_get_user_count_by_clienttag(t_clienttag ct)
{
   t_connection * conn;
   t_elem const * curr;
   int clienttagusers = 0;
   
   /* Get Number of Users for client tag specific */
   LIST_TRAVERSE_CONST(connlist(),curr)
     {
	conn = elem_get_data(curr);
	if (ct == conn->protocol.client.clienttag)
	  clienttagusers++;
     }

   return clienttagusers;
}

extern char const * conn_get_user_game_title(t_clienttag ct)
{
   switch (ct)
   {
      case CLIENTTAG_WAR3XP_UINT:
	return "Warcraft III Frozen Throne";
      case CLIENTTAG_WARCRAFT3_UINT:
	return "Warcraft III";
      case CLIENTTAG_DIABLO2XP_UINT:
     	return "Diablo II Lord of Destruction";
      case CLIENTTAG_DIABLO2DV_UINT:
     	return "Diablo II";
      case CLIENTTAG_STARJAPAN_UINT:
     	return "Starcraft (Japan)";
      case CLIENTTAG_WARCIIBNE_UINT:
     	return "Warcraft II";
      case CLIENTTAG_DIABLOSHR_UINT:
     	return "Diablo I (Shareware)";
      case CLIENTTAG_DIABLORTL_UINT:
     	return "Diablo I";
      case CLIENTTAG_SHAREWARE_UINT:
     	return "Starcraft (Shareware)";
      case CLIENTTAG_BROODWARS_UINT:
     	return "Starcraft: BroodWars";
      case CLIENTTAG_STARCRAFT_UINT:
     	return "Starcraft";
      case CLIENTTAG_BNCHATBOT_UINT:
     	return "Chat";
      default:
     	return "Unknown";
   }
}

//NO LONGER USED - WAS FOR WHEN WE DIDNT HAVE PASSWORDS AT LOGIN - THEUNDYING
//extern int conn_set_w3_isidentified( t_connection * c, int isidentified )
//{
//    if ( isidentified != 1 )
//	c->w3_isidentified = 0;
//    else
//	c->w3_isidentified = 1;
//
//    return c->w3_isidentified;
//}
//
//extern int conn_get_w3_isidentified( t_connection * c )
//{
//    return c->w3_isidentified;
//}

//extern void conn_w3_identtimeout( t_connection * c, time_t now, t_timer_data data )
//{
//    if ( conn_get_w3_isidentified(c) == 0 )
//    {
//	conn_set_state( c, conn_state_destroy );
//    }
//    return;
//}
/* END OF UNDYING SOULZZ MODS */

extern int connlist_create(void)
{
    if (!(conn_head = list_create()))
	return -1;
    return 0;
}

extern int connlist_destroy(void)
{
    if (conn_dead) list_destroy(conn_dead);
    conn_dead = NULL;
    /* FIXME: if called with active connection, connection are not freed */
    if (list_destroy(conn_head)<0)
	return -1;
    conn_head = NULL;
    return 0;
}

extern void connlist_reap(void)
{
    t_elem		*curr;
    t_connection	*c;

    if (!conn_dead || !conn_head) return;

    LIST_TRAVERSE(conn_dead, curr)
    {
	c = (t_connection *)elem_get_data(curr);

	if (!c)
	    eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in conn_dead list");
	else conn_destroy(c); /* also removes from conn_dead list and fdwatch */
    }
    list_purge(conn_dead);
    list_purge(conn_head); /* remove deleted elements from the connlist */
}

extern t_list * connlist(void)
{
    return conn_head;
}


extern t_connection * connlist_find_connection_by_accountname(char const * accountname)
{
    t_account * temp;
    
    if (!accountname)
    {
	eventlog(eventlog_level_error,"connlist_find_connection_by_accountname","got NULL accountname");
	return NULL;
    }
    
    if (!(temp = accountlist_find_account(accountname)))
	return NULL;
   
    return account_get_conn(temp);
}

extern t_connection * connlist_find_connection_by_account(t_account * account)
{
    if (!account) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL account");
	return NULL;
    }
    return account_get_conn(account);
}


extern t_connection * connlist_find_connection_by_sessionkey(unsigned int sessionkey)
{
    t_connection * c;
    t_elem const * curr;
    
    LIST_TRAVERSE_CONST(conn_head,curr)
    {
	c = elem_get_data(curr);
	if (c->protocol.sessionkey==sessionkey)
	    return c;
    }
    
    return NULL;
}


extern t_connection * connlist_find_connection_by_sessionnum(unsigned int sessionnum)
{
    t_connection * c;
    t_elem const * curr;
    
    LIST_TRAVERSE_CONST(conn_head,curr)
    {
	c = elem_get_data(curr);
	if (c->protocol.sessionnum==sessionnum)
	    return c;
    }
    
    return NULL;
}


extern t_connection * connlist_find_connection_by_socket(int socket)
{
    t_connection * c;
    t_elem const * curr;
    
    LIST_TRAVERSE_CONST(conn_head,curr)
    {
	c = elem_get_data(curr);
	if (c->socket.tcp_sock==socket)
	    return c;
    }
    
    return NULL;
}


extern t_connection * connlist_find_connection_by_name(char const * name, char const * realmname)
{
    char         charname[CHAR_NAME_LEN];
    char const * temp;
    
    if (!name)
    {
	eventlog(eventlog_level_error,"connlist_find_connection_by_name","got NULL name");
	return NULL;
    }
    if (name[0]=='\0')
    {
	eventlog(eventlog_level_error,"connlist_find_connection_by_name","got empty name");
	return NULL;
    }
    
    /* format: *username */
    if (name[0]=='*')
    {
        name++;
        return connlist_find_connection_by_accountname(name);
    }
    
    /* If is charname@otherrealm or ch@rname@realm */
    if ((temp=strrchr(name,'@'))) /* search from the right */
    {
	unsigned int n;
	
	n = temp - name;
	if (n>=CHAR_NAME_LEN)
	{
	    eventlog(eventlog_level_info,"connlist_find_connection_by_name","character name too long in \"%s\" (charname@otherrealm format)",name);
	    return NULL;
	}
	strncpy(charname,name,n);
	charname[n] = '\0';
	return connlist_find_connection_by_charname(name,temp + 1);
    }
    
    /* format: charname*username */
    if ((temp=strchr(name,'*')))
    {
	unsigned int n;
	
	n = temp - name;
	if (n>=CHAR_NAME_LEN)
	{
	    eventlog(eventlog_level_info,"connlist_find_connection_by_name","character name too long in \"%s\" (charname*username format)",name);
	    return NULL;
	}
	name = temp + 1;
	return connlist_find_connection_by_accountname(name);
    }
    
    /* format: charname (realm must be not NULL) */
    if (realmname)
	return connlist_find_connection_by_charname(name,realmname);
    
    /* format: Simple username, clients with no realm, like starcraft or d2 open,
     * the format is the same of charname but is matched if realmname is NULL */
    return connlist_find_connection_by_accountname(name);
}


extern t_connection * connlist_find_connection_by_charname(char const * charname, char const * realmname)
{
     t_connection    * c;
     t_elem const    * curr;

     if (!realmname) {
	eventlog(eventlog_level_error,"connlist_find_connection_by_charname","got NULL realmname");
     	return NULL;
     }
     LIST_TRAVERSE_CONST(conn_head, curr)
     {
        c = elem_get_data(curr);
        if (!c)
            continue;
        if (!c->protocol.d2.charname)
            continue;
        if (!c->protocol.d2.realmname)
            continue;
        if ((strcasecmp(c->protocol.d2.charname, charname)==0)&&(strcasecmp(c->protocol.d2.realmname,realmname)==0))
            return c;
     }
     return NULL;
}


extern t_connection * connlist_find_connection_by_uid(unsigned int uid)
{
    t_account * temp;
    
    if (!(temp = accountlist_find_account_by_uid(uid)))
    {
        return NULL;
    }
    return account_get_conn(temp);
}

extern int connlist_get_length(void)
{
    return list_get_length(conn_head);
}


extern unsigned int connlist_login_get_length(void)
{
    t_connection const * c;
    unsigned int         count;
    t_elem const *       curr;
    
    count = 0;
    LIST_TRAVERSE_CONST(conn_head,curr)
    {
	c = elem_get_data(curr);
	if ((c->protocol.state==conn_state_loggedin)&&
	    ((c->protocol.class==conn_class_bnet)||(c->protocol.class==conn_class_bot)||(c->protocol.class==conn_class_telnet)||(c->protocol.class==conn_class_irc)))
	    count++;
    }
    
    return count;
}


extern int connlist_total_logins(void)
{
    return totalcount;
}


extern unsigned int connlist_count_connections(unsigned int addr)
{
  t_connection * c;
  t_elem const * curr;
  unsigned int count;

  count = 0;

  LIST_TRAVERSE_CONST(conn_head,curr)
  {
	c = (t_connection *)elem_get_data(curr);
	if (c->socket.tcp_addr == addr)
	  count++;
  }

  return count;
}

extern int conn_update_w3_playerinfo(t_connection * c)
{
    t_account * 	account;
    char const * 	clienttag;
    t_clan * 		user_clan;
    int 		clantag=0;
    unsigned int	acctlevel;
    char		tempplayerinfo[40];
    char		raceicon; /* appeared in 1.03 */
    unsigned int	raceiconnumber;
    unsigned int    	wins;
    char const *	usericon;
    char 		clantag_str_tmp[5];
    const char * 	clantag_str = NULL;
    char        revtag[5];

    if (c == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
	return -1;
    }

    account = conn_get_account(c);

    if (account == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL account");
	return -1;
    }

    strncpy(revtag, conn_get_fake_clienttag(c),5); revtag[4] = '\0';
    strreverse(revtag);

    clienttag = clienttag_uint_to_str(c->protocol.client.clienttag);

    acctlevel = account_get_highestladderlevel(account,clienttag);
    account_get_raceicon(account, &raceicon, &raceiconnumber, &wins, clienttag);

    if((user_clan = account_get_clan(account)) != NULL)
	clantag = clan_get_clantag(user_clan);

    if(clantag) {
	sprintf(clantag_str_tmp, "%c%c%c%c", clantag&0xff, (clantag>>8)&0xff, (clantag>>16)&0xff, clantag>>24);
        clantag_str=clantag_str_tmp;
        while((* clantag_str) == 0) clantag_str++;
    }

    if(acctlevel == 0) {
	if(clantag)
	    sprintf(tempplayerinfo, "%s %s 0 %s", revtag, revtag, clantag_str);
	else
	    strcpy(tempplayerinfo, revtag);
	eventlog(eventlog_level_info,__FUNCTION__,"[%d] %s",conn_get_socket(c), revtag);
    } else {
	usericon = account_get_user_icon(account,clienttag);
	if (!usericon) {
    	    if(clantag)
		sprintf(tempplayerinfo, "%s %1u%c3W %u %s", revtag, raceiconnumber, raceicon, acctlevel, clantag_str); 
            else
		sprintf(tempplayerinfo, "%s %1u%c3W %u", revtag, raceiconnumber, raceicon, acctlevel); 
	    eventlog(eventlog_level_info,__FUNCTION__,"[%d] %s using generated icon [%1u%c3W]",conn_get_socket(c), revtag, raceiconnumber, raceicon);
	} else {
            if(clantag)
		sprintf(tempplayerinfo, "%s %s %u %s",revtag, usericon, acctlevel, clantag_str);
            else
		sprintf(tempplayerinfo, "%s %s %u",revtag, usericon, acctlevel);
	    eventlog(eventlog_level_info,__FUNCTION__,"[%d] %s using user-selected icon [%s]",conn_get_socket(c),revtag,usericon);
	}
    }

    conn_set_w3_playerinfo( c, tempplayerinfo ); 

    if(account_get_new_at_team(account) == 1) {
	int temp;
	temp = account_get_atteamcount(account,clienttag);
	temp = temp-1;
	account_set_atteamcount(account,clienttag,temp);
	account_set_new_at_team(account,0);
    }

    return 0;     
}


extern int conn_get_passfail_count (t_connection * c)
{
    if (!c) 
    {
        eventlog(eventlog_level_error, "conn_get_passfail_count", "got NULL connection");
        return -1;
    }
    return c->protocol.passfail_count;
}


extern int conn_set_passfail_count (t_connection * c, unsigned int n)
{
    if (c == NULL)
    {
	eventlog(eventlog_level_error, "conn_set_passfail_count", "got NULL connection");
	return -1;
    }
    c->protocol.passfail_count = n;
    return 0;
}


extern int conn_increment_passfail_count (t_connection * c)
{
    unsigned int count;
		
    if (prefs_get_passfail_count() > 0)
    {
	count = conn_get_passfail_count(c) + 1;
	if (count == prefs_get_passfail_count())
	{
	    ipbanlist_add(NULL, addr_num_to_ip_str(conn_get_addr(c)), time(NULL)+(time_t)prefs_get_passfail_bantime());
	    eventlog(eventlog_level_info,"conn_increment_passfail_count","[%d] failed password tries: %d (banned ip)",conn_get_socket(c), count);
	    conn_set_state(c, conn_state_destroy);
	    return -1;
	}
	else conn_set_passfail_count(c, count);
    }
    return 0;
}

extern int conn_set_tmpOP_channel(t_connection * c, char const * tmpOP_channel)
{
	char * tmp;

	if (!c)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL conn");
	  return -1;
	}

	if (c->protocol.chat.tmpOP_channel)
	{
	  free((void *)c->protocol.chat.tmpOP_channel);
	  c->protocol.chat.tmpOP_channel = NULL;
	}

	if (tmpOP_channel)
	{
	  if (!(tmp = strdup(tmpOP_channel)))
	  {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not strdup tmpOP_channel");
	    return -1;
	  }
	  c->protocol.chat.tmpOP_channel = tmp;
	}

	return 0;
}

extern char const * conn_get_tmpOP_channel(t_connection * c)
{
	if (!c)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL conn");
	  return NULL;
	}
	
	return c->protocol.chat.tmpOP_channel;
}

extern int conn_set_tmpVOICE_channel(t_connection * c, char const * tmpVOICE_channel)
{
	char * tmp;

	if (!c)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL conn");
	  return -1;
	}

	if (c->protocol.chat.tmpVOICE_channel)
	{
	  free((void *)c->protocol.chat.tmpVOICE_channel);
	  c->protocol.chat.tmpVOICE_channel = NULL;
	}

	if (tmpVOICE_channel)
	{
	  if (!(tmp = strdup(tmpVOICE_channel)))
	  {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not strdup tmpVOICE_channel");
	    return -1;
	  }
	  c->protocol.chat.tmpVOICE_channel = tmp;
	}

	return 0;
}

extern char const * conn_get_tmpVOICE_channel(t_connection * c)
{
	if (!c)
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"got NULL conn");
	  return NULL;
	}
	
	return c->protocol.chat.tmpVOICE_channel;
}

