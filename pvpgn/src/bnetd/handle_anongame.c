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
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <errno.h>
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#ifdef WIN32_GUI
# include <bnetd/winmain.h>
#endif

#include "common/bn_type.h"
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/queue.h"
#include "common/tag.h"
#include "common/list.h"
#include "common/util.h"
#include "connection.h"
#include "account.h"
#include "channel.h"
#include "anongame.h"
#include "anongame_infos.h"
#include "handle_anongame.h"
#include "common/setup_after.h"


static t_tournament_info * tournament_info = NULL;

/* option - handling function */

/* 0x00 */ /* PG style search - handle_anongame_search() in anongame.c */ 
/* 0x01 */ /* server side packet sent from handle_anongame_search() in anongame.c */
/* 0x02 */ static int _client_anongame_infos(t_connection * c, t_packet const * const packet);
/* 0x03 */ static int _client_anongame_cancel(t_connection * c);
/* 0x04 */ static int _client_anongame_profile(t_connection * c, t_packet const * const packet);
/* 0x05 */ /* AT style search - handle_anongame_search() in anongame.c */
/* 0x06 */ /* AT style search (Inviter) handle_anongame_search() in anongame.c */
/* 0x07 */ static int _client_anongame_tournament(t_connection * c, t_packet const * const packet);
/* 0x08 */ /* unknown if it even exists */
/* 0x09 */ static int _client_anongame_get_icon(t_connection * c, t_packet const * const packet);
/* 0x0A */ static int _client_anongame_set_icon(t_connection * c, t_packet const * const packet);

/* misc functions used by _client_anongame_tournament() */
static unsigned int _tournament_time_convert(unsigned int time);
//static unsigned int _tournament_get_starttime(void);
//static unsigned int _tournament_get_endtime(void);

