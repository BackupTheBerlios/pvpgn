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

#ifdef WIN32_GUI
#include <bnetd/winmain.h>
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

#ifdef WIN32
# include "compat/socket.h" /* is this needed */
#endif

#include "compat/strdup.h"
#include "common/packet.h"
#include "common/eventlog.h"
#include "account.h"
#include "connection.h"
#include "common/queue.h"
#include "prefs.h"
#include "common/bn_type.h"
#include "common/list.h"
#include "common/addr.h"
#include "gametrans.h"
#include "versioncheck.h"
#include "anongame.h"
#include "tournament.h"
#include "timer.h"
#include "war3ladder.h"
#include "anongame_maplists.h"
#include "common/setup_after.h"

#define MAX_LEVEL 100

/* [quetzal] 20020827 - this one get modified by anongame_queue player when there're enough
 * players and map has been chosen based on their preferences. otherwise its NULL
 */
static char * mapname = NULL;

static int players[ANONGAME_TYPES] = {0,0,0,0,0,0,0,0,0,0,0};
static t_connection * player[ANONGAME_TYPES][ANONGAME_MAX_GAMECOUNT];

/* [quetzal] 20020815 - queue to hold matching players */
static t_list * matchlists[ANONGAME_TYPES][MAX_LEVEL];

long average_anongame_search_time = 30;
unsigned int anongame_search_count = 0;

/**********************************************************************************/
static t_connection * _connlist_find_connection_by_uid(int uid);
static int _anongame_gametype_to_queue(int type, int gametype);
static int _anongame_level_by_queue(t_connection *c, int queue);
static char * _get_map_from_prefs(int queue, t_uint32 cur_prefs, const char * clienttag);

static int _handle_anongame_PG_search(t_connection * c, t_packet const * packet);
static int _handle_anongame_AT_inv_search(t_connection * c, t_packet const * packet);
static int _handle_anongame_AT_search(t_connection * c, t_packet const * packet);
static int _anongame_queue_player(t_connection * c, int queue, t_uint32 map_prefs);
static int _anongame_match_PG_player(t_connection * c, int queue);
static int _anongame_match_AT_player(t_connection * c, int queue);
static int _anongame_search_found(int queue);
/**********************************************************************************/

static t_connection * _connlist_find_connection_by_uid(int uid)
{
    return connlist_find_connection_by_account(accountlist_find_account_by_uid(uid));
}

static int _anongame_gametype_to_queue(int type, int gametype)
{
    switch (type) {
	case 0: /* PG */
	    switch (gametype) {
		case 0:
		    return ANONGAME_TYPE_1V1;
		case 1:
		    return ANONGAME_TYPE_2V2;
		case 2:
		    return ANONGAME_TYPE_3V3;
		case 3:
		    return ANONGAME_TYPE_4V4;
		case 4:
		    return ANONGAME_TYPE_SMALL_FFA;
		case 5:
		    return ANONGAME_TYPE_2V2V2;
		default:
	    	    eventlog(eventlog_level_error,__FUNCTION__,"invalid PG game type: %d",gametype);
	    	    return -1;
	    }
	case 1: /* AT */
	    switch (gametype) {
		case 0:
		    return ANONGAME_TYPE_AT_2V2;
		case 2:
		    return ANONGAME_TYPE_AT_3V3;
		case 3:
		    return ANONGAME_TYPE_AT_4V4;
		default:
	    	    eventlog(eventlog_level_error,__FUNCTION__,"invalid AT game type: %d",gametype);
	    	    return -1;
	    }
	case 2: /* TY */
	    return ANONGAME_TYPE_TY;
	default:
	    eventlog(eventlog_level_error,__FUNCTION__,"invalid type: %d",type);
	    return -1;
    }
}

static int _anongame_level_by_queue(t_connection *c, int queue) 
{
    char const * ct = conn_get_clienttag(c);
    
    if(queue >= ANONGAME_TYPES) {
	eventlog(eventlog_level_fatal,__FUNCTION__, "unknown queue: %d", queue);
	return -1;
    }
    
    switch(queue) {
	case ANONGAME_TYPE_1V1:
	    return account_get_sololevel(conn_get_account(c),ct);
	case ANONGAME_TYPE_2V2:
	case ANONGAME_TYPE_3V3:
	case ANONGAME_TYPE_4V4:
	case ANONGAME_TYPE_2V2V2:
	    return account_get_teamlevel(conn_get_account(c),ct);
	case ANONGAME_TYPE_SMALL_FFA:
	case ANONGAME_TYPE_TEAM_FFA:
	    return account_get_ffalevel(conn_get_account(c),ct);
	case ANONGAME_TYPE_AT_2V2:
	case ANONGAME_TYPE_AT_3V3:
	case ANONGAME_TYPE_AT_4V4:
	case ANONGAME_TYPE_TY: /* do we want to set this? */
	    return 0;
	default:
	    break;
    }
    return -1;
}

static char * _get_map_from_prefs(int queue, t_uint32 cur_prefs, const char * clienttag)
{
    int i, j = 0;
    char * default_map, *selected;
    char *res_maps[32];
    t_list * mapnames;
    
    if (clienttag) {
	mapnames = anongame_get_w3xp_maplist(queue, clienttag);
	default_map = "Maps\\(8)PlainsOfSnow.w3m";
    } else {
	eventlog(eventlog_level_error,__FUNCTION__, "invalid clienttag : %s", clienttag);
	return "Maps\\(8)PlainsOfSnow.w3m";
    }
    
    for (i = 0; i < 32; i++)
	res_maps[i] = NULL;
    
    for (i = 0; i < 32; i++) {
	if (cur_prefs & 1)
	    res_maps[j++] = list_get_data_by_pos(mapnames, i);
        cur_prefs >>= 1;
    }
    
    i = rand() % j;
    if (res_maps[i])
	selected = res_maps[i];
    else
	selected = default_map;
    
    eventlog(eventlog_level_debug,__FUNCTION__, "got map %s from prefs", selected);
    return selected;
}

