/*
 * Copyright (C) 2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#define REALM_INTERNAL_ACCESS
#include "common/setup_before.h"
#include <stdio.h>
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
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strrchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"
#include "common/addr.h"
#include "connection.h"
#include "realm.h"
#include "common/setup_after.h"


static t_list * realmlist_head=NULL;

static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port, unsigned int showip, unsigned int showport);
static int realm_destroy(t_realm * realm);

static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port, unsigned int showip, unsigned int showport)
{
    t_realm * realm;
    
    if (!name)
    {
	eventlog(eventlog_level_error,"realm_create","got NULL name");
	return NULL;
    }
    if (!description)
    {
	eventlog(eventlog_level_error,"realm_create","got NULL description");
	return NULL;
    }
    
    if (!(realm = malloc(sizeof(t_realm))))
    {
	eventlog(eventlog_level_error,"realm_create","could not allocate memory for ad");
	return NULL;
    }
    
    realm->name = NULL;
    realm->description = NULL;

    if (realm_set_name(realm ,name)<0)
    {
        eventlog(eventlog_level_error,"realm_create","failed to set name for realm");
	free(realm);
	return NULL;
    }
    if (realm->description != NULL) free((void *)realm->description);
    if (!(realm->description = strdup(description)))
    {
	eventlog(eventlog_level_error,"realm_create","could not allocate memory for description");
	free((void *)realm->name); /* avoid warning */
	free(realm);
	return NULL;
    }
    realm->ip = ip;
    realm->port = port;
    realm->showip = showip;
    realm->showport = showport;
    realm->active = 0;
    realm->player_number = 0;
    realm->game_number = 0;
    realm->sessionnum = 0;
    realm->tcp_sock = 0;

    eventlog(eventlog_level_info,"realm_create","created realm \"%s\"",name);
    return realm;
}


static int realm_destroy(t_realm * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,"realm_destroy","got NULL realm");
	return -1;
    }
    
    if (realm->active)
    	realm_deactive(realm);

    free((void *)realm->name); /* avoid warning */
    free((void *)realm->description); /* avoid warning */
    free((void *)realm); /* avoid warning */
    
    return 0;
}


extern char const * realm_get_name(t_realm const * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,"realm_get_name","got NULL realm");
	return NULL;
    }
    return realm->name;
}


extern int realm_set_name(t_realm * realm, char const * name)
{
    char const      * temp;
    t_realm const * temprealm;

    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_set_name","got NULL realm");
        return -1;
    }

    if (name && (temprealm=realmlist_find_realm(name)))
    {
         if (temprealm == realm)
              return 0;
         else
         {
              eventlog(eventlog_level_error,"realm_set_name","realm %s does already exist in list",name);
              return -1;
         }
    }

    if (name)
      {
	if (!(temp=strdup(name)))
	  {
	    eventlog(eventlog_level_error,"realm_set_name","could not allocate memory for new realmname");
	    return -1;
	  }
      }
    else
      temp = NULL;

    if (realm->name)
      free((void *)realm->name); /* avoid warning */
    realm->name = temp;

    return 0;
}


extern char const * realm_get_description(t_realm const * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,"realm_get_description","got NULL realm");
	return NULL;
    }
    return realm->description;
}


extern unsigned short realm_get_port(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_port","got NULL realm");
        return 0;
    }
    return realm->port;
}


extern unsigned short realm_get_showport(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_port","got NULL realm");
        return 0;
    }
    return realm->showport;
}


extern unsigned int realm_get_ip(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_ip","got NULL realm");
        return 0;
    }
    return realm->ip;
}


extern unsigned int realm_get_showip(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_ip","got NULL realm");
        return 0;
    }
    return realm->showip;
}

extern unsigned int realm_get_active(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_active","got NULL realm");
        return 0;
    }
    return realm->active;
}

extern int realm_set_active(t_realm * realm, unsigned int active)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_active","got NULL realm");
        return -1;
    }

    realm->active=active;
    return 0;
}

extern unsigned int realm_get_player_number(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_player_number","got NULL realm");
        return 0;
    }
    return realm->player_number;
}

extern int realm_add_player_number(t_realm * realm, int number)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_add_player_number","got NULL realm");
        return -1;
    }
    realm->player_number += number;
    return 0;
}

extern unsigned int realm_get_game_number(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_get_game_number","got NULL realm");
        return 0;
    }
    return realm->game_number;
}

extern int realm_add_game_number(t_realm * realm, int number)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_add_game_number","got NULL realm");
        return -1;
    }
    realm->game_number += number;
    return 0;
}