/* and now the functions */
static int _client_anongame_profile(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
    char const * ct = conn_get_clienttag(c);
    char const * username;
    int Count;
    int temp;
    t_account * account; 
    t_connection * dest_c;
    char const * ctag;
    
    Count = bn_int_get(packet->u.client_findanongame.count);
    eventlog(eventlog_level_info,__FUNCTION__,"[%d] got a FINDANONGAME PROFILE packet",conn_get_socket(c));
    
    if (!(username = packet_get_str_const(packet,sizeof(t_client_findanongame_profile),USER_NAME_MAX)))
    {
	eventlog(eventlog_level_error,__FUNCTION__,"[%d] got bad FINDANONGAME_PROFILE (missing or too long username)",conn_get_socket(c));
	return -1;
    }
    
    //If no account is found then break
    if (!(account = accountlist_find_account(username)))
    {				
	eventlog(eventlog_level_error, __FUNCTION__, "Could not get account - PROFILE");
	return -1;
    }										

    if (!(dest_c = connlist_find_connection_by_accountname(username))) {
	eventlog(eventlog_level_error, __FUNCTION__, "account is offline (profile request)");
	return -1;
    }

    eventlog(eventlog_level_info,__FUNCTION__,"Looking up %s's WAR3 Stats.",username);

    ctag = conn_get_clienttag(dest_c);
    if (account_get_sololevel(account,ctag)==0 && account_get_teamlevel(account,ct)==0 && account_get_atteamcount(account,ctag)==0)
    {
	eventlog(eventlog_level_info,__FUNCTION__,"%s does not have WAR3 Stats.",username);
	if (!(rpacket = packet_create(packet_class_bnet)))
	    return -1;
	packet_set_size(rpacket,sizeof(t_server_findanongame_profile2));
	packet_set_type(rpacket,SERVER_FINDANONGAME_PROFILE);
	bn_byte_set(&rpacket->u.server_findanongame_profile2.option,CLIENT_FINDANONGAME_PROFILE);
	bn_int_set(&rpacket->u.server_findanongame_profile2.count,Count);
	bn_int_set(&rpacket->u.server_findanongame_profile2.icon,SERVER_FINDANONGAME_PROFILE_UNKNOWN2);
	bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount,0);
	temp=0;
	packet_append_data(rpacket,&temp,2); 
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
    }
    else // If they do have a profile then:
    {
	int solowins=account_get_solowin(account,ctag); 
	int sololoss=account_get_sololoss(account,ctag);
	int soloxp=account_get_soloxp(account,ctag);
	int sololevel=account_get_sololevel(account,ctag);
	int solorank=account_get_solorank(account,ctag);
	
	int teamwins=account_get_teamwin(account,ctag);
	int teamloss=account_get_teamloss(account,ctag);
	int teamxp=account_get_teamxp(account,ctag);
	int teamlevel=account_get_teamlevel(account,ctag);
	int teamrank=account_get_teamrank(account,ctag);
	
	int ffawins=account_get_ffawin(account,ctag);
	int ffaloss=account_get_ffaloss(account,ctag);
	int ffaxp=account_get_ffaxp(account,ctag);
	int ffalevel=account_get_ffalevel(account,ctag);
	int ffarank=account_get_ffarank(account,ctag);
	
	int tmp2=0;
	int humanwins=account_get_racewin(account,1,ctag);
	int humanlosses=account_get_raceloss(account,1,ctag);
	int orcwins=account_get_racewin(account,2,ctag);
	int orclosses=account_get_raceloss(account,2,ctag);
	int undeadwins=account_get_racewin(account,8,ctag);
	int undeadlosses=account_get_raceloss(account,8,ctag);
	int nightelfwins=account_get_racewin(account,4,ctag);
	int nightelflosses=account_get_raceloss(account,4,ctag);
	int randomwins=account_get_racewin(account,0,ctag);
	int randomlosses=account_get_raceloss(account,0,ctag);
	
	unsigned char rescount;
	
	if (!(rpacket = packet_create(packet_class_bnet)))
	    return -1;
	packet_set_size(rpacket,sizeof(t_server_findanongame_profile2));
	packet_set_type(rpacket,SERVER_FINDANONGAME_PROFILE);
	bn_byte_set(&rpacket->u.server_findanongame_profile2.option,CLIENT_FINDANONGAME_PROFILE);
	bn_int_set(&rpacket->u.server_findanongame_profile2.count,Count); //job count
	if (strcmp(ctag,CLIENTTAG_WARCRAFT3)==0 || strcmp(ctag,CLIENTTAG_WAR3XP)==0) {
	    bn_int_set(&rpacket->u.server_findanongame_profile2.icon,account_icon_to_profile_icon(account_get_user_icon(account,ctag),account,ctag));
	} else {
	    bn_int_set(&rpacket->u.server_findanongame_profile2.icon,account_get_icon_profile(account,ctag));
	}
	
	rescount = 0;
	if (sololevel) {
	    bn_int_set((bn_int*)&temp,0x534F4C4F); // SOLO backwards
	    packet_append_data(rpacket,&temp,4);
	    temp=0;
	    bn_int_set((bn_int*)&temp,solowins);
	    packet_append_data(rpacket,&temp,2); //SOLO WINS
	    bn_int_set((bn_int*)&temp,sololoss);
	    packet_append_data(rpacket,&temp,2); // SOLO LOSSES
	    bn_int_set((bn_int*)&temp,sololevel);
	    packet_append_data(rpacket,&temp,1); // SOLO LEVEL
	    bn_int_set((bn_int*)&temp,account_get_profile_calcs(account,soloxp,sololevel));
	    packet_append_data(rpacket,&temp,1); // SOLO PROFILE CALC
	    bn_int_set((bn_int *)&temp,soloxp);
	    packet_append_data(rpacket,&temp,2); // SOLO XP
	    bn_int_set((bn_int *)&temp,solorank);
	    packet_append_data(rpacket,&temp,4); // SOLO LADDER RANK
	    rescount++;
	}
	
	if (teamlevel) {
	    //below is for team records. Add this after 2v2,3v3,4v4 are done
	    bn_int_set((bn_int*)&temp,0x5445414D); 
	    packet_append_data(rpacket,&temp,4);
	    bn_int_set((bn_int*)&temp,teamwins);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,teamloss);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,teamlevel);
	    packet_append_data(rpacket,&temp,1);
	    bn_int_set((bn_int*)&temp,account_get_profile_calcs(account,teamxp,teamlevel));
	    
	    packet_append_data(rpacket,&temp,1);
	    bn_int_set((bn_int*)&temp,teamxp);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,teamrank);
	    packet_append_data(rpacket,&temp,4);
	    //done of team game stats
	    rescount++;
	}
	//FFA stats
	if (ffalevel) {
	    bn_int_set((bn_int*)&temp,0x46464120);
	    packet_append_data(rpacket,&temp,4);
	    bn_int_set((bn_int*)&temp,ffawins);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,ffaloss);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,ffalevel);
	    packet_append_data(rpacket,&temp,1);
	    bn_int_set((bn_int*)&temp,account_get_profile_calcs(account,ffaxp,ffalevel));
	    packet_append_data(rpacket,&temp,1);
	    bn_int_set((bn_int*)&temp,ffaxp);
	    packet_append_data(rpacket,&temp,2);
	    bn_int_set((bn_int*)&temp,ffarank);
	    packet_append_data(rpacket,&temp,4);
	    //End of FFA Stats
	    rescount++;
	}
	/* set result count */
	bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount,rescount);
	
	bn_int_set((bn_int*)&temp,0x06); //start of race stats
	packet_append_data(rpacket,&temp,1);
	bn_int_set((bn_int*)&temp,randomwins);
	packet_append_data(rpacket,&temp,2); //random wins
	bn_int_set((bn_int*)&temp,randomlosses);
	packet_append_data(rpacket,&temp,2); //random losses
	bn_int_set((bn_int*)&temp,humanwins);
	packet_append_data(rpacket,&temp,2); //human wins
	bn_int_set((bn_int*)&temp,humanlosses);
	packet_append_data(rpacket,&temp,2); //human losses
	bn_int_set((bn_int*)&temp,orcwins);
	packet_append_data(rpacket,&temp,2); //orc wins
	bn_int_set((bn_int*)&temp,orclosses);
	packet_append_data(rpacket,&temp,2); //orc losses
	bn_int_set((bn_int*)&temp,undeadwins);
	packet_append_data(rpacket,&temp,2); //undead wins
	bn_int_set((bn_int*)&temp,undeadlosses);
	packet_append_data(rpacket,&temp,2); //undead losses
	bn_int_set((bn_int*)&temp,nightelfwins);
	packet_append_data(rpacket,&temp,2); //elf wins
	bn_int_set((bn_int*)&temp,nightelflosses);
	packet_append_data(rpacket,&temp,2); //elf losses
	temp=0;
	packet_append_data(rpacket,&temp,4);
	//end of normal stats - Start of AT stats
	temp=account_get_atteamcount(account,ctag);
	if (account_get_atteammembers(account, temp,ctag) == NULL && temp > 0) {
	    temp = temp - 1;
	    eventlog(eventlog_level_error, "handle_bnet_packet", "teammembers is NULL : Decrease atteamcount of 1 !");
	    account_set_atteamcount(account,ctag,temp);
	}

	if (temp > 6) temp = 6; //byte (num of AT teams) 
	
	bn_int_set((bn_int*)&tmp2,temp);
	packet_append_data(rpacket,&tmp2,1); 
	
	if(temp>0)
	{
	    // [quetzal] 20020827 - partially rewritten AT part
	    int i, j, lvl, highest_lvl[6], cnt = account_get_atteamcount(account,ctag);
	    // allocate array for teamlevels
	    int *teamlevels = malloc(cnt * sizeof(int));
	    
	    // populate our array
	    for (i = 0; i < cnt; i++) {
		teamlevels[i] = account_get_atteamlevel(account, i+1,ctag);
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
		eventlog(eventlog_level_debug, __FUNCTION__, 
		    "profile/AT - highest level is %d with index %d", lvl, highest_lvl[j]);
	    }
	    // <-- end of picking indices
	    // 
	    free(teamlevels);
	    
	    for(i = 0; i < j; i++)
	    {
		int n = highest_lvl[i] + 1;
		int teamsize = account_get_atteamsize(account, n, ctag);
		char * teammembers = NULL, *self = NULL, *p2, *p3;
		int teamtype[] = {0, 0x32565332, 0x33565333, 0x34565334, 0x35565335, 0x36565336};
		
		// [quetzal] 20020907 - sometimes we get NULL string, even if
		// teamcount is not 0. that should prevent crash.
		if (!account_get_atteammembers(account, n,ctag) || teamsize < 1 || teamsize > 5)
		    continue;
		
		p2 = p3 = teammembers = strdup(account_get_atteammembers(account, n,ctag));
		
		eventlog(eventlog_level_debug, __FUNCTION__,"profile/AT - processing team %d", n);
		
		bn_int_set((bn_int*)&temp,teamtype[teamsize]);
		packet_append_data(rpacket,&temp,4);
		
		bn_int_set((bn_int*)&temp,account_get_atteamwin(account,n,ctag)); //at team wins
		packet_append_data(rpacket,&temp,2);
		bn_int_set((bn_int*)&temp,account_get_atteamloss(account,n,ctag)); //at team losses
		packet_append_data(rpacket,&temp,2);
		bn_int_set((bn_int*)&temp,account_get_atteamlevel(account,n,ctag)); 
		packet_append_data(rpacket,&temp,1);
		bn_int_set((bn_int*)&temp,account_get_profile_calcs(account,account_get_atteamxp(account,n,ctag),account_get_atteamlevel(account,n,ctag))); // xp bar calc
		packet_append_data(rpacket,&temp,1);
		bn_int_set((bn_int*)&temp,account_get_atteamxp(account,n,ctag));
		packet_append_data(rpacket,&temp,2);
		bn_int_set((bn_int*)&temp,account_get_atteamrank(account,n,ctag)); //rank on AT ladder
		packet_append_data(rpacket,&temp,4);
		temp=0;
		packet_append_data(rpacket,&temp,4); //some unknown packet? random shit
		temp=0;
		packet_append_data(rpacket,&temp,4); //another unknown packet..random shit
		bn_int_set((bn_int*)&temp,teamsize);
		packet_append_data(rpacket,&temp,1);
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
	
	eventlog(eventlog_level_info,__FUNCTION__,"Sent %s's WAR3 Stats to requestor.",username);
    }
    return 0;
}    