/**********/
static int _handle_anongame_PG_search(t_connection * c, t_packet const * packet)
{
    t_packet * rpacket = NULL;
    t_anongame * a = NULL; 
    int	temp;
    
    if (!(a = conn_get_anongame(c))) {
	if (!(a = conn_create_anongame(c))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"[%d] conn_create_anongame failed",conn_get_socket(c));
	    return -1;
	}
    }
        
    conn_set_anongame_search_starttime(c,time(NULL));
    
    a->tc[0] = c; /* not sure i need to do this */
    
    a->count		= bn_int_get(packet->u.client_findanongame.count);
    a->id		= bn_int_get(packet->u.client_findanongame.id);
    a->race		= bn_int_get(packet->u.client_findanongame.race);
    a->map_prefs	= bn_int_get(packet->u.client_findanongame.map_prefs);
    a->type		= bn_byte_get(packet->u.client_findanongame.type);
    a->gametype		= bn_byte_get(packet->u.client_findanongame.gametype);
    
    if ((a->queue = _anongame_gametype_to_queue(a->type, a->gametype))<0) {
	eventlog(eventlog_level_error,__FUNCTION__,"invalid queue: %d",a->queue);
	return -1;
    }


    account_set_w3pgrace(conn_get_account(c), conn_get_clienttag(c), a->race);

    /* send search reply to client */
    if (!(rpacket = packet_create(packet_class_bnet)))
	return -1;
    packet_set_size(rpacket,sizeof(t_server_anongame_search_reply));
    packet_set_type(rpacket,SERVER_ANONGAME_SEARCH_REPLY);
    bn_byte_set(&rpacket->u.server_anongame_search_reply.option,SERVER_FINDANONGAME_SEARCH);
    bn_int_set(&rpacket->u.server_anongame_search_reply.count,a->count);
    bn_int_set(&rpacket->u.server_anongame_search_reply.reply,0);
    temp = (int)average_anongame_search_time;
    packet_append_data(rpacket, &temp, 2);
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    /* end search reply */
    
    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] matching PG player in queue: %d",conn_get_socket(c),a->queue);

    if(_anongame_queue_player(c, a->queue, a->map_prefs) < 0) {
	eventlog(eventlog_level_error,__FUNCTION__,"queue failed");
	return -1;
    }
    
    _anongame_match_PG_player(c, a->queue);
    
    eventlog(eventlog_level_trace,__FUNCTION__, "totalplayers for queue %d: %d", a->queue, anongame_totalplayers(a->queue));
    
    /* if enough players are queued send found packet */
    if(players[a->queue] == anongame_totalplayers(a->queue))
	if (_anongame_search_found(a->queue) < 0)
	    return -1;
    
    return 0;
}

static int _handle_anongame_AT_inv_search(t_connection * c, t_packet const * packet)
{
    t_packet * rpacket = NULL;
    t_connection * tc[4];
    t_anongame * a = NULL;
    t_anongame * ta = NULL;
    t_uint8 teamsize = 0;
    int	set = 1;
    int	i, temp;
    
    if (!(a = conn_get_anongame(c))) {
	if (!(a = conn_create_anongame(c))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"[%d] conn_create_anongame failed",conn_get_socket(c));
	    return -1;
	}
    }
        
    conn_set_anongame_search_starttime(c,time(NULL));
    
    a->count		= bn_int_get(packet->u.client_findanongame_at_inv.count);
    a->id		= bn_int_get(packet->u.client_findanongame_at_inv.id);
    a->tid		= bn_int_get(packet->u.client_findanongame_at_inv.tid);
    a->race		= bn_int_get(packet->u.client_findanongame_at_inv.race);
    a->map_prefs	= bn_int_get(packet->u.client_findanongame_at_inv.map_prefs);
    a->type		= bn_byte_get(packet->u.client_findanongame_at_inv.type);
    a->gametype		= bn_byte_get(packet->u.client_findanongame_at_inv.gametype);
    
    teamsize		= bn_byte_get(packet->u.client_findanongame_at_inv.teamsize);

    if ((a->queue = _anongame_gametype_to_queue(a->type, a->gametype))<0) {
	eventlog(eventlog_level_error,__FUNCTION__,"invalid queue: %d",a->queue);
	return -1;
    }

    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] setting AT player to tid: %X",conn_get_socket(c),a->tid);

    conn_set_atid(c, a->tid);
    
    account_set_w3pgrace(conn_get_account(c), conn_get_clienttag(c), a->race);

    /* send search reply to client */
    if (!(rpacket = packet_create(packet_class_bnet)))
	return -1;
    packet_set_size(rpacket,sizeof(t_server_anongame_search_reply));
    packet_set_type(rpacket,SERVER_ANONGAME_SEARCH_REPLY);
    bn_byte_set(&rpacket->u.server_anongame_search_reply.option,SERVER_FINDANONGAME_SEARCH);
    bn_int_set(&rpacket->u.server_anongame_search_reply.count,a->count);
    bn_int_set(&rpacket->u.server_anongame_search_reply.reply,0);
    temp = (int)average_anongame_search_time;
    packet_append_data(rpacket, &temp, 2);
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    /* end search reply */
    
    for (i = 0; i < teamsize; i++) {
	tc[i] = _connlist_find_connection_by_uid(bn_int_get(packet->u.client_findanongame_at.info[i]));

	if (!(ta = conn_get_anongame(tc[i]))) {
	    if (!(ta = conn_create_anongame(tc[i]))) {
		eventlog(eventlog_level_error,__FUNCTION__,"[%d] conn_create_anongame failed",conn_get_socket(tc[i]));
		return -1;
	    }
	}
	
	if (ta->tc[i] == NULL)
	    ta->tc[i] = tc[i];

	ta->type = a->type;
	ta->gametype = a->gametype;
	ta->queue = a->queue;
	ta->map_prefs = a->map_prefs;

	eventlog(eventlog_level_trace,__FUNCTION__,"[%d] player %d: tid=%X inviter tid=%X",
		conn_get_socket(c),i+1,ta->tid,a->tid);
	    
	if (ta->tid != a->tid) {
	    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] tid mismatch, not queueing team",conn_get_socket(c));
	    set = 0;
	}
    }
    
    /* fixme: we should queue the team as a single player, then matching would be much easer */
    if (set) { /* queue whole team at one time */
	for (i = 0; i < teamsize; i++) {
	    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] queueing AT player in queue: %d",conn_get_socket(tc[i]),a->queue);
	    if(_anongame_queue_player(tc[i], a->queue, a->map_prefs) < 0) {
		eventlog(eventlog_level_error,__FUNCTION__,"queue failed");
		return -1;
	    }
	}
	_anongame_match_AT_player(c, a->queue);
    }
    else
	return 0;
    
    eventlog(eventlog_level_trace,__FUNCTION__, "totalplayers for queue %d: %d", a->queue, anongame_totalplayers(a->queue));
    
    /* if enough players are queued send found packet */
    if(players[a->queue] == anongame_totalplayers(a->queue))
	if (_anongame_search_found(a->queue) < 0)
	    return -1;
    
    return 0;
}