extern int realm_active(t_realm * realm, t_connection * c)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_active","got NULL realm");
        return -1;
    }
    if (!c)
    {
        eventlog(eventlog_level_error,"realm_active","got NULL connection");
        return -1;
    }
    if (realm->active)
    {
        eventlog(eventlog_level_debug,"realm_active", "realm %s is already actived,destroy previous one",realm->name);
        realm_deactive(realm);
    }
    realm->active=1;
    realm->sessionnum=conn_get_sessionnum(c);
    realm->tcp_sock=conn_get_socket(c);
    eventlog(eventlog_level_info,"realm_active", "realm %s actived",realm->name);
    return 0;
}

extern int realm_deactive(t_realm * realm)
{
    t_connection * c;

    if (!realm)
    {
        eventlog(eventlog_level_error,"realm_deactive","got NULL realm");
        return -1;
    }
    if (!realm->active)
    {
        eventlog(eventlog_level_error,"realm_deactive","realm %s is not actived",realm->name);
        return -1;
    }
    if ((c = connlist_find_connection_by_sessionnum(realm->sessionnum)))
        conn_set_state(c,conn_state_destroy);

    realm->active=0;
    realm->sessionnum=0;
    realm->tcp_sock=0;
    /*
    realm->player_number=0;
    realm->game_number=0;
    */
    eventlog(eventlog_level_info,"realm_set_active", "realm %s deactived",realm->name);
    return 0;
}