static int _client_anongame_cancel(t_connection * c)
{
    t_packet * rpacket = NULL;
    
    // [quetzal] 20020809 - added a_count, so we dont refer to already destroyed anongame
    t_anongame *a = conn_get_anongame(c);
    int a_count;
    
    eventlog(eventlog_level_info,__FUNCTION__,"[%d] got FINDANONGAME CANCEL packet", conn_get_socket(c));
    
    if(!a)
	return -1;
    
    a_count = anongame_get_count(a);
    
    // anongame_unqueue_player(c, anongame_get_type(a)); 
    // -- already doing unqueue in anongame_destroy_anongame
    conn_set_routeconn(c, NULL);
    conn_destroy_anongame(c);
    
    if (!(rpacket = packet_create(packet_class_bnet)))
	return -1;
    
    packet_set_size(rpacket,sizeof(t_server_findanongame_playgame_cancel));
    packet_set_type(rpacket,SERVER_FINDANONGAME_PLAYGAME_CANCEL);
    bn_byte_set(&rpacket->u.server_findanongame_playgame_cancel.cancel,SERVER_FINDANONGAME_CANCEL);
    bn_int_set(&rpacket->u.server_findanongame_playgame_cancel.count, a_count);
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    return 0;
}

static int _client_anongame_get_icon(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
    
    //BlacKDicK 04/20/2003 Need some huge re-work on this.
    {
	struct
	{
	    char	 icon_code[4];
	    unsigned int portrait_code;
	    char	 race;
	    bn_short	 required_wins;
	    char	 client_enabled;
        } tempicon;
        
	//FIXME: Add those to the prefs and also merge them on accoun_wrap;
	// FIXED BY DJP 07/16/2003 FOR 110 CHANGE ( TOURNEY & RACE WINS ) + Table_witdh
        int icon_req_tourney_wins[] = {10,75,150,250,500};
        int icon_req_race_wins[] = {25,150,350,750,1500};
        int race[]={W3_RACE_RANDOM,W3_RACE_HUMANS,W3_RACE_ORCS,W3_RACE_UNDEAD,W3_RACE_NIGHTELVES,W3_ICON_DEMONS};
        char race_char[6] ={'R','H','O','U','N','D'};
        char icon_pos[5] ={'2','3','4','5','6',};
        char table_width = 6;
        char table_height= 5;
        int i,j;
        char rico;
        unsigned int rlvl,rwins;
        
        char user_icon[5];
        char const * uicon;
        eventlog(eventlog_level_info,__FUNCTION__,"[%d] got FINDANONGAME Get Icons packet",conn_get_socket(c));
	
	if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
	    return -1;
	}
	
	packet_set_size(rpacket, sizeof(t_server_findanongame_iconreply));
        packet_set_type(rpacket, SERVER_FINDANONGAME_ICONREPLY);
        bn_int_set(&rpacket->u.server_findanongame_iconreply.count, bn_int_get(packet->u.client_findanongame_inforeq.count));
        bn_byte_set(&rpacket->u.server_findanongame_iconreply.option, CLIENT_FINDANONGAME_GET_ICON);
        if ((uicon = account_get_user_icon(conn_get_account(c),conn_get_clienttag(c))))
        {
	    memcpy(&rpacket->u.server_findanongame_iconreply.curricon, uicon,4);
        }
        else 
        {
	    account_get_raceicon(conn_get_account(c),&rico,&rlvl,&rwins,conn_get_clienttag(c));
	    sprintf(user_icon,"%1d%c3W",rlvl,rico);
            memcpy(&rpacket->u.server_findanongame_iconreply.curricon,user_icon,4);
        }
	
	bn_byte_set(&rpacket->u.server_findanongame_iconreply.table_width, table_width);
        bn_byte_set(&rpacket->u.server_findanongame_iconreply.table_size, table_width*table_height);
        for (j=0;j<table_height;j++){
	    for (i=0;i<table_width;i++){
		tempicon.race=i;
	        tempicon.icon_code[0]=icon_pos[j];
	        tempicon.icon_code[1]=race_char[i];
	        tempicon.icon_code[2]='3';
	        tempicon.icon_code[3]='W';
	        //#ifdef WIN32
	        //tempicon.portrait_code=htonl(account_icon_to_profile_icon(tempicon.icon_code));
	        //#else
	        tempicon.portrait_code=(account_icon_to_profile_icon(tempicon.icon_code,conn_get_account(c),conn_get_clienttag(c)));
	        //#endif
	        if (i<=4){
	    	    //Building the icon for the races
	    	    bn_short_set(&tempicon.required_wins,icon_req_race_wins[j]);
	    	    if (account_get_racewin(conn_get_account(c),race[i],conn_get_clienttag(c))>=icon_req_race_wins[j]) {
			tempicon.client_enabled=1;
	    	    }else{
			tempicon.client_enabled=0;
		    }
	    	}else{
	        //Building the icon for the tourney
	        bn_short_set(&tempicon.required_wins,icon_req_tourney_wins[j]);
	        if (account_get_racewin(conn_get_account(c),race[i],conn_get_clienttag(c))>=icon_req_tourney_wins[j]) {
		    tempicon.client_enabled=1;
	        }else{
		    tempicon.client_enabled=0;}
	        }
	        packet_append_data(rpacket, &tempicon, sizeof(tempicon));
	    }
	}
        //Go,go,go
        queue_push_packet(conn_get_out_queue(c),rpacket);
        packet_del_ref(rpacket);
    }
    return 0;
}