static int _handle_anongame_AT_search(t_connection * c, t_packet const * packet)
{
    t_packet * rpacket = NULL;
    t_connection * tc[4];
    t_anongame * a = NULL;
    t_anongame * ta = NULL;
    t_uint8 teamsize = 0;
    int	i, temp;
    
    if (!(a = conn_get_anongame(c))) {
	if (!(a = conn_create_anongame(c))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"[%d] conn_create_anongame failed",conn_get_socket(c));
	    return -1;
	}
    }
    
    conn_set_anongame_search_starttime(c,time(NULL));
    
    a->count	= bn_int_get(packet->u.client_findanongame_at.count);
    a->id	= bn_int_get(packet->u.client_findanongame_at.id);	
    a->tid	= bn_int_get(packet->u.client_findanongame_at.tid);
    a->race	= bn_int_get(packet->u.client_findanongame_at.race);
    
    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] setting AT player to tid: %X",conn_get_socket(c),a->tid);

    teamsize	= bn_byte_get(packet->u.client_findanongame_at.teamsize);

    conn_set_atid(c, a->tid);
    
    account_set_w3pgrace(conn_get_account(c), conn_get_clienttag(c), a->race);

    /* send search reply to client */
    if (!(rpacket = packet_create(packet_class_bnet)))
	return -1;
    packet_set_size(rpacket,sizeof(t_server_anongame_search_reply));
    packet_set_type(rpacket,SERVER_ANONGAME_SEARCH_REPLY);
    bn_byte_set(&rpacket->u.server_anongame_search_reply.option,SERVER_FINDANONGAME_SEARCH);
    bn_int_set(&rpacket->u.server_anongame_search_reply.count,a->count);
    bn_int_set(&rpacket->u.server_anongame_search_reply.reply,0);
    temp = (int)average_anongame_search_time;
    packet_append_data(rpacket, &temp, 2);
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    /* end search reply */
    
    for (i = 0; i < teamsize; i++) {
	tc[i] = _connlist_find_connection_by_uid(bn_int_get(packet->u.client_findanongame_at.info[i]));
	
	if (!(ta = conn_get_anongame(tc[i]))) {
	    if (!(ta = conn_create_anongame(tc[i]))) {
		eventlog(eventlog_level_error,__FUNCTION__,"[%d] conn_create_anongame failed",conn_get_socket(tc[i]));
		return -1;
	    }
	}
    
	if (ta->tc[i] == NULL)
	    ta->tc[i] = tc[i];
	
	eventlog(eventlog_level_trace,__FUNCTION__,"[%d] player %d: tid=%X joiner tid=%X",
	    conn_get_socket(c),i+1,ta->tid,a->tid);

	if (ta->tid != a->tid) {
	    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] tid mismatch, not queueing team",conn_get_socket(c));
	    return 0;
	}
    }
    
    for (i = 0; i < teamsize; i++) {
	eventlog(eventlog_level_trace,__FUNCTION__,"[%d] queueing AT player in queue: %d",conn_get_socket(tc[i]),a->queue);
	
	if(_anongame_queue_player(tc[i], a->queue, a->map_prefs) < 0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"queue failed");
	    return -1;
	}
    }
    
    _anongame_match_AT_player(c, a->queue);
    
    eventlog(eventlog_level_trace,__FUNCTION__, "totalplayers for queue %d: %d", a->queue, anongame_totalplayers(a->queue));
    
    /* if enough players are queued send found packet */
    if(players[a->queue] == anongame_totalplayers(a->queue))
	if (_anongame_search_found(a->queue) < 0)
	    return -1;
    
    return 0;
}

static int _anongame_queue_player(t_connection * c, int queue, t_uint32 map_prefs)
{
    int level;
    t_matchdata *md;
    
    if (!c) {
	eventlog(eventlog_level_fatal,__FUNCTION__, "got NULL connection");
    }
    
    if(queue >= ANONGAME_TYPES) {
	eventlog(eventlog_level_fatal,__FUNCTION__, "unknown queue: %d", queue);
	return -1;
    }
    
    level = _anongame_level_by_queue(c, queue);
    
    if (!matchlists[queue][level])
	matchlists[queue][level] = list_create();
    
    md = malloc(sizeof(t_matchdata));
    md->c = c;
    md->map_prefs = map_prefs;
    md->versiontag = versioncheck_get_versiontag(conn_get_versioncheck(c)) ? strdup(versioncheck_get_versiontag(conn_get_versioncheck(c))) : NULL;
    
    list_append_data(matchlists[queue][level], md);
    
    return 0;
}

