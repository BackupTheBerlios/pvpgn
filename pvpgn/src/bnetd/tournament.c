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

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
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

#include "common/eventlog.h"
#include "common/packet.h"
#include "common/tag.h"
#include "common/list.h"
#include "common/util.h"
#include "account.h"
#include "anongame_maplists.h"
#include "tournament.h"
#include "common/setup_after.h"

static t_tournament_info * tournament_info = NULL;
static t_list * tournament_head=NULL;

static int tournamentlist_create(void);
static int gamelist_destroy(void);
static t_tournament_user * tournament_get_user(t_account * account);
//static int tournament_get_in_game_status(t_account * account);

/*****/
static int tournamentlist_create(void)
{
    if (!(tournament_head = list_create()))
	return -1;
    return 0;
}

static int gamelist_destroy(void)
{
    t_elem *		curr;
    t_tournament_user * user;
    
    if (tournament_head) {
	LIST_TRAVERSE(tournament_head,curr)
	{
	    if (!(user = elem_get_data(curr))) {
		eventlog(eventlog_level_error,__FUNCTION__,"tournament list contains NULL item");
		continue;
	    }
	    
	    if (list_remove_elem(tournament_head,curr)<0)
		eventlog(eventlog_level_error,__FUNCTION__,"could not remove item from list");
	    
	    if (user->name)
		free((void *)user->name); /* avoid warning */
	    
	    free(user);
	    
	}
	
	if (list_destroy(tournament_head)<0)
	    return -1;
	tournament_head = NULL;
    }
    return 0;
}

/*****/
extern int tournament_check_client(char const * clienttag)
{
    if ((strcmp(clienttag,CLIENTTAG_WAR3XP) == 0) && (tournament_info->game_client == 2))
	return 1;
    if ((strcmp(clienttag,CLIENTTAG_WARCRAFT3) == 0) && (tournament_info->game_client == 1))
	return 1;
	
    return -1;
}

extern int tournament_signup_user(t_account * account)
{
    t_tournament_user * user;

    if (!(account))
	return -1;
    
    if ((user = tournament_get_user(account))) {
	eventlog(eventlog_level_info,__FUNCTION__,"user \"%s\" already signed up in tournament",account_get_name(account));
	return 0;
    }
    
    if (!(user = malloc(sizeof(t_tournament_user)))) {
	eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for tournament user");
	return -1;
    }
    
    user->name		= strdup(account_get_name(account));
    user->wins		= 0;
    user->losses	= 0;
    user->ties		= 0;
    user->in_game	= 0;
    user->in_finals	= 0;
        
    if (list_prepend_data(tournament_head,user)<0) {
	eventlog(eventlog_level_error,__FUNCTION__,"could not insert user");
	free((void *)user->name); /* avoid warning */
	free(user);
	return -1;
    }
    
    eventlog(eventlog_level_info,__FUNCTION__,"added user \"%s\" to tournament",account_get_name(account));
    return 0;
}

static t_tournament_user * tournament_get_user(t_account * account)
{
    t_elem const * curr;
    t_tournament_user * user;
    
    if (tournament_head)
	LIST_TRAVERSE(tournament_head,curr)
	{
	    user = elem_get_data(curr);
	    if (strcmp(user->name, account_get_name(account)) == 0)
		return user;
	}
    
    return NULL;
}

extern int tournament_user_signed_up(t_account * account)
{
    if (!(tournament_get_user(account)))
	return -1;
        
    return 0;
}


extern int tournament_add_stat(t_account * account, int stat)
{
    t_tournament_user * user;
    
    if (!(user = tournament_get_user(account)))
	return -1;
    
    if (stat == 1)
	user->wins++;
    if (stat == 2)
	user->losses++;
    if (stat == 3)
	user->ties++;
    
    return 0;
}

extern int tournament_get_stat(t_account * account, int stat)
{
    t_tournament_user * user;
    
    if (!(user = tournament_get_user(account)))
	return 0;
    
    if (stat == 1)
	return user->wins;
    if (stat == 2)
	return user->losses;
    if (stat == 3)
	return user->ties;

    return 0;
}