static int _client_anongame_set_icon(t_connection * c, t_packet const * const packet)
{
    //BlacKDicK 04/20/2003
    unsigned int desired_icon;
    char user_icon[5];
    char playerinfo[40];
    char rico;
    unsigned int rlvl,rwin;
    int hll;
    
    /*FIXME: In this case we do not get a 'count' but insted of it we get the icon
    that the client wants to set.'W3H2' for an example. For now it is ok, since they share
    the same position	on the packet*/
    desired_icon=bn_int_get(packet->u.client_findanongame.count);
    user_icon[4]=0;
    if (desired_icon==0){
	strcpy(user_icon,"NULL");
	eventlog(eventlog_level_info,__FUNCTION__,"[%d] Set icon packet to DEFAULT ICON [%4.4s]",conn_get_socket(c),user_icon);
    }else{
	memcpy(user_icon,&desired_icon,4);
	eventlog(eventlog_level_info,__FUNCTION__,"[%d] Set icon packet to ICON [%s]",conn_get_socket(c),user_icon);
    }
    
    account_set_user_icon(conn_get_account(c),conn_get_clienttag(c),user_icon);
    //FIXME: Still need a way to 'refresh the user/channel' 
    //_handle_rejoin_command(conn_get_account(c),""); 
    /* ??? channel_update_flags() */
    hll = account_get_highestladderlevel(conn_get_account(c),conn_get_clienttag(c));
    if (strcmp(user_icon,"NULL")!=0)
	sprintf(playerinfo,"PX3W %s %u",user_icon,hll);
    else 
    {
	account_get_raceicon(conn_get_account(c),&rico,&rlvl,&rwin,conn_get_clienttag(c));
	sprintf(playerinfo,"PX3W %1d%c3W %u",rlvl,rico,hll);
    }
    
    eventlog(eventlog_level_info,__FUNCTION__,"playerinfo: %s",playerinfo);
    conn_set_w3_playerinfo(c,playerinfo);
    channel_rejoin(c);
    return 0;
}