static int _anongame_match_PG_player(t_connection * c, int queue)
{
    int level = _anongame_level_by_queue(c, queue);
    int delta = 0;
    int i;
    t_matchdata *md;
    t_elem *curr;
    t_uint32 cur_prefs = 0xffffffff;
        
    players[queue] = 0;

    eventlog(eventlog_level_trace,__FUNCTION__, "[%d] PG matching started for level %d player", conn_get_socket(c), level);
    
    while (abs(delta) < 7) {
	eventlog(eventlog_level_trace,__FUNCTION__, "Traversing level %d players", level + delta);
	
	LIST_TRAVERSE(matchlists[queue][level + delta], curr)
	{
	    md = elem_get_data(curr);
	    if (md->versiontag && versioncheck_get_versiontag(conn_get_versioncheck(c)) && !strcmp(md->versiontag, versioncheck_get_versiontag(conn_get_versioncheck(c)))) {
		if (cur_prefs & md->map_prefs) {
		    cur_prefs &= md->map_prefs;
		    player[queue][players[queue]++] = md->c;
		    if (players[queue] == anongame_totalplayers(queue)) {
			eventlog(eventlog_level_trace,__FUNCTION__, "Found enough players (%d), calling unqueue", players[queue]);
			for (i = 0; i < players[queue]; i++)
			    anongame_unqueue_player(player[queue][i], queue);
			
			mapname = _get_map_from_prefs(queue, cur_prefs, conn_get_clienttag(c));
			return 0;
		    }
		}
	    }
	}

	if (delta <= 0 || level - delta < 0)
	    delta = abs(delta) + 1;
	else
	    delta = -delta;

	if (level + delta > MAX_LEVEL)
	    delta = -delta;
	
	if (level + delta < 0) break; // cant really happen
	
    }
    eventlog(eventlog_level_trace,__FUNCTION__,"[%d] Matching finished, not enough players (found %d)", conn_get_socket(c), players[queue]);
    mapname = NULL;
    return 0;
}

