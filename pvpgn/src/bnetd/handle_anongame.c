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
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
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
#include "anongame_maplists.h"
#include "handle_anongame.h"
#include "tournament.h"
#include "common/setup_after.h"

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

/* and now the functions */
static int _client_anongame_profile(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
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
	eventlog(eventlog_level_debug, __FUNCTION__, "account is offline -  try ll_clienttag");
	if (!(ctag = account_get_ll_clienttag(account))) return -1;
    }
    else
    ctag = conn_get_clienttag(dest_c);

    eventlog(eventlog_level_info,__FUNCTION__,"Looking up %s's WAR3 Stats.",username);

    if (account_get_sololevel(account,ctag)<=0 && account_get_teamlevel(account,ctag)<=0 && account_get_atteamcount(account,ctag)<=0)
    {
	eventlog(eventlog_level_info,__FUNCTION__,"%s does not have WAR3 Stats.",username);
	if (!(rpacket = packet_create(packet_class_bnet)))
	    return -1;
	packet_set_size(rpacket,sizeof(t_server_findanongame_profile2));
	packet_set_type(rpacket,SERVER_FINDANONGAME_PROFILE);
	bn_byte_set(&rpacket->u.server_findanongame_profile2.option,CLIENT_FINDANONGAME_PROFILE);
	bn_int_set(&rpacket->u.server_findanongame_profile2.count,Count);
	bn_int_set(&rpacket->u.server_findanongame_profile2.icon,account_icon_to_profile_icon(account_get_user_icon(account,ctag),account,ctag));
	bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount,0);
	temp=0;
	packet_append_data(rpacket,&temp,2); 
	conn_push_outqueue(c,rpacket);
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
	bn_int_set(&rpacket->u.server_findanongame_profile2.icon,account_icon_to_profile_icon(account_get_user_icon(account,ctag),account,ctag));

	rescount = 0;
	if (sololevel > 0) {
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

	if (teamlevel > 0) {
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

	if (ffalevel > 0) {
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

	/* 1 byte team count place holder, set later */
	packet_append_data(rpacket, &temp, 1);

	temp=account_get_atteamcount(account,ctag);
	if(temp>0)
	{
	    /* [quetzal] 20020827 - partially rewritten AT part */
	    int i, j, lvl, highest_lvl[6], cnt;
	    int invalid;
	    /* allocate array for teamlevels */
	    int *teamlevels;
	    unsigned char *atcountp;

	    cnt = temp;
	    teamlevels = malloc(cnt * sizeof(int));

	    /* we need to store the AT team count but we dont know yet the no
	     * of corectly stored teams so we cache the pointer for later use 
	     */
	    atcountp = (unsigned char *)packet_get_raw_data(rpacket, packet_get_size(rpacket) - 1);

	    /* populate our array */
	    for (i = 0; i < cnt; i++)
		if ((teamlevels[i] = account_get_atteamlevel(account, i+1,ctag)) < 0)
		    teamlevels[i] = 0;

	    /* now lets pick indices of 6 highest levels */
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

	    free((void*)teamlevels);

	    cnt = 0;
	    invalid = 0;
	    for(i = 0; i < j; i++)
	    {
		int n;
		int teamsize;
		char * teammembers = NULL, *self = NULL, *p2, *p3;
		int teamtype[] = {0, 0x32565332, 0x33565333, 0x34565334, 0x35565335, 0x36565336};
		
		n = highest_lvl[i] + 1;
		teamsize = account_get_atteamsize(account, n, ctag);

		teammembers = (char *)account_get_atteammembers(account, n,ctag);
		if (!teammembers || teamsize < 1 || teamsize > 5) {
		    eventlog(eventlog_level_warn, __FUNCTION__, "skipping invalid AT (members: '%s' teamsize: %d)", teammembers ? teammembers : "NULL", teamsize);
		    invalid = 1;
		    continue;
		}

		p2 = p3 = teammembers = strdup(teammembers);
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

		free((void *)teammembers);
		cnt++;
	    }

	    if (!cnt) {
		eventlog(eventlog_level_warn, __FUNCTION__, "no valid team found, sending bogus team");
		bn_int_set((bn_int*)&temp, 0x32565332);
		packet_append_data(rpacket,&temp,4);

		temp = 0;
		packet_append_data(rpacket,&temp,2);
		packet_append_data(rpacket,&temp,2);
		packet_append_data(rpacket,&temp,1);
		packet_append_data(rpacket,&temp,1);
		packet_append_data(rpacket,&temp,2);
		packet_append_data(rpacket,&temp,4);
		packet_append_data(rpacket,&temp,4);
		packet_append_data(rpacket,&temp,4);
		bn_int_set((bn_int*)&temp, 1);
		packet_append_data(rpacket,&temp,1);
		packet_append_string(rpacket,"error");
		cnt++;
	    }
	    *atcountp = (unsigned char)cnt;

	    if (invalid) account_fix_at(account, ctag);
	}

	conn_push_outqueue(c,rpacket);
	packet_del_ref(rpacket);				
	
	eventlog(eventlog_level_info,__FUNCTION__,"Sent %s's WAR3 Stats to requestor.",username);
    }
    return 0;
}    

static int _client_anongame_cancel(t_connection * c)
{
    t_packet * rpacket = NULL;
    t_connection * tc[ANONGAME_MAX_GAMECOUNT/2];
    
    // [quetzal] 20020809 - added a_count, so we dont refer to already destroyed anongame
    t_anongame *a = conn_get_anongame(c);
    int a_count, i;
    
    eventlog(eventlog_level_info,__FUNCTION__,"[%d] got FINDANONGAME CANCEL packet", conn_get_socket(c));
    
    if(!a)
	return -1;
    
    a_count = anongame_get_count(a);
    
    // anongame_unqueue(c, anongame_get_queue(a)); 
    // -- already doing unqueue in conn_destroy_anongame
    for (i=0; i < ANONGAME_MAX_GAMECOUNT/2; i++)
	tc[i] = anongame_get_tc(a, i);
	
    for (i=0; i < ANONGAME_MAX_GAMECOUNT/2; i++) {
	if (tc[i] == NULL)
	    continue;
	    
	conn_set_routeconn(tc[i], NULL);
	conn_destroy_anongame(tc[i]);
    }
    
    if (!(rpacket = packet_create(packet_class_bnet)))
	return -1;
    
    packet_set_size(rpacket,sizeof(t_server_findanongame_playgame_cancel));
    packet_set_type(rpacket,SERVER_FINDANONGAME_PLAYGAME_CANCEL);
    bn_byte_set(&rpacket->u.server_findanongame_playgame_cancel.cancel,SERVER_FINDANONGAME_CANCEL);
    bn_int_set(&rpacket->u.server_findanongame_playgame_cancel.count, a_count);
    conn_push_outqueue(c,rpacket);
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

	/* WAR3 uses a different table size, might change if blizzard add tournament support to RoC */
	if (strcmp(conn_get_clienttag(c),CLIENTTAG_WARCRAFT3)==0) {
    	    table_width = 5;
	    table_height= 4;
	}
	
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
        conn_push_outqueue(c,rpacket);
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
	
	conn_push_outqueue(c,rpacket);
	packet_del_ref(rpacket);
    } else {
	int i, j;
	bn_int temp;
	int client_tag;
	int client_tag_unk;
	int server_tag_unk;
	char ladr_count		= 0;
	char server_tag_count	= 0;
	char noitems		= 0;
	char desc_count		= 0;
	char mapscount_total	= 0;
	char value		= 0;
	char PG_gamestyles	= 0;
	char AT_gamestyles	= 0;
	char TY_gamestyles	= 0;
	char const * clienttag	= conn_get_clienttag(c);
	
	char anongame_prefix[ANONGAME_TYPES][5] = {		/* queue */
	/* PG 1v1	*/	{0x00, 0x00, 0x03, 0x3F, 0x00},	/*  0	*/
	/* PG 2v2	*/	{0x01, 0x00, 0x02, 0x3F, 0x00},	/*  1	*/
	/* PG 3v3	*/	{0x02, 0x00, 0x01, 0x3F, 0x00},	/*  2	*/
	/* PG 4v4	*/	{0x03, 0x00, 0x01, 0x3F, 0x00},	/*  3	*/
	/* PG sffa	*/	{0x04, 0x00, 0x02, 0x3F, 0x00},	/*  4	*/
	
	/* AT 2v2	*/	{0x00, 0x00, 0x02, 0x3F, 0x02},	/*  5	*/
	/* AT tffa	*/	{0x01, 0x00, 0x02, 0x3F, 0x02},	/*  6	*/
	/* AT 3v3	*/	{0x02, 0x00, 0x02, 0x3F, 0x03},	/*  7	*/
	/* AT 4v4	*/	{0x03, 0x00, 0x02, 0x3F, 0x04},	/*  8	*/
	
	/* TY		*/	{0x00, 0x01, 0x00, 0x3F, 0x00},	/*  9	*/
	
	/* PG 5v5	*/	{0x05, 0x00, 0x01, 0x3F, 0x00},	/* 10	*/
	/* PG 6v6	*/	{0x06, 0x00, 0x01, 0x3F, 0x00},	/* 11	*/
	/* PG 2v2v2	*/	{0x07, 0x00, 0x01, 0x3F, 0x00},	/* 12	*/
	/* PG 3v3v3	*/	{0x08, 0x00, 0x01, 0x3F, 0x00},	/* 13	*/
	/* PG 4v4v4	*/	{0x09, 0x00, 0x01, 0x3F, 0x00},	/* 14	*/
	/* PG 2v2v2v2	*/	{0x0A, 0x00, 0x01, 0x3F, 0x00},	/* 15	*/
	/* PG 3v3v3v3	*/	{0x0B, 0x00, 0x01, 0x3F, 0x00},	/* 16	*/

	/* AT 2v2v2	*/	{0x04, 0x00, 0x02, 0x3F, 0x02}	/* 17	*/
	};

	/* hack to give names for new gametypes untill there added to anongame_infos.c */
	char * anongame_gametype_names[ANONGAME_TYPES] = {
	    "One vs. One",
	    "Two vs. Two",
	    "Three vs. Three",
	    "Four vs. Four",
	    "Small Free for All",
	    "Two vs. Two",
	    "Team Free for All",
	    "Three vs. Three",
	    "Four vs. Four",
	    "Tournament Game",
	    "Five vs. Five",
	    "Six Vs. Six",
	    "Two vs. Two vs. Two",
	    "3 vs. 3 vs. 3",
	    "4 vs. 4 vs. 4",
	    "2 vs. 2 vs. 2 vs. 2",
	    "3 vs. 3 vs. 3 vs. 3",
	    "Two vs. Two vs. Two"
	};
	
	char anongame_PG_section	= 0x00;
	char anongame_AT_section	= 0x01;
	char anongame_TY_section	= 0x02;
	
	char last_packet		= 0x00;
	char other_packet		= 0x01;
	
	/* set thumbsdown from the conf file */
	for (j=0; j < ANONGAME_TYPES; j++)
	    anongame_prefix[j][2] = anongame_infos_get_thumbsdown(j);
	
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
		    mapscount_total = maplists_get_totalmaps(clienttag);
		    packet_append_data(rpacket, &mapscount_total, 1);
		    maplists_add_maps_to_packet(rpacket, clienttag);
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s)  tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_MAP",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_TYPE:
		    server_tag_unk=0x7C87DEEE;
		    packet_append_data(rpacket, "EPYT" , 4);
		    packet_append_data(rpacket, &server_tag_unk , 4);
		    
		    /* count of gametypes (PG, AT, TY) */
		    for (j=0; j < ANONGAME_TYPES; j++)
			if (maplists_get_totalmaps_by_queue(clienttag, j)) {
			    if (!anongame_prefix[j][1] && !anongame_prefix[j][4])
				PG_gamestyles++;
			    if (!anongame_prefix[j][1] && anongame_prefix[j][4])
				AT_gamestyles++;
			    if (anongame_prefix[j][1])
				TY_gamestyles++;
			}
		    
		    if (PG_gamestyles)
			value++;
		    if (AT_gamestyles)
			value++;
		    if (TY_gamestyles)
			value++;
		    
		    packet_append_data(rpacket, &value, 1);
		    
		    /* PG */
		    if (PG_gamestyles) {
			packet_append_data(rpacket, &anongame_PG_section,1);
			packet_append_data(rpacket, &PG_gamestyles,1);
			for (j=0; j < ANONGAME_TYPES; j++)
			    if (!anongame_prefix[j][1] && !anongame_prefix[j][4] &&
				    maplists_get_totalmaps_by_queue(clienttag,j))
			    {
				packet_append_data(rpacket, &anongame_prefix[j], 5);
				maplists_add_map_info_to_packet(rpacket, clienttag, j);
			    }
		    }
		    
		    /* AT */
		    if (AT_gamestyles) {
			packet_append_data(rpacket,&anongame_AT_section,1);
			packet_append_data(rpacket,&AT_gamestyles,1);
			for (j=0; j < ANONGAME_TYPES; j++)
			    if (!anongame_prefix[j][1] && anongame_prefix[j][4] &&
				    maplists_get_totalmaps_by_queue(clienttag,j))
			    {
				packet_append_data(rpacket, &anongame_prefix[j], 5);
				maplists_add_map_info_to_packet(rpacket, clienttag, j);
			    }
		    }
		    
		    /* TY */
		    if (TY_gamestyles) {
			packet_append_data(rpacket, &anongame_TY_section,1);
			packet_append_data(rpacket, &TY_gamestyles,1);
			for (j=0; j < ANONGAME_TYPES; j++)
			    if (anongame_prefix[j][1] &&
				    maplists_get_totalmaps_by_queue(clienttag,j))
			    {
				/* set tournament races available */
				anongame_prefix[j][3] = tournament_get_races();
				/* set tournament type (PG or AT)
				 * PG = 0
				 * AT = number players per team */
				if (tournament_is_arranged())
				    anongame_prefix[j][4] = tournament_get_game_type();
				else
				    anongame_prefix[j][4] = 0;
				
				packet_append_data(rpacket, &anongame_prefix[j], 5);
				maplists_add_map_info_to_packet(rpacket, clienttag, j);
			    }
		    }
		    
		    noitems++;
		    server_tag_count++;
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_TYPE",client_tag_unk);
		    break;
		case CLIENT_FINDANONGAME_INFOTAG_DESC:
		    server_tag_unk=0xA4F0A22F;
		    packet_append_data(rpacket, "CSED" , 4);
		    packet_append_data(rpacket,&server_tag_unk,4);
		    /* total descriptions */
		    for (j=0; j < ANONGAME_TYPES; j++)
			if (maplists_get_totalmaps_by_queue(clienttag, j))
			    desc_count++;
		    packet_append_data(rpacket,&desc_count,1);
		    /* PG description section */
		    for (j=0; j < ANONGAME_TYPES; j++)
			if (!anongame_prefix[j][1] && !anongame_prefix[j][4] &&
				maplists_get_totalmaps_by_queue(clienttag, j))
			{
			    packet_append_data(rpacket, &anongame_PG_section, 1);
			    packet_append_data(rpacket, &anongame_prefix[j][0], 1);
			    
			    if (anongame_infos_get_short_desc((char *)conn_get_country(c), j) == NULL)
				packet_append_string(rpacket,anongame_gametype_names[j]);
			    else
				packet_append_string(rpacket,anongame_infos_get_short_desc((char *)conn_get_country(c), j));
			    
			    if (anongame_infos_get_long_desc((char *)conn_get_country(c), j) == NULL)
				packet_append_string(rpacket,"No Descreption");
			    else
				packet_append_string(rpacket,anongame_infos_get_long_desc((char *)conn_get_country(c), j));
			}
		    /* AT description section */
		    for (j=0; j < ANONGAME_TYPES; j++)
			if (!anongame_prefix[j][1] && anongame_prefix[j][4] &&
				maplists_get_totalmaps_by_queue(clienttag, j))
			{
			    packet_append_data(rpacket, &anongame_AT_section, 1);
			    packet_append_data(rpacket, &anongame_prefix[j][0], 1);
			    packet_append_string(rpacket,anongame_infos_get_short_desc((char *)conn_get_country(c), j));
			    packet_append_string(rpacket,anongame_infos_get_long_desc((char *)conn_get_country(c), j));
			}
		    /* TY description section */
		    for (j=0; j < ANONGAME_TYPES; j++)
			if (anongame_prefix[j][1] &&
				maplists_get_totalmaps_by_queue(clienttag, j))
			{
			    packet_append_data(rpacket, &anongame_TY_section, 1);
			    packet_append_data(rpacket, &anongame_prefix[j][0], 1);
			    packet_append_string(rpacket,anongame_infos_get_short_desc((char *)conn_get_country(c), j));
			    packet_append_string(rpacket,anongame_infos_get_long_desc((char *)conn_get_country(c), j));
			}
		    eventlog(eventlog_level_debug,__FUNCTION__,"client_tag request tagid=(0x%01x) tag=(%s) tag_unk=(0x%04x)",i,"CLIENT_FINDANONGAME_INFOTAG_DESC",client_tag_unk);
		    noitems++;
		    server_tag_count++;
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
	    conn_push_outqueue(c,rpacket);
	    packet_del_ref(rpacket);
	}
    }
    return 0;
}