static int _client_anongame_infos(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
    
    if (bn_int_get(packet->u.client_findanongame_inforeq.count) > 1) {
	/* reply with 0 entries found */
	int	temp = 0;
	
	if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
	    return -1;
	}
	
	packet_set_size(rpacket, sizeof(t_server_findanongame_inforeply));
	packet_set_type(rpacket, SERVER_FINDANONGAME_INFOREPLY);
	bn_byte_set(&rpacket->u.server_findanongame_inforeply.option, CLIENT_FINDANONGAME_INFOS);
	bn_int_set(&rpacket->u.server_findanongame_inforeply.count, bn_int_get(packet->u.client_findanongame_inforeq.count));
	bn_byte_set(&rpacket->u.server_findanongame_inforeply.noitems, 0);
	packet_append_data(rpacket, &temp, 1);
	
	queue_push_packet(conn_get_out_queue(c),rpacket);
	packet_del_ref(rpacket);
    } else { /* FIXME: this needs to be done properly */
	//BlacKDicK 04/02/2003
	int i;
	bn_int temp;
	int client_tag;
	int client_tag_unk;
	int server_tag_unk;
	char ladr_count=0;
	char server_tag_count=0;
	char noitems=0;
	char desc_count=0;
	char mapscount_1v1;
	char mapscount_2v2;
	char mapscount_3v3;
	char mapscount_4v4;
	char mapscount_sffa;
	char mapscount_at2v2;
	char mapscount_at3v3;
	char mapscount_at4v4;
	char mapscount_TY;
	char mapscount_total;
	char value;
	char PG_gamestyles, AT_gamestyles, TY_gamestyles;
	int counter1, counter2;
	char const * clienttag = conn_get_clienttag(c);
	
	// value changes according to Fr3DBr. the 3rd value is the allowed thumbs down count
	// maybe make it configurable via anongame_infos.conf later on...
	char anongame_PG_1v1_prefix[]  = {0x00, 0x00, 0x03, 0x3F, 0x00};
	char anongame_PG_2v2_prefix[]  = {0x01, 0x00, 0x02, 0x3F, 0x00};
	char anongame_PG_3v3_prefix[]  = {0x02, 0x00, 0x01, 0x3F, 0x00};
	char anongame_PG_4v4_prefix[]  = {0x03, 0x00, 0x01, 0x3F, 0x00};
	char anongame_PG_sffa_prefix[] = {0x04, 0x00, 0x02, 0x3F, 0x00};
	
	char anongame_AT_2v2_prefix[]  = {0x00, 0x00, 0x02, 0x3F, 0x02};
	char anongame_AT_3v3_prefix[]  = {0x02, 0x00, 0x02, 0x3F, 0x03};
	char anongame_AT_4v4_prefix[]  = {0x03, 0x00, 0x02, 0x3F, 0x04};
	
	char anongame_TY_prefix[]  = {0x00, 0x00, 0x00, 0x3F, 0x00};

	char anongame_PG_section = 0x00;
	char anongame_AT_section = 0x01;
	char anongame_TY_section = 0x02;
	
	char last_packet	= 0x00;
	char other_packet	= 0x01;
	
	anongame_PG_1v1_prefix[2] = anongame_infos_THUMBSDOWN_get_PG_1v1();
	anongame_PG_2v2_prefix[2] = anongame_infos_THUMBSDOWN_get_PG_2v2();
	anongame_PG_3v3_prefix[2] = anongame_infos_THUMBSDOWN_get_PG_3v3();
	anongame_PG_4v4_prefix[2] = anongame_infos_THUMBSDOWN_get_PG_4v4();
	anongame_PG_sffa_prefix[2]= anongame_infos_THUMBSDOWN_get_PG_ffa();
	
	anongame_AT_2v2_prefix[2] = anongame_infos_THUMBSDOWN_get_AT_2v2();
	anongame_AT_3v3_prefix[2] = anongame_infos_THUMBSDOWN_get_AT_3v3();
	anongame_AT_4v4_prefix[2] = anongame_infos_THUMBSDOWN_get_AT_4v4();
	
	anongame_TY_prefix[2] = tournament_info->thumbs_down;

	/* last value = number of players per team
	 * 0 = PG , 2 = AT 2v2 , 3 = AT 3v3 , 4 = AT 4v4
	 * needs to be set for TY, for team selection screen to appear on team games */
	if (tournament_info->game_selection == 2)
	    anongame_TY_prefix[4] = tournament_info->game_type;
	else 
	    anongame_TY_prefix[4] = 0;
	
	mapscount_1v1  = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_1V1, clienttag));
	mapscount_2v2  = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_2V2, clienttag));
	mapscount_3v3  = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_3V3, clienttag));
	mapscount_4v4  = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_4V4, clienttag));
	mapscount_sffa = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_SMALL_FFA, clienttag));
	mapscount_at2v2 = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_AT_2V2, clienttag));
	mapscount_at3v3 = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_AT_3V3, clienttag));
	mapscount_at4v4 = list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_AT_4V4, clienttag));
	mapscount_TY	= list_get_length(anongame_get_w3xp_maplist(ANONGAME_TYPE_TY, clienttag));
	mapscount_total = mapscount_1v1 + mapscount_2v2 + mapscount_3v3 + mapscount_4v4 + mapscount_sffa +mapscount_at2v2 + mapscount_at3v3 + mapscount_at4v4 + mapscount_TY;
	
	eventlog(eventlog_level_debug,__FUNCTION__,"client_findanongame_inforeq.noitems=(0x%01x)",bn_byte_get(packet->u.client_findanongame_inforeq.noitems));
	
	/* Send seperate packet for each item requested
	 * sending all at once overloaded w3xp
	 * [Omega] */
	for (i=0;i<bn_byte_get(packet->u.client_findanongame_inforeq.noitems);i++){
	    noitems = 0;
	    
	    if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
		eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
		return -1;
	    }
	    
	    /* Starting the packet stuff */
	    packet_set_size(rpacket, sizeof(t_server_findanongame_inforeply));
	    packet_set_type(rpacket, SERVER_FINDANONGAME_INFOREPLY);
	    bn_byte_set(&rpacket->u.server_findanongame_inforeply.option, CLIENT_FINDANONGAME_INFOS);
	    bn_int_set(&rpacket->u.server_findanongame_inforeply.count, 1);
	    
	    memcpy(&temp,(packet_get_data_const(packet,10+(i*8),4)),sizeof(int));
	    client_tag=bn_int_get(temp);
	    memcpy(&temp,packet_get_data_const(packet,14+(i*8),4),sizeof(int));
	    client_tag_unk=bn_int_get(temp);
	    
	    switch (client_tag){
		case CLIENT_FINDANONGAME_INFOTAG_URL:
		    server_tag_unk=0xBF1F1047;
		    packet_append_data(rpacket, "LRU\0" , 4);
		    packet_append_data(rpacket, &server_tag_unk , 4);
		    // FIXME: Maybe need do do some checks to avoid prefs empty strings.
		    packet_append_string(rpacket, anongame_infos_URL_get_server_url());
		    packet_append_string(rpacket, anongame_infos_URL_get_player_url());
		    packet_append_string(rpacket, anongame_infos_URL_get_tourney_url());
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s)  tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_URL",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_MAP:
		    server_tag_unk=0x70E2E0D5;
		    packet_append_data(rpacket, "PAM\0" , 4);
		    packet_append_data(rpacket, &server_tag_unk , 4);
		    packet_append_data(rpacket, &mapscount_total, 1);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_1V1, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_2V2, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_3V3, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_4V4, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_SMALL_FFA, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_AT_2V2, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_AT_3V3, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_AT_4V4, clienttag);
		    anongame_add_maps_to_packet(rpacket, ANONGAME_TYPE_TY, clienttag);
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s)  tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_MAP",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_TYPE:
		    server_tag_unk=0x7C87DEEE;
		    packet_append_data(rpacket, "EPYT" , 4);
		    packet_append_data(rpacket, &server_tag_unk , 4);
		    value = 3; /* probably count of gametypes (PG,AT) */ /* added TY */
		    packet_append_data(rpacket, &value, 1);
		    packet_append_data(rpacket, &anongame_PG_section,1);
		    
		    PG_gamestyles = 0;
		    if (mapscount_1v1) {
			PG_gamestyles++;
			if (mapscount_2v2) {
			    PG_gamestyles++;
			    if (mapscount_3v3) {
				PG_gamestyles++;
				if (mapscount_4v4) {
				    PG_gamestyles++;
				    if (mapscount_sffa)
					PG_gamestyles++;
				}
			    }
			}
		    }
		    
		    packet_append_data(rpacket, &PG_gamestyles,1);
		    
		    counter2 = 0;
		    		
		    if (mapscount_1v1)
		    {
			packet_append_data(rpacket, &anongame_PG_1v1_prefix,5);
			packet_append_data(rpacket, &mapscount_1v1,1);
			for (counter1=counter2; counter1<(counter2+mapscount_1v1);counter1++)
			    packet_append_data(rpacket,&counter1,1);
			counter2+=mapscount_1v1;
				  
			if (mapscount_2v2)
			{
			    packet_append_data(rpacket, &anongame_PG_2v2_prefix,5);
			    packet_append_data(rpacket, &mapscount_2v2,1);
			    for (counter1=counter2; counter1<(counter2+mapscount_2v2);counter1++)
				packet_append_data(rpacket,&counter1,1);
			    counter2+=mapscount_2v2;
			
			    if (mapscount_3v3)
			    {
				packet_append_data(rpacket, &anongame_PG_3v3_prefix,5);
				packet_append_data(rpacket, &mapscount_3v3,1);
				for (counter1=counter2; counter1<(counter2+mapscount_3v3);counter1++)
				    packet_append_data(rpacket,&counter1,1);
				counter2+=mapscount_3v3;
				
				if (mapscount_4v4)
				{
				    packet_append_data(rpacket, &anongame_PG_4v4_prefix,5);
				    packet_append_data(rpacket, &mapscount_4v4,1);
				    for (counter1=counter2; counter1<(counter2+mapscount_4v4);counter1++)
					packet_append_data(rpacket,&counter1,1);
				    counter2+=mapscount_4v4;
				    
				    if (mapscount_sffa)
				    {
					packet_append_data(rpacket, &anongame_PG_sffa_prefix,5);
					packet_append_data(rpacket, &mapscount_sffa,1);
					for (counter1=counter2; counter1<(counter2+mapscount_sffa);counter1++)
					    packet_append_data(rpacket,&counter1,1);
					counter2+=mapscount_sffa;
				    }
				    else
					eventlog(eventlog_level_error,__FUNCTION__,"found no sffa PG maps in bnmaps.txt - this will disturb anongameinfo packet creation");
				}
				else
				    eventlog(eventlog_level_error,__FUNCTION__,"found no 4v4 PG maps in bnmaps.txt - this will disturb anongameinfo packet creation");
			    }
			    else
				eventlog(eventlog_level_error,__FUNCTION__,"found no 3v3 PG maps in bnmaps.txt - this will disturb anongameinfo packet creation");
			}
			else
			    eventlog(eventlog_level_error,__FUNCTION__,"found no 2v2 PG maps in bnmaps.txt - this will disturb anongameinfo packet creation");
		    }
		    else
			eventlog(eventlog_level_error,__FUNCTION__,"found no 1v1 PG maps in bnmaps.txt - this will disturb anongameinfo packet creation");
		    
		    packet_append_data(rpacket,&anongame_AT_section,1);
		    
		    AT_gamestyles = 0;
		    if (mapscount_at2v2) {
			AT_gamestyles++;
			if (mapscount_at3v3) {
			    AT_gamestyles++;
			    if (mapscount_at4v4) {
				AT_gamestyles++;
			    }
			}
		    }
		    
		    packet_append_data(rpacket,&AT_gamestyles,1);
		    
		    if (mapscount_at2v2)
		    {
			packet_append_data(rpacket, &anongame_AT_2v2_prefix,5);
			packet_append_data(rpacket, &mapscount_at2v2,1);
			for (counter1=counter2; counter1<(counter2+mapscount_at2v2);counter1++)
			    packet_append_data(rpacket,&counter1,1);
			counter2+=mapscount_at2v2;
			
			if (mapscount_at3v3)
			{
			    packet_append_data(rpacket, &anongame_AT_3v3_prefix,5);
			    packet_append_data(rpacket, &mapscount_at3v3,1);
			    for (counter1=counter2; counter1<(counter2+mapscount_at3v3);counter1++)
			        packet_append_data(rpacket,&counter1,1);
			    counter2+=mapscount_at3v3;
			    
			    if (mapscount_at4v4)
			    {
			        packet_append_data(rpacket, &anongame_AT_4v4_prefix,5);
			        packet_append_data(rpacket, &mapscount_at4v4,1);
			        for (counter1=counter2; counter1<(counter2+mapscount_at4v4);counter1++)
			    	    packet_append_data(rpacket,&counter1,1);
			        counter2+=mapscount_at4v4;
			    }
			    else
				eventlog(eventlog_level_error,__FUNCTION__,"found no 4v4 AT maps in bnmaps.txt - this will disturb anongameinfo packet creation");
			}
			else
			    eventlog(eventlog_level_error,__FUNCTION__,"found no 3v3 AT maps in bnmaps.txt - this will disturb anongameinfo packet creation");
		    }
		    else
			eventlog(eventlog_level_error,__FUNCTION__,"found no 2v2 AT maps in bnmaps.txt - this will disturb anongameinfo packet creation");
		    
		    /* TY game types */
		    packet_append_data(rpacket, &anongame_TY_section,1);
		    
		    TY_gamestyles = 0;
		    
		    if (mapscount_TY)
			TY_gamestyles++;
		    
		    packet_append_data(rpacket, &TY_gamestyles,1);
		    
		    if (mapscount_TY) {
			packet_append_data(rpacket, &anongame_TY_prefix,5);
			packet_append_data(rpacket, &mapscount_TY,1);
			for (counter1=counter2; counter1<(counter2+mapscount_TY);counter1++)
			    packet_append_data(rpacket,&counter1,1);
			counter2+=mapscount_TY;
		    }
		    else
			eventlog(eventlog_level_error,__FUNCTION__,"found no TY maps in bnmaps.txt - this will disturb anongameinfo packet creation");
		    /* end of TY types */	
		    
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_TYPE",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_DESC:
		    server_tag_unk=0xA4F0A22F;
		    packet_append_data(rpacket, "CSED" , 4);
		    packet_append_data(rpacket,&server_tag_unk,4);
		    // total descriptions
		    desc_count=9; /* PG = 5 , AT = 3 , TY = 1 , Total = 9 */
                    packet_append_data(rpacket,&desc_count,1);
		    // PG description section
		    desc_count=0;
		    packet_append_data(rpacket,&anongame_PG_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_1v1_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_1v1_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_PG_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_2v2_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_2v2_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_PG_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_3v3_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_3v3_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_PG_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_4v4_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_4v4_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_PG_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_ffa_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_ffa_long((char *)conn_get_country(c)));
		    // AT description section
		    desc_count = 0;
		    packet_append_data(rpacket,&anongame_AT_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_2v2_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_2v2_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_AT_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_3v3_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_3v3_long((char *)conn_get_country(c)));
		    packet_append_data(rpacket,&anongame_AT_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_4v4_short((char *)conn_get_country(c)));
		    packet_append_string(rpacket,anongame_infos_DESC_get_gametype_4v4_long((char *)conn_get_country(c)));
		    