extern int realmlist_create(char const * filename)
{
    FILE *          fp;
    unsigned int    line;
    unsigned int    pos;
    unsigned int    len;
    t_addr *        raddr;
    t_addr *	    showraddr;
    char *          temp, *temp2;
    char *          buff;
    char *          name;
    char *          desc;
    t_realm *       realm;
    
    if (!filename)
    {
        eventlog(eventlog_level_error,"realmlist_create","got NULL filename");
        return -1;
    }
    
    if (!(realmlist_head = list_create()))
    {
        eventlog(eventlog_level_error,"realmlist_create","could not create list");
        return -1;
    }
    
    if (!(fp = fopen(filename,"r")))
    {
        eventlog(eventlog_level_error,"realmlist_create","could not open realm file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	list_destroy(realmlist_head);
	realmlist_head = NULL;
        return -1;
    }
    
    for (line=1; (buff = file_get_line(fp)); line++)
    {
        for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
        if (buff[pos]=='\0' || buff[pos]=='#')
        {
            free(buff);
            continue;
        }
        if ((temp = strrchr(buff,'#')))
        {
	    unsigned int endpos;
	    
            *temp = '\0';
	    len = strlen(buff)+1;
            for (endpos=len-1;  buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
            buff[endpos+1] = '\0';
        }
        len = strlen(buff)+1;

	/* skip any separators */
	for (temp = buff; *temp && (*temp == ' ' || *temp == '\t');temp++);
	if (*temp != '"') {
	    eventlog(eventlog_level_error,"realmlist_create","malformed line %u in file \"%s\" (no realmname)",line,filename);
	    free(buff);
	    continue;
	}
	
	temp2 = temp + 1;
	/* find the next " */
	for (temp = temp2; *temp && *temp != '"';temp++);
	if (*temp != '"' || temp == temp2) {
	    eventlog(eventlog_level_error,"realmlist_create","malformed line %u in file \"%s\" (no realmname)",line,filename);
	    free(buff);
	    continue;
	}
	
	/* save the realmname */
	*temp = '\0';
        if (!(name = strdup(temp2)))
        {
            eventlog(eventlog_level_error,"realmlist_create","could not allocate memory for name");
            free(buff);
            continue;
        }
	
	/* eventlog(eventlog_level_trace, "realmlist_create","found realmname: %s",name); */

	/* skip any separators */
	for(temp = temp + 1; *temp && (*temp == '\t' || *temp == ' ');temp++);
	
	if (*temp == '"') { /* we have realm description */
	    temp2 = temp + 1;
	    /* find the next " */
	    for(temp = temp2;*temp && *temp != '"';temp++);
	    if (*temp != '"' || temp == temp2) {
		eventlog(eventlog_level_error,"realmlist_create","malformed line %u in file \"%s\" (no valid description)",line,filename);
		free(name);
		free(buff);
		continue;
	    }
	    
	    /* save the description */
	    *temp = '\0';
    	    if (!(desc = strdup(temp2))) {
        	eventlog(eventlog_level_error,"realmlist_create","could not allocate memory for desc");
        	free(name);
        	free(buff);
        	continue;
    	    }
	    
	    /* eventlog(eventlog_level_trace, "realmlist_create","found realm desc: %s",desc); */

	    /* skip any separators */
	    for(temp = temp + 1; *temp && (*temp == ' ' || *temp == '\t');temp++);
	} else desc = strdup("\0");
	
	temp2 = temp;
	/* find out where address ends */
	for(temp = temp2 + 1; *temp && *temp != ' ' && *temp != '\t';temp++);

	if (*temp) *temp++ = '\0'; /* if is not the end of the file, end addr and move forward */

	/* eventlog(eventlog_level_trace, "realmlist_create","found realm ip: %s",temp2); */

	if (!(raddr = addr_create_str(temp2,0,BNETD_REALM_PORT))) /* 0 means "this computer" */ {
	    eventlog(eventlog_level_error,"realmlist_create","invalid address value for field 3 on line %u in file \"%s\"",line,filename);
	    free(name);
	    free(buff);
	    free(desc);
	    continue;
	}
	
	/* skip any separators */
	for(; *temp && (*temp == ' ' || *temp == '\t');temp++);
	
	if (*temp) { /* do we have show addr */
	    temp2 = temp;
	    /* find out where address ends */
	    for(temp = temp2 + 1; *temp && *temp != ' ' && *temp != '\t';temp++);

	    *temp = '\0';
	    /* eventlog(eventlog_level_trace, "realmlist_create","found realm show ip: %s", temp2);*/

	    if (!(showraddr = addr_create_str(temp2,0,BNETD_REALM_PORT))) /* 0 means "this computer" */ {
		eventlog(eventlog_level_error,"realmlist_create","invalid show address value for field 3 on line %u in file \"%s\"",line,filename);
		free(name);
		free(buff);
		free(desc);
		continue;
	    }
	} else showraddr = raddr;
	
	if (!(realm = realm_create(name,desc,addr_get_ip(raddr),addr_get_port(raddr),addr_get_ip(showraddr),addr_get_port(showraddr))))
	{
	    eventlog(eventlog_level_error,"realmlist_create","could not create realm");
	    if (showraddr != raddr) addr_destroy(showraddr);
	    addr_destroy(raddr);
	    free(name);
	    free(buff);
	    free(desc);
	    continue;
	}
	
	if (showraddr != raddr) addr_destroy(showraddr);
	addr_destroy(raddr);
	free(name);
	free(buff);
	free(desc);
	
	if (list_prepend_data(realmlist_head,realm)<0)
	{
	    eventlog(eventlog_level_error,"realmlist_create","could not prepend realm");
	    realm_destroy(realm);
	    continue;
	}
    }
    if (fclose(fp)<0)
	eventlog(eventlog_level_error,"realmlist_create","could not close realm file \"%s\" after reading (fclose: %s)",filename,strerror(errno));
    return 0;
}


extern int realmlist_destroy(void)
{
    t_elem *  curr;
    t_realm * realm;
    
    if (realmlist_head)
    {
	LIST_TRAVERSE(realmlist_head,curr)
	{
	    if (!(realm = elem_get_data(curr)))
		eventlog(eventlog_level_error,"realmlist_destroy","found NULL realm in list");
	    else
		realm_destroy(realm);
	    list_remove_elem(realmlist_head,curr);
	}
	list_destroy(realmlist_head);
	realmlist_head = NULL;
    }
    
    return 0;
}


extern t_list * realmlist(void)
{
    return realmlist_head;
}


extern t_realm * realmlist_find_realm(char const * realmname)
{
    t_elem const *  curr;
    t_realm * realm;
    
    if (!realmname)
    {
	eventlog(eventlog_level_error,"realmlist_find_realm","got NULL realmname");
	return NULL;
    }

    LIST_TRAVERSE_CONST(realmlist_head,curr)
    {
	realm = elem_get_data(curr);
	if (strcasecmp(realm->name,realmname)==0)
	    return realm;
    }
    
    return NULL;
}

extern t_realm * realmlist_find_realm_by_ip(unsigned long ip)
{
    t_elem const *  curr;
    t_realm * realm;

    LIST_TRAVERSE_CONST(realmlist_head,curr)
    {
        realm = elem_get_data(curr);
        if (realm->ip==ip)
            return realm;
    }
    return NULL;
}

extern t_realm * realmlist_find_realm_by_sock(int tcp_sock)
{
    t_elem const *  curr;
    t_realm * realm;

    LIST_TRAVERSE_CONST(realmlist_head,curr)
    {
        realm = elem_get_data(curr);
        if (realm->tcp_sock==tcp_sock)
            return realm;
    }
    return NULL;
}