extern int tournament_get_player_score(t_account * account)
{
    t_tournament_user * user;
    int score;
    
    if (!(user = tournament_get_user(account)))
	return 0;
    
    score = user->wins * 3 + user->ties - user->losses;
    
    if (score < 0)
	return 0;
	
    return score;
}
    
extern int tournament_set_in_game_status(t_account * account, int status)
{
    t_tournament_user * user;
    
    if (!(user = tournament_get_user(account)))
	return -1;
    
    user->in_game = status;
    
    return 0;
}
/*
static int tournament_get_in_game_status(t_account * account)
{
    t_tournament_user * user;
    
    if (!(user = tournament_get_user(account)))
	return 0;
    
    return user->in_game;
}
*/
extern int tournament_get_in_finals_status(t_account * account)
{
    t_tournament_user * user;
    
    if (!(user = tournament_get_user(account)))
	return 0;
    
    return user->in_finals;
}

extern int tournament_get_game_in_progress(void)
{
    t_elem const * curr;
    t_tournament_user * user;
    
    if (tournament_head)
	LIST_TRAVERSE_CONST(tournament_head,curr)
	{
	    user = elem_get_data(curr);
	    if (user->in_game == 1)
		return 1;
	}
    
    return 0;
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

/*****/
extern int tournament_init(char const * filename)
{
    FILE * fp;
    unsigned int line,pos,mon,day,year,hour,min,sec;
    char *buff,*temp,*pointer,*value;
    char format[30];
    char *variable;
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
    tournament_info->game_selection	= 1; /* Default to PG */
    tournament_info->game_type		= 1; /* Default to 1v1 */
    tournament_info->game_client	= 2; /* Default to FT */
    tournament_info->races		= 0x3F; /* Default to all races */
    tournament_info->format		= strdup("");
    tournament_info->sponsor		= strdup("");
    tournament_info->thumbs_down	= 0;
    
    anongame_tournament_maplists_destroy();
    
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
	
	if (strcmp(buff,"[MAPS]") == 0) {
	    char *clienttag, *ctag, *mapname, *mname;
	    
	    free(buff);
	    for (; (buff = file_get_line(fp));) {
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
		
		if (!(clienttag = strtok(buff, " \t"))) { /* strtok modifies the string it is passed */
		    free(buff);
		    continue;
		}
		if (strcmp(buff,"[ENDMAPS]") == 0) {
		    free(buff);
		    break;
		}
		if (!(mapname = strtok(NULL," \t"))) {
		    free(buff);
		    continue;
		}
		ctag = strdup(clienttag);
		mname = strdup(mapname);
		
		anongame_add_tournament_map(ctag, mname);
		eventlog(eventlog_level_trace,__FUNCTION__,"added tournament map \"%s\" for %s",mname,ctag);
		free(ctag);
		free(mname);
		free(buff);
	    }
	} else {
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
		if (atoi(pointer) >= 1 && atoi(pointer) <= 2)
		    tournament_info->game_selection = atoi(pointer);
	    }
	    else if (strcmp(variable,"game_type") == 0) {
		if (atoi(pointer) >= 1 && atoi(pointer) <= 4)
		    tournament_info->game_type = atoi(pointer);
	    }
	    else if (strcmp(variable,"game_client") == 0) {
		if (atoi(pointer) >= 1 && atoi(pointer) <= 2)
		    tournament_info->game_client = atoi(pointer);
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
	    else if (strcmp(variable,"races") == 0) {
	        unsigned int intvalue = 0;
	        unsigned int i;
	        
	        pointer = strchr(pointer,'\"');
	        pointer++;
	        value = pointer;
	        pointer = strchr(pointer,'\"');
	        pointer[0]='\0';
	        
	        for(i=0;i<strlen(value);i++) {
		    if (value[i] == 'H') intvalue = intvalue | 0x01;
		    if (value[i] == 'O') intvalue = intvalue | 0x02;
		    if (value[i] == 'N') intvalue = intvalue | 0x04;
		    if (value[i] == 'U') intvalue = intvalue | 0x08;
		    if (value[i] == 'R') intvalue = intvalue | 0x20;
		}
		
		if (intvalue == 0 || intvalue == 0x2F)
		    intvalue = 0x3F; /* hack to make all races availiable */
		
	        tournament_info->races = intvalue;
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
		
		if (strlen(have_icon) == 4)
		    sprintf(sponsor, "%c%c%c%c,%s",have_icon[3],have_icon[2],have_icon[1],have_icon[0],have_sponsor);
		else if (strlen(have_icon) == 2)
		    sprintf(sponsor, "%c%c3W,%s",have_icon[1],have_icon[0],have_sponsor);
		else {
		    sprintf(sponsor, "PX3W,%s",have_sponsor); /* default to standard FT icon */
		    eventlog(eventlog_level_warn,__FUNCTION__,"bad icon length, using W3XP");
		}
		
		if (tournament_info->sponsor)
		    free((void *)tournament_info->sponsor);
	        
		tournament_info->sponsor = strdup(sponsor);
	        free((void *)have_sponsor);
		free((void *)have_icon);
	        free((void *)sponsor);
		have_sponsor = NULL;
	        have_icon = NULL;
	    }
	    free(buff);
	}
    }
    if (have_sponsor) free((void *)have_sponsor);
    if (have_icon) free((void *)have_icon);
    free((void *)timestamp);
    fclose(fp);
    
    /* check if we have timestamps for all the times */ 
    /* if not disable tournament by setting "start_preliminary" to 0 */
    if (tournament_info->end_signup == 0 || tournament_info->end_preliminary == 0 ||
	    tournament_info->start_round_1 == 0 || tournament_info->start_round_2 == 0 ||
	    tournament_info->start_round_3 == 0 || tournament_info->start_round_4 == 0 ||
	    tournament_info->tournament_end == 0) {
	tournament_info->start_preliminary = 0;
	eventlog(eventlog_level_warn,__FUNCTION__,"one or more timestamps for tournaments is not valid, tournament has been disabled");
    } else {
    	if (tournamentlist_create()<0) {
	    tournament_info->start_preliminary = 0;
	    eventlog(eventlog_level_warn,__FUNCTION__,"unable to create tournament list, tournament has been disabled");
	}
    }
    
    return 0;
}