//		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_DESC",client_tag_unk);
		    
		    /* start of info for tournament info */
//		    server_tag_unk=0x9023A85A;
//		    packet_append_data(rpacket, "CSED" , 4);
//		    packet_append_data(rpacket,&server_tag_unk,4);
		    /* total descriptions */
//		    desc_count=1;
//		    packet_append_data(rpacket,&desc_count,1);
		    /* TY description section */
		    desc_count=0;
		    packet_append_data(rpacket,&anongame_TY_section,1);
		    packet_append_data(rpacket,&desc_count,1);
		    desc_count++;
		    packet_append_string(rpacket,tournament_info->format);
		    packet_append_string(rpacket,tournament_info->sponsor);
		    
		    noitems++;
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_LADR:
		    server_tag_unk=0x3BADE25A;
		    packet_append_data(rpacket, "RDAL" , 4);
		    packet_append_data(rpacket, &server_tag_unk , 4);
		    /*FIXME: Still adding a static number (5)
		    Also maybe need do do some checks to avoid prefs empty strings.*/
		    ladr_count=6;
		    packet_append_data(rpacket, &ladr_count, 1);
		    packet_append_data(rpacket, "OLOS", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_PG_1v1_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_PG_1v1_url());
		    packet_append_data(rpacket, "MAET", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_PG_team_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_PG_team_url());
		    packet_append_data(rpacket, " AFF", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_PG_ffa_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_PG_ffa_url());
		    packet_append_data(rpacket, "2SV2", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_AT_2v2_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_AT_2v2_url());
		    packet_append_data(rpacket, "3SV3", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_AT_3v3_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_AT_3v3_url());
		    packet_append_data(rpacket, "4SV4", 4);
		    packet_append_string(rpacket, anongame_infos_DESC_get_ladder_AT_4v4_desc((char *)conn_get_country(c)));
		    packet_append_string(rpacket, anongame_infos_URL_get_ladder_AT_4v4_url());
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_LADR",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_SOLO:
		    // Do nothing.BNET Server W3XP 305b doesn't answer to this request
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_SOLO",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_TEAM:
		    // Do nothing.BNET Server W3XP 305b doesn't answer to this request
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_TEAM",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_FFA:
		    // Do nothing.BNET Server W3XP 305b doesn't answer to this request
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s)  tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_FFA",client_tag_unk);
		    break;
		default:
		     eventlog(eventlog_level_debug,__FUNCTION__,"unrec client_tag request tagid=(0x%01x) tag=(0x%04x)",i,client_tag);
		
	    }
	    //Adding a last padding null-byte
	    if (server_tag_count == bn_byte_get(packet->u.client_findanongame_inforeq.noitems))
		packet_append_data(rpacket, &last_packet, 1); /* only last packet in group gets 0x00 */
	    else
		packet_append_data(rpacket, &other_packet, 1); /* the rest get 0x01 */
		
	    //Go,go,go
	    bn_byte_set(&rpacket->u.server_findanongame_inforeply.noitems, noitems);
	    packet_set_size(rpacket,packet_get_size(rpacket));
	    queue_push_packet(conn_get_out_queue(c),rpacket);
	    packet_del_ref(rpacket);
	}
    }
    return 0;
}