/* tournament notice disabled at this time, but responce is sent to cleint */
static int _client_anongame_tournament(t_connection * c, t_packet const * const packet)
{
    t_packet * rpacket = NULL;
    
    t_account * account = conn_get_account(c);
    char const * clienttag = conn_get_clienttag(c);
    
    unsigned int now		= time(NULL);
    unsigned int start_prelim	= tournament_get_start_preliminary();
    unsigned int end_signup	= tournament_get_end_signup();
    unsigned int end_prelim	= tournament_get_end_preliminary();
    unsigned int start_r1	= tournament_get_start_round_1();
    
    if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
	return -1;
    }
    
    packet_set_size(rpacket, sizeof(t_server_anongame_tournament_reply));
    packet_set_type(rpacket, SERVER_FINDANONGAME_TOURNAMENT_REPLY);
    bn_byte_set(&rpacket->u.server_anongame_tournament_reply.option, 7);
    bn_int_set(&rpacket->u.server_anongame_tournament_reply.count,
    bn_int_get(packet->u.client_anongame_tournament_request.count));
    
    if ( !start_prelim || (end_signup <= now && tournament_user_signed_up(account) < 0) ||
	    tournament_check_client(clienttag) < 0) { /* No Tournament Notice */
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
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0000); /* random */
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
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		tournament_get_stat(account, 1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		tournament_get_stat(account, 2));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		tournament_get_stat(account, 3));
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
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		tournament_get_stat(account, 1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		tournament_get_stat(account, 2));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		tournament_get_stat(account, 3));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x08);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (start_r1>=now && (tournament_get_game_in_progress()) ) { /* Prelim Period Over - Shows user stats (not all prelim games finished) */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		4);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0x0000); /* random */
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		_tournament_time_convert(start_r1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0x01);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		start_r1-now);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0); /* 00 00 */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		tournament_get_stat(account, 1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		tournament_get_stat(account, 2));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		tournament_get_stat(account, 3));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown3,		0x08);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.selection,		2);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.descnum,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.nulltag,		0);
    }
    else if (!(tournament_get_in_finals_status(account))) { /* Prelim Period Over - user did not make finals - Shows user stats */
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.type,		5);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown4,		0);
	bn_int_set(	&rpacket->u.server_anongame_tournament_reply.timestamp,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.unknown5,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.countdown,		0);
	bn_short_set(	&rpacket->u.server_anongame_tournament_reply.unknown2,		0);
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.wins,		tournament_get_stat(account, 1));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.losses,		tournament_get_stat(account, 2));
	bn_byte_set(	&rpacket->u.server_anongame_tournament_reply.ties,		tournament_get_stat(account, 3));
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
    
    conn_push_outqueue(c,rpacket);
    packet_del_ref(rpacket);
    return 0;
}

static unsigned int _tournament_time_convert(unsigned int time)
{
    /* it works, don't ask me how */ /* some time drift reportd by testers */
    unsigned int tmp1, tmp2, tmp3;
    
    tmp1 = time-1059179400;	/* 0x3F21CB88  */
    tmp2 = tmp1*0.59604645;
    tmp3 = tmp2+3276999960U;
    /*eventlog(eventlog_level_trace,__FUNCTION__,"time: 0x%08x, tmp1: 0x%08x, tmp2 0x%08x, tmp3 0x%08x",time,tmp1,tmp2,tmp3);*/

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