extern int tournament_destroy(void)
{
    if (tournament_info->format) free((void *)tournament_info->format);
    if (tournament_info->sponsor) free((void *)tournament_info->sponsor);
    if (tournament_info) free((void *)tournament_info);
    tournament_info = NULL;
    gamelist_destroy();
    return 0;
}
/*****/
extern unsigned int tournament_get_start_preliminary(void)
{
    return tournament_info->start_preliminary;
}

extern unsigned int tournament_get_end_signup(void)
{
    return tournament_info->end_signup;
}

extern unsigned int tournament_get_end_preliminary(void)
{
    return tournament_info->end_preliminary;
}

extern unsigned int tournament_get_start_round_1(void)
{
    return tournament_info->start_round_1;
}

extern unsigned int tournament_get_start_round_2(void)
{
    return tournament_info->start_round_2;
}

extern unsigned int tournament_get_start_round_3(void)
{
    return tournament_info->start_round_3;
}

extern unsigned int tournament_get_start_round_4(void)
{
    return tournament_info->start_round_4;
}

extern unsigned int tournament_get_tournament_end(void)
{
    return tournament_info->tournament_end;
}

extern unsigned int tournament_get_game_selection(void)
{
    return tournament_info->game_selection;
}

extern unsigned int tournament_get_game_type(void)
{
    return tournament_info->game_type;
}

extern unsigned int tournament_get_races(void)
{
    return tournament_info->races;
}

extern char * tournament_get_format(void)
{
    return tournament_info->format;
}

extern char * tournament_get_sponsor(void)
{
    return tournament_info->sponsor;
}

extern unsigned int tournament_get_thumbs_down(void)
{
    return tournament_info->thumbs_down;
}
/****/