/* tournament notice disabled at this time, but responce is sent to cleint */
static int _client_anongame_tournament(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
    
    unsigned int now		= time(NULL);
    unsigned int start_prelim	= tournament_info->start_preliminary;
    unsigned int end_signup	= tournament_info->end_signup;
    unsigned int end_prelim	= tournament_info->end_preliminary;
    unsigned int start_r1	= tournament_info->start_round_1;
    
    if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
	return -1;
    }
    
    packet_set_size(rpacket, sizeof(t_server_anongame_tournament_reply));
    packet_set_type(rpacket, SERVER_FINDANONGAME_TOURNAMENT_REPLY);
    bn_byte_set(&rpacket->u.server_anongame_tournament_reply.option, 7);
    bn_int_set(&rpacket->u.server_anongame_tournament_reply.count,
    bn_int_get(packet->u.client_anongame_tournament_request.count));
    
    /* (1) forces tournament to be disabled */
    if ( (1) || !start_prelim || (end_signup <= now && (0) /* check if user has signed up */ )) { /* No Tournament Notice */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0);
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (start_prelim>=now) { /* Tournament Notice */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		1);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x25F4); /* random */
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(start_prelim));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		start_prelim-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x00);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (end_signup>=now) { /* Tournament Signup Notice - Play Game Active */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0828); /* random */
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(end_signup));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		end_signup-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		0); /* FIXME-TY: set wins */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0); /* FIXME-TY: set losses */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0); /* FIXME-TY: set ties */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x08);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (end_prelim>=now) { /* Tournament Prelim Period - Play Game Active */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		3);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0828); /* random */
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(end_prelim));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		end_prelim-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		0); /* FIXME-TY: set wins */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0); /* FIXME-TY: set losses */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0); /* FIXME-TY: set ties */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x08);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (start_r1>=now && (0) /* check if all prelim games are done */ ) { /* Prelim Period Over - Shows user stats (not all prelim games finished) */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		4);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0000); /* random */
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(start_r1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		start_r1-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0); /* 00 00 */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		7); /* FIXME-TY: set wins */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		1); /* FIXME-TY: set losses */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		2); /* FIXME-TY: set ties */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x08);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if ( (0) /* check if user made finals */ ) { /* Prelim Period Over - user did not make finals - Shows user stats */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		5);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0);
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		7); /* FIXME-TY: set wins */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		1); /* FIXME-TY: set losses */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		2); /* FIXME-TY: set ties */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x04);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    /* cycle through [type-6] & [type-7] packets
     *
     * use [type-6] to show client "eliminated" or "continue"
     *     timestamp , countdown & round number (of next round) must be set if clinet continues
     *
     * use [type-7] to make cleint wait for 44FF packet option 1 to start game (A guess, not tested)
     *
     * not sure if there is overall winner packet sent at end of last final round
     */
    
    else if ( (0) ) { /* User in finals - Shows user stats and start of next round*/
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		6);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0000);
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(start_r1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		start_r1-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0x0000); /* 00 00 */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		4); /* round number */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0); /* 0 = continue , 1= eliminated */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x04); /* number of rounds in finals */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if ( (0) ) { /* user waiting for match to be made */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		7);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0);
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		1); /* round number */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x04); /* number of finals */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    
    queue_push_packet(conn_get_out_queue(c),rpacket);
    packet_del_ref(rpacket);
    return 0;
}

static unsigned int _tournament_time_convert(unsigned int time)
{
    /* it works, don't ask me how */
    unsigned int tmp1, tmp2, tmp3;
    
    tmp1 = time-1059179400;	/* 0x3F21CB88  */
    tmp2 = tmp1*1073/1800;	/* 0x431/0x708 */
    tmp3 = tmp2+3276999948;	/* 0xC3530D0C  */
/*  eventlog(eventlog_level_trace,__FUNCTION__,"time: 0x%08x, tmp1: 0x%08x, tmp2 0x%08x, tmp3 0x%08x",time,tmp1,tmp2,tmp3); */

    return tmp3;
}

extern int handle_anongame_packet(t_connection * c, t_packet const * const packet)
{
    if (bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_PROFILE)
	return _client_anongame_profile(c, packet);

    if (bn_byte_get(packet->u.client_anongame.option) == CLIENT_FINDANONGAME_CANCEL) 
	return _client_anongame_cancel(c);
	
    if (bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_SEARCH ||
	    bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_AT_INVITER_SEARCH ||
	    bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_AT_SEARCH)
	return handle_anongame_search(c, packet); /* located in anongame.c */

    if (bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_GET_ICON)
	return _client_anongame_get_icon(c, packet);
    
    if (bn_byte_get(packet->u.client_anongame.option)==CLIENT_FINDANONGAME_SET_ICON)
	return _client_anongame_set_icon(c, packet);

    if (bn_byte_get(packet->u.client_anongame.option) == CLIENT_FINDANONGAME_INFOS)
	return _client_anongame_infos(c, packet);
    
    if (bn_byte_get(packet->u.client_anongame.option)==CLIENT_ANONGAME_TOURNAMENT)
	return _client_anongame_tournament(c, packet);
    
    eventlog(eventlog_level_error,__FUNCTION__,"got unhandled option %d",bn_byte_get(packet->u.client_findanongame.option));
    return -1;
}