static int _anongame_match_AT_player(t_connection * c, int queue)
{
    int level = _anongame_level_by_queue(c, queue);
    t_elem *curr;
    t_atcountinfo acinfo[100];
    t_uint32 cur_prefs = 0xffffffff;
    t_matchdata *md;
    int i, team1_id = 0, team2_id = 0;

    players[queue] = 0;
    
    for (i = 0; i < 100; i++) {
	acinfo[i].atid = 0;
	acinfo[i].count = 0;
	acinfo[i].map_prefs = 0xffffffff;
    }
    
    eventlog(eventlog_level_trace,__FUNCTION__, "[%d] AT matching for gametype %d, gathering teams (%d players available)",
	    conn_get_socket(c), queue, list_get_length(matchlists[queue][level]));
    
    /* find out how many players in each team available */
    LIST_TRAVERSE(matchlists[queue][level], curr)
    {
	md = elem_get_data(curr);
	if (conn_get_atid(md->c) == 0) {
	    eventlog(eventlog_level_error,__FUNCTION__, "AT matching, got zero arranged team id");
	} else {
	    if (md->versiontag && versioncheck_get_versiontag(conn_get_versioncheck(c)) && !strcmp(md->versiontag, versioncheck_get_versiontag(conn_get_versioncheck(c)))) {
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
    
    /* take first 2 teams with enough players */
    for (i = 0; i < 100; i++) {
	if (!acinfo[i].atid) break;
	
	eventlog(eventlog_level_trace,__FUNCTION__, "AT count information entry %d: id = %X, count = %X", 
		i, acinfo[i].atid, acinfo[i].count);
	/* found team1 */
	if (acinfo[i].count >= anongame_totalplayers(queue) / 2 && !team1_id && cur_prefs & acinfo[i].map_prefs) {
	    cur_prefs &= acinfo[i].map_prefs;
	    eventlog(eventlog_level_trace,__FUNCTION__, "AT matching, found one team, teamid = %X", acinfo[i].atid);
	    team1_id = acinfo[i].atid;
	/* found team2 */
	} else if (acinfo[i].count >= anongame_totalplayers(queue) / 2 && !team2_id && cur_prefs & acinfo[i].map_prefs) {
	    cur_prefs &= acinfo[i].map_prefs;
	    eventlog(eventlog_level_trace,__FUNCTION__, "AT matching, found other team, teamid = %X", acinfo[i].atid);
	    team2_id = acinfo[i].atid;
	}
	/* both teams found */
	if (team1_id && team2_id) {
	    int j = 0, k = 0;
	    /* add team1 first */
	    LIST_TRAVERSE(matchlists[queue][level], curr)
	    {
		md = elem_get_data(curr);
		if (team1_id == conn_get_atid(md->c))
		    player[queue][2*(j++)] = md->c;
	    }
	    /* and then team2 */
	    LIST_TRAVERSE(matchlists[queue][level], curr)
	    {
		md = elem_get_data(curr);
		if (team2_id == conn_get_atid(md->c))
		    player[queue][1+2*(k++)] = md->c;
	    }
	    players[queue] = j + k;
	    /* added enough players? remove em from queue and quit */
	    if (players[queue] == anongame_totalplayers(queue)) {
		eventlog(eventlog_level_trace,__FUNCTION__,"AT Found enough players in both teams (%d), calling unqueue", players[queue]);
		for (i = 0; i < players[queue]; i++)
		    anongame_unqueue_player(player[queue][i], queue);
		mapname = _get_map_from_prefs(queue, cur_prefs, conn_get_clienttag(c));
		return 0;
	    }
	    eventlog(eventlog_level_error,__FUNCTION__, "Found teams, but was unable to form players array");
	    return -1;
	}
    }
    eventlog(eventlog_level_trace, "anongame_queue_player", "[%d] AT matching finished, not enough players", conn_get_socket(c));
    return 0;
}

static int w3routeip = -1; /* changed by dizzy to show the w3routeshow addr if available */
static unsigned short w3routeport = 6200;

static int _anongame_search_found(int queue)
{
    t_packet		*rpacket = NULL;
    t_anongameinfo	*info;
    t_anongame		*a = NULL; 
    int i, j, temp;

    /* FIXME: maybe periodically lookup w3routeaddr to support dynamic ips?
     * (or should dns lookup be even quick enough to do it everytime?)
     */
    
    if(w3routeip==-1) {
	t_addr * routeraddr;
	
	if (prefs_get_w3route_show())
	    routeraddr = addr_create_str(prefs_get_w3route_show(), 0, 6200);
	else
	    routeraddr = addr_create_str(prefs_get_w3route_addr(), 0, 6200);
	
//	eventlog(eventlog_level_trace,__FUNCTION__,"setting w3routeip, addr from prefs");
	
	if(!routeraddr) {
	    eventlog(eventlog_level_error,__FUNCTION__,"error getting w3route_addr");
	    return -1;
	}
	
	w3routeip = addr_get_ip(routeraddr);
	w3routeport = addr_get_port(routeraddr);
	addr_destroy(routeraddr);
    }
    
    info = anongameinfo_create(anongame_totalplayers(queue));
    
    if(!info) {
	eventlog(eventlog_level_error,__FUNCTION__,"anongameinfo_create failed");
	return -1;
    }
    
    /* send found packet to each of the players */
    for(i=0; i<players[queue]; i++) {
	if(!(a = conn_get_anongame(player[queue][i]))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"no anongame struct for queued player");
	    return -1;
	}
	
	a->info = info;
	eventlog(eventlog_level_trace,__FUNCTION__, "anongame_get_totalplayers: %d", a->info->totalplayers);
	a->playernum = i+1;
	
	for(j=0; j<players[queue]; j++) {
	    a->info->player[j] = player[queue][j];
	    a->info->account[j] = conn_get_account(player[queue][j]);
	}
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	    return -1;
	
	packet_set_size(rpacket,sizeof(t_server_anongame_found));
	packet_set_type(rpacket,SERVER_ANONGAME_FOUND);
	bn_byte_set(&rpacket->u.server_anongame_found.option,1);
	bn_int_set(&rpacket->u.server_anongame_found.count,a->count);
	bn_int_set(&rpacket->u.server_anongame_found.unknown1,0);
	bn_int_nset(&rpacket->u.server_anongame_found.ip,w3routeip);
	bn_short_set(&rpacket->u.server_anongame_found.port,w3routeport);
	bn_byte_set(&rpacket->u.server_anongame_found.unknown2,i+1);
	bn_byte_set(&rpacket->u.server_anongame_found.unknown3,queue);
	bn_short_set(&rpacket->u.server_anongame_found.unknown4,0);
	bn_int_set(&rpacket->u.server_anongame_found.id,0xdeadbeef);
	bn_byte_set(&rpacket->u.server_anongame_found.unknown5,6);
	bn_byte_set(&rpacket->u.server_anongame_found.type,a->type);
	bn_byte_set(&rpacket->u.server_anongame_found.gametype,a->gametype);
	
	if (!mapname) /* set in anongame_queue_player() */
	    eventlog(eventlog_level_fatal,__FUNCTION__,"got all players, but there's no map to play on");
	else
	    eventlog(eventlog_level_info,__FUNCTION__,"selected map: %s", mapname);
	
	packet_append_string(rpacket, mapname); 
	temp=-1;
	packet_append_data(rpacket, &temp, 4);

	{
	    /* dizzy: this changed in 1.05 */
	    int gametype_tab [] = {
		SERVER_ANONGAME_SOLO_STR,
		SERVER_ANONGAME_TEAM_STR,
		SERVER_ANONGAME_TEAM_STR,
		SERVER_ANONGAME_TEAM_STR,
		SERVER_ANONGAME_SFFA_STR,
		SERVER_ANONGAME_AT2v2_STR,
		0, /* Team FFA is no longer supported */
		SERVER_ANONGAME_AT3v3_STR,
		SERVER_ANONGAME_AT4v4_STR,
		SERVER_ANONGAME_TY_STR,
		SERVER_ANONGAME_TEAM_STR
	    };
	
	    if (queue > ANONGAME_TYPES) {
		eventlog(eventlog_level_error,__FUNCTION__, "invalid queue (%d)", queue);
		temp = 0;
	    } 
	    else
		bn_int_set((bn_int*)&temp,gametype_tab[queue]);
	}
	packet_append_data(rpacket, &temp, 4);
	
	/* total players */
	bn_byte_set((bn_byte*)&temp,anongame_totalplayers(queue));
	packet_append_data(rpacket, &temp, 1);
	
	/* number of teams */
	/* next byte is 1v1 = 0 , sffa = 0 , rest = 2 , (blizzard default) */
	/* tested with 2v2v2, works [Omega]*/
	if(queue == ANONGAME_TYPE_1V1 || queue == ANONGAME_TYPE_SMALL_FFA)
	    temp = 0;
	else if (queue == ANONGAME_TYPE_2V2V2)
	    temp = 3;
	else
	    temp = 2;
	
	packet_append_data(rpacket, &temp, 1);

	temp=0;
	packet_append_data(rpacket, &temp, 2);
	bn_byte_set((bn_byte*)&temp,0x02);	/* visibility. 0x01 - dark 0x02 - default */
	packet_append_data(rpacket, &temp, 1);
	bn_byte_set((bn_byte*)&temp,0x02);
	packet_append_data(rpacket, &temp, 1);
	
	queue_push_packet(conn_get_out_queue(player[queue][i]),rpacket);
	packet_del_ref(rpacket);
    }

    /* clear queue */
    players[queue] = 0;
    return 0;
}

/**********/
/**********************************************************************************/
/* external functions */
/**********************************************************************************/
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

/**********/
extern int handle_anongame_search(t_connection * c, t_packet const * packet)
{
    t_uint8	option = bn_byte_get(packet->u.client_findanongame.option);
    
    if (option==CLIENT_FINDANONGAME_AT_INVITER_SEARCH)
	return _handle_anongame_AT_inv_search(c, packet);

    else if (option==CLIENT_FINDANONGAME_AT_SEARCH)
	return _handle_anongame_AT_search(c, packet);

    else if (option==CLIENT_FINDANONGAME_SEARCH)
	return _handle_anongame_PG_search(c, packet);
    
    else
	return -1;
}

extern int anongame_unqueue_player(t_connection * c, int queue)
{
    int i;
    t_elem *curr;
    t_matchdata *md;
    
    if(queue >= ANONGAME_TYPES) {
	eventlog(eventlog_level_fatal,__FUNCTION__, "unknown queue: %d", queue);
	return -1;
    }
    
    if (conn_get_anongame_search_starttime(c) != ((time_t)0))  {
	average_anongame_search_time *=	anongame_search_count;
	average_anongame_search_time += (long)difftime(time(NULL),conn_get_anongame_search_starttime(c));
	anongame_search_count++;
	average_anongame_search_time /= anongame_search_count;
	if (anongame_search_count > 20000)
	    anongame_search_count = anongame_search_count / 2; /* to prevent an overflow of the average time */
	conn_set_anongame_search_starttime(c, ((time_t)0));
    }
    	
    for (i = 0; i < MAX_LEVEL; i++) {
	if (matchlists[queue][i] == NULL)
	    continue;
	
	LIST_TRAVERSE(matchlists[queue][i], curr)
	{
	    md = elem_get_data(curr);
	    if (md->c == c) {
		eventlog(eventlog_level_trace,__FUNCTION__, "unqueued player [%d] level %d", conn_get_socket(c), i);
		list_remove_elem(matchlists[queue][i], curr);
		if (md->versiontag != NULL)
		    free((void *)md->versiontag);
		free(md);
		return 0;
	    }
	}
    }
    
    eventlog(eventlog_level_trace,__FUNCTION__, "[%d] player not found in queue", conn_get_socket(c));
    return -1;
}

extern int anongame_unqueue_team(t_connection *c, int queue)
{
/* [smith] 20030427 fixed Big-Endian/Little-Endian conversion (Solaris bug)
 * then use packet_append_data for append platform dependent data types - like "int",
 * cos this code was broken for BE platforms. it's rewriten in platform independent style whis 
 * usege bn_int and other bn_* like datatypes and fuctions for wor with datatypes - bn_int_set(),
 * what provide right byteorder, not depended on LE/BE
 *
 * fixed broken htonl() conversion for BE platforms - change it to  bn_int_nset().
 * i hope it's worked on intel too %)
 */
	int id, i;
	t_elem *curr;
	t_matchdata *md;
	
	if(queue >= ANONGAME_TYPES) {
		eventlog(eventlog_level_fatal,__FUNCTION__, "unknown queue: %d", queue);
		return -1;
	}
	if (!c) {
		eventlog(eventlog_level_fatal,__FUNCTION__, "got NULL connection");
		return -1;
	}

	id = conn_get_atid(c);
	
	for (i = 0; i < MAX_LEVEL; i++) {
		if (matchlists[queue][i] == NULL) continue;

		LIST_TRAVERSE(matchlists[queue][i], curr)
		{
			md = elem_get_data(curr);
			if (id == conn_get_atid(md->c)) {
				eventlog(eventlog_level_trace,__FUNCTION__, "unqueued player [%d] level %d", conn_get_socket(md->c), i);
				list_remove_elem(matchlists[queue][i], curr);
				if (md->versiontag != NULL)
				    free((void *)md->versiontag);
				free(md);
			}
		}
	}
	return 0;
}

/**********/
extern int anongame_totalplayers(int queue)
{
    switch(queue) {
	case ANONGAME_TYPE_1V1:
		return 2;
	case ANONGAME_TYPE_2V2:
	case ANONGAME_TYPE_AT_2V2:
	case ANONGAME_TYPE_SMALL_FFA: /* fixme: total players not always 4 */
		return 4;
	case ANONGAME_TYPE_3V3:
	case ANONGAME_TYPE_AT_3V3:
	case ANONGAME_TYPE_2V2V2:
		return 6;
	case ANONGAME_TYPE_4V4:
	case ANONGAME_TYPE_AT_4V4:
	case ANONGAME_TYPE_TEAM_FFA:
		return 8;
	case ANONGAME_TYPE_TY:	
		return tournament_get_totalplayers();
	default:
		eventlog(eventlog_level_error,__FUNCTION__, "unknown queue: %d", queue);
		return 0;
    }
}

extern char anongame_arranged(int queue)
{
    switch(queue) {
	case ANONGAME_TYPE_AT_2V2:
	case ANONGAME_TYPE_AT_3V3:
	case ANONGAME_TYPE_AT_4V4:
		return 1;
	case ANONGAME_TYPE_TY:
		return tournament_is_arranged();
	default:
		return 0;
    }
}

extern int anongame_stats(t_connection * c)
{
    int			i;
    int			wins=0, losses=0, discs=0;
    t_connection	* gamec = conn_get_routeconn(c);
    t_anongame		* a = conn_get_anongame(gamec);
    int			tp = anongame_get_totalplayers(a);
    int                 oppon_level[ANONGAME_MAX_GAMECOUNT];
    t_uint8		gametype = a->queue;
    t_uint8		plnum = a->playernum;
    char const *        ct = conn_get_clienttag(c);
    
    /* do nothing till all other players have w3route conn closed */
    for(i=0; i<tp; i++)
	if(i+1 != plnum && a->info->player[i])
	    if(conn_get_routeconn(a->info->player[i]))
		return 0;
    
    /* count wins, losses, discs */
    for(i=0; i<tp; i++) {
	if(a->info->result[i] == W3_GAMERESULT_WIN)
	    wins++;
	else if(a->info->result[i] == W3_GAMERESULT_LOSS)
	    losses++;
	else
	    discs++;
    }
    
    /* do some sanity checking (hack prevention) */
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
    
    /* prevent users from getting loss if server is shutdown (does not prevent errors from crash) - [Omega] */
    if (discs == tp)
	if (!wins)
    	    return -1;
    
    /* according to zap, order of players in anongame is:
     * for PG: t1_p1, t2_p1, t1_p2, t2_p2, ...
     * for AT: t1_p1, t1_p2, ..., t2_p1, t2_p2, ...
     */

    /* opponent level calculation has to be done here, because later on, the level of other players
     * may allready be modified
     */
    for (i=0; i<tp; i++)
    {
	int j,k;
        t_account * oacc; 
        oppon_level[i]=0;
        switch(gametype) {
	    case ANONGAME_TYPE_TY:
		/* FIXME-TY: ADD TOURNAMENT STATS RECORDING (this part not required?) */
		break;
	    case ANONGAME_TYPE_1V1:
		oppon_level[i] = account_get_sololevel(a->info->account[(i+1)%tp],ct);
		break;
    	    case ANONGAME_TYPE_SMALL_FFA:
		/* oppon_level = average level of all other players */
		for (j=0; j<tp; j++)
		    if (i!=j)
			oppon_level[i]+=account_get_ffalevel(a->info->account[j],ct);
		oppon_level[i]/=(tp-1);
		break;
	    case ANONGAME_TYPE_AT_2V2:
    	    case ANONGAME_TYPE_AT_3V3:
    	    case ANONGAME_TYPE_AT_4V4:
		if (i<(tp/2))
		    j=(tp/2);
		else
		    j=0;
		oacc = a->info->account[j];
		oppon_level[i]= account_get_atteamlevel(oacc,account_get_currentatteam(oacc),ct);
		break;
	    case ANONGAME_TYPE_2V2V2:
		/* I think this works [Omega] */
		k = i+1;
		for (j=0; j<(tp/3); j++)
		{
		    oppon_level[i]+= account_get_teamlevel(a->info->account[k%tp],ct);
		    k++;
		    oppon_level[i]+= account_get_teamlevel(a->info->account[k%tp],ct);
		    k = k+2;
		}
		oppon_level[i]/=((tp/3)*2);
		break;
	    default:
		/* oppon_level = average level of all opponents (which are every 2nd player in the list) */
		k = i+1;
		for (j=0; j<(tp/2); j++) 
		{
		    oppon_level[i]+= account_get_teamlevel(a->info->account[k%tp],ct);
		    k = k+2;
		}
		oppon_level[i]/=(tp/2);
	}
    }
    
    for(i=0; i<tp; i++) {
	t_account * acc;
        int result = a->info->result[i];
        
        if(result == -1)
	    result = W3_GAMERESULT_LOSS;
      
        acc = a->info->account[i];
      
        switch(gametype) {
	    case ANONGAME_TYPE_TY:
		if(result == W3_GAMERESULT_WIN)
		    tournament_add_stat(acc, 1);
		if(result == W3_GAMERESULT_LOSS)
		    tournament_add_stat(acc, 2);
		/* FIXME-TY: how to do ties? */
		break;
	    case ANONGAME_TYPE_AT_2V2:
	    case ANONGAME_TYPE_AT_3V3:
	    case ANONGAME_TYPE_AT_4V4:
	    	/* Added by DJP in an attempt to manage teamcount ! ( bug of previous CVS 1.2.4 ) */
		if(account_get_new_at_team(acc)==1) {
		    int temp;
		
		    account_set_new_at_team(acc,0);
		    temp = account_get_atteamcount(acc,ct);
		    temp = temp+1;
		    account_set_atteamcount(acc,ct,temp);
	        }
		if(result == W3_GAMERESULT_WIN)
		    account_set_saveATladderstats(acc,gametype,game_result_win,oppon_level[i],account_get_currentatteam(acc),conn_get_clienttag(c));
		if(result == W3_GAMERESULT_LOSS) 
	    	    account_set_saveATladderstats(acc,gametype,game_result_loss,oppon_level[i],account_get_currentatteam(acc),conn_get_clienttag(c));
		break;
	    case ANONGAME_TYPE_1V1:
	    case ANONGAME_TYPE_2V2:
	    case ANONGAME_TYPE_3V3:
	    case ANONGAME_TYPE_4V4:
	    case ANONGAME_TYPE_SMALL_FFA:
	    case ANONGAME_TYPE_2V2V2:
		if(result == W3_GAMERESULT_WIN)
		    account_set_saveladderstats(acc,gametype,game_result_win,oppon_level[i],conn_get_clienttag(c));
		if(result == W3_GAMERESULT_LOSS)
		    account_set_saveladderstats(acc,gametype,game_result_loss,oppon_level[i],conn_get_clienttag(c));
		break;
	    default:
		break;
	}
    }
    /* aaron: now update war3 ladders */
    war3_ladder_update_all_accounts();
    return 1;
}

/**********/
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
	for(i=0; i<ANONGAME_MAX_GAMECOUNT; i++) {
	   temp->player[i] = NULL;
	   temp->account[i] = NULL;
	   temp->result[i] = -1; /* consider DISC default */
	}

	return temp;
}

extern void anongameinfo_destroy(t_anongameinfo * i)
{
	if(!i) {
		eventlog(eventlog_level_error, "anongameinfo_destroy", "got NULL anongameinfo");
		return;
	}
	free(i);
}

/**********/
extern t_anongameinfo * anongame_get_info(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_count","got NULL anongame");
		return NULL;
	}

	return a->info;
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

extern int anongame_get_count(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_count","got NULL anongame");
		return 0;
	}
	return a->count;
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

extern t_connection * anongame_get_tc(t_anongame * a, int tpnumber)
{
	if (!a)
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame");
		return 0;
	}
	return a->tc[tpnumber];
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

extern t_uint32 anongame_get_handle(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_handle","got NULL anongame");
		return 0;
	}
	return a->handle;
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

extern char anongame_get_loaded(t_anongame * a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_loaded","got NULL anongame");
		return 0;
	}
	return a->loaded;
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

extern t_uint8 anongame_get_playernum(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_get_playernum","got NULL anongame");
		return 0;
	}
	return a->playernum;
}