extern int tournament_is_arranged(void)
{
    if (tournament_info->game_selection == 2)
	return 1;
    else
	return 0;
}

extern int tournament_get_totalplayers(void)
{
    return tournament_info->game_type * 2;
}

extern int tournament_init(char const * filename)
{
    FILE * fp;
    unsigned int line,pos,mon,day,year,hour,min,sec;
    char *buff,*temp,*variable,*pointer,*value;
    char format[30];
    char *sponsor = NULL;
    char *have_sponsor = NULL;
    char *have_icon = NULL;
    struct tm * timestamp = malloc(sizeof(struct tm));
    
    sprintf(format,"%%02u/%%02u/%%04u %%02u:%%02u:%%02u");

    tournament_info = malloc(sizeof(t_tournament_info));
    tournament_info->start_preliminary	= 0;
    tournament_info->end_signup		= 0;
    tournament_info->end_preliminary	= 0;
    tournament_info->start_round_1	= 0;
    tournament_info->start_round_2	= 0;
    tournament_info->start_round_3	= 0;
    tournament_info->start_round_4	= 0;
    tournament_info->tournament_end	= 0;
    tournament_info->game_selection	= 1;
    tournament_info->game_type		= 1;
    tournament_info->format		= strdup("");
    tournament_info->sponsor		= strdup("");
    tournament_info->thumbs_down	= 0;
    
    if (!filename) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
        free((void *)timestamp);
	return -1;
    }
    
    if (!(fp = fopen(filename,"r"))) {
	eventlog(eventlog_level_error,__FUNCTION__,"could not open file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	free((void *)timestamp);
	return -1;
    }
    
    for (line=1; (buff = file_get_line(fp)); line++) {
	for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
	if (buff[pos]=='\0' || buff[pos]=='#') {
	    free(buff);
	    continue;
	}
	if ((temp = strrchr(buff,'#'))) {
	    unsigned int len;
	    unsigned int endpos;
	    
	    *temp = '\0';
	    len = strlen(buff)+1;
	    for (endpos=len-1; buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
	    buff[endpos+1] = '\0';
	}
	
	variable = buff;
	pointer = strchr(variable,'=');
	for(pointer--;pointer[0]==' ' || pointer[0]=='\t';pointer--);
	pointer[1]='\0';
	pointer++;
	pointer++;
	pointer = strchr(pointer,'=');
	pointer++;
	
	if (strcmp(variable,"start_preliminary") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;
	    
	    tournament_info->start_preliminary = mktime(timestamp);
	}
	else if (strcmp(variable,"end_signup") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->end_signup = mktime(timestamp);
	}
	else if (strcmp(variable,"end_preliminary") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->end_preliminary = mktime(timestamp);
	}
	else if (strcmp(variable,"start_round_1") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->start_round_1 = mktime(timestamp);
	}
	else if (strcmp(variable,"start_round_2") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->start_round_2 = mktime(timestamp);
	}
	else if (strcmp(variable,"start_round_3") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->start_round_3 = mktime(timestamp);
	}
	else if (strcmp(variable,"start_round_4") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->start_round_4 = mktime(timestamp);
	}
	else if (strcmp(variable,"tournament_end") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    sscanf(value,format,&mon,&day,&year,&hour,&min,&sec);
	    
	    timestamp->tm_mon	= mon-1;
	    timestamp->tm_mday	= day;
	    timestamp->tm_year	= year-1900;
	    timestamp->tm_hour	= hour;
	    timestamp->tm_min	= min;
	    timestamp->tm_sec	= sec;
	    timestamp->tm_isdst	= -1;

	    tournament_info->tournament_end = mktime(timestamp);
	}
	else if (strcmp(variable,"game_selection") == 0) {
	    tournament_info->game_selection = atoi(pointer);
	}
	else if (strcmp(variable,"game_type") == 0) {
	    tournament_info->game_type = atoi(pointer);
	}
	else if (strcmp(variable,"format") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';
	    
	    if (tournament_info->format) free((void *)tournament_info->format);
	    tournament_info->format = strdup(value);
	}
	else if (strcmp(variable,"sponsor") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';

	    have_sponsor = strdup(value);
	}
	else if (strcmp(variable,"icon") == 0) {
	    pointer = strchr(pointer,'\"');
	    pointer++;
	    value = pointer;
	    pointer = strchr(pointer,'\"');
	    pointer[0]='\0';

	    have_icon = strdup(value);
	}
	else if (strcmp(variable,"thumbs_down") == 0) {
	    tournament_info->thumbs_down = atoi(pointer);
	}
	else
	    eventlog(eventlog_level_error,__FUNCTION__,"bad option \"%s\" in \"%s\"",variable,filename);
	
	if (have_sponsor && have_icon) {
	    sponsor = malloc(strlen(have_sponsor)+6);
	    sprintf(sponsor, "%c%c3W,%s",have_icon[1],have_icon[0],have_sponsor);
	    if (tournament_info->sponsor) free((void *)tournament_info->sponsor);
	    tournament_info->sponsor = strdup(sponsor);
	    free((void *)have_sponsor);
	    free((void *)have_icon);
	    free((void *)sponsor);
	    have_sponsor = NULL;
	    have_icon = NULL;
	}
	free(buff);
    }
    if (have_sponsor) free((void *)have_sponsor);
    if (have_icon) free((void *)have_icon);
    free((void *)timestamp);
    fclose(fp);
    
    /* check if we have timestamps for all the times */ 
    /* if not disable tournament by setting "start_preliminary" to 0 */
    if (tournament_info->end_signup	 == 0 ||
	tournament_info->end_preliminary == 0 ||
	tournament_info->start_round_1	 == 0 ||
	tournament_info->start_round_2	 == 0 ||
	tournament_info->start_round_3	 == 0 ||
	tournament_info->start_round_4	 == 0 ||
	tournament_info->tournament_end	 == 0)
	{
	    tournament_info->start_preliminary = 0;
	    eventlog(eventlog_level_warn,__FUNCTION__,"one or more timestamps for tournaments is not valid, tournament has been disabled");
	}
    
    return 0;
}

extern int tournament_destroy(void)
{
    if (tournament_info->format) free((void *)tournament_info->format);
    if (tournament_info->sponsor) free((void *)tournament_info->sponsor);
    if (tournament_info) free((void *)tournament_info);
    tournament_info = NULL;
    return 0;
}