extern t_uint8 anongame_get_queue(t_anongame *a)
{
	if (!a)
	{
		eventlog(eventlog_level_error,__FUNCTION__,"got NULL anongame");
		return 0;
	}
	return a->queue;
}

/**********/
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
   
   if (a->playernum < 1 || a->playernum > ANONGAME_MAX_GAMECOUNT) 
     {
	eventlog(eventlog_level_error,"anongame_set_result","invalid playernum: %d", a->playernum);
	return;
     }
   
   a->info->result[a->playernum-1] = result;
}

extern void anongame_set_handle(t_anongame *a, t_uint32 h)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_handle","got NULL anongame");
		return;
	}

	a->handle = h;
}

extern void anongame_set_addr(t_anongame *a, unsigned int addr)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_addr","got NULL anongame");
		return;
	}

	a->addr = addr;
}

extern void anongame_set_loaded(t_anongame * a, char loaded)
{
	if (!a)
	{
		eventlog(eventlog_level_error,"anongame_set_loaded","got NULL anongame");
		return;
	}

	a->loaded = loaded;
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

/**********/
/* move to own .c/.h file for handling w3route connections */
extern int handle_w3route_packet(t_connection * c, t_packet const * const packet)
{
//[smith] 20030427 fixed Big-Endian/Little-Endian conversion (Solaris bug) then use  packet_append_data for append platform dependent data types - like "int", cos this code was broken for BE platforms. it's rewriten in platform independent style whis usege bn_int and other bn_* like datatypes and fuctions for wor with datatypes - bn_int_set(), what provide right byteorder, not depended on LE/BE
//  fixed broken htonl() conversion for BE platforms - change it to  bn_int_nset(). i hope it's worked on intel too %) 

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

      /* set clienttag for w3route connections; we can do conn_get_clienttag() on them */
      conn_set_clienttag(c, conn_get_clienttag(gamec));

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
      bn_int_set(&rpacket->u.server_w3route_ack.unknown3,SERVER_W3ROUTE_ACK_UNKNOWN3);

//      bn_int_set(&rpacket->u.server_w3route_ack.unknown3,0xdeafbeef);
      bn_short_set(&rpacket->u.server_w3route_ack.unknown4,0xcccc);
      bn_byte_set(&rpacket->u.server_w3route_ack.playernum,anongame_get_playernum(a));
      bn_short_set(&rpacket->u.server_w3route_ack.unknown5,0x0002);
      bn_short_set(&rpacket->u.server_w3route_ack.port,conn_get_port(c));
      bn_int_nset(&rpacket->u.server_w3route_ack.ip,conn_get_addr(c));
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
   
   gametype = anongame_get_queue(a);
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
    case CLIENT_W3ROUTE_GAMERESULT_W3XP:
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
	      	if (anongame_get_player(a,i)) {
	           ac = conn_get_routeconn(anongame_get_player(a,i));
	           if(ac) {
		      timerlist_add_timer(ac,time(NULL)+(time_t)300,conn_shutdown,data); // 300 seconds or 5 minute timer
		      eventlog(eventlog_level_trace,"handle_w3route_packet","[%d] started timer to close w3route",conn_get_socket(ac));
		    }
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
	 bn_byte_set(&rpacket->u.server_w3route_host.unknown1, 0);
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
	char const * ct = conn_get_clienttag(c);

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
	gametype = anongame_get_queue(a);
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
					{
					    unsigned short port = conn_get_game_port(o);
					    unsigned int addr = conn_get_game_addr(o);
					    
					    gametrans_net(conn_get_game_addr(jc), conn_get_game_port(jc), conn_get_local_addr(jc), conn_get_local_port(jc), &addr, &port);
					    
					    bn_short_nset(&pl_addr.port,port);
    					    bn_int_nset(&pl_addr.ip,addr);
					}
					bn_int_set(&pl_addr.unknown2,0);
					bn_int_set(&pl_addr.unknown3,0);
					packet_append_data(rpacket, &pl_addr, sizeof(pl_addr));

					// local addr
					bn_short_set(&pl_addr.unknown1,2);
					bn_short_nset(&pl_addr.port,conn_get_game_port(o));
					bn_int_set(&pl_addr.ip,anongame_get_addr(oa));
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
				level = account_get_sololevel(conn_get_account(anongame_get_player(ja,i)),ct);
				break;
			case ANONGAME_TYPE_SMALL_FFA:
			case ANONGAME_TYPE_TEAM_FFA:
				level = account_get_ffalevel(conn_get_account(anongame_get_player(ja,i)),ct);
				break;
			case ANONGAME_TYPE_AT_2V2:
			case ANONGAME_TYPE_AT_3V3:
			case ANONGAME_TYPE_AT_4V4:
				acct = conn_get_account(anongame_get_player(ja,i));
				level = account_get_atteamlevel((acct),account_get_currentatteam(acct),ct);
				break;
			case ANONGAME_TYPE_TY:
				level = 0; /* FIXME-TY: WHAT TO DO HERE */
				break;
			default:
				level = account_get_teamlevel(conn_get_account(anongame_get_player(ja,i)),ct);
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
