/*
 * Copyright (C) 2002
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
#define W3TRANS_INTERNAL_ACCESS
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
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/addr.h"
#include "common/util.h"
#include "w3trans.h"
#include "common/setup_after.h"

#define DEBUGW3TRANS 1

static t_list * w3trans_head=NULL;

extern int w3trans_load(char const * filename)
{
    FILE		* fp;
    unsigned int	line, pos;
    char		* buff, * temp;
    char const		* network, * output;
    t_w3trans		* entry;
    
    if (!filename) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
        return -1;
    }
    
    if (!(w3trans_head = list_create())) {
        eventlog(eventlog_level_error,__FUNCTION__,"could not create list");
        return -1;
    }
    
    if (!(fp = fopen(filename,"r"))) {
        eventlog(eventlog_level_error,__FUNCTION__,"could not open file \"%s\" for reading (fopen: %s)",filename,strerror(errno));
	list_destroy(w3trans_head);
	w3trans_head = NULL;
        return -1;
    }

    for (line=1; (buff = file_get_line(fp)); line++) {
        for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
	
        if (buff[pos]=='\0' || buff[pos]=='#') {
            free(buff);
            continue;
        }
	
        if ((temp = strrchr(buff,'#'))) {
	    unsigned int	len, endpos;
	    
            *temp = '\0';
	    len = strlen(buff)+1;
            for (endpos=len-1; buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
            buff[endpos+1] = '\0';
        }
	
	if (!(network = strtok(buff," \t"))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"missing network on line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	
	if (!(output = strtok(NULL," \t"))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"missing output on line %u of file \"%s\"",line,filename);
	    free(buff);
	    continue;
	}
	
	if (!(entry = malloc(sizeof(t_w3trans)))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for entry");
	    free(buff);
	    continue;
	}
	
	if (!(entry->network = netaddr_create_str(network))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for network address");
	    free(entry);
	    free(buff);
	    continue;
	}
	
	if (!(entry->output = addr_create_str(output,0,BNETD_W3ROUTE_PORT))) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not allocate memory for output address");
	    netaddr_destroy(entry->network);
	    free(entry);
	    free(buff);
	    continue;
	}
	
	free(buff);

	if (list_append_data(w3trans_head,entry)<0) {
	    eventlog(eventlog_level_error,__FUNCTION__,"could not append item");
	    addr_destroy(entry->output);
	    netaddr_destroy(entry->network);
	    free(entry);
	}
    }
    
    fclose(fp);
    eventlog(eventlog_level_info,__FUNCTION__,"w3trans file loaded");
    return 0;
}


extern int w3trans_unload(void)
{
    t_elem	* curr;
    t_w3trans	* entry;
    
    if (w3trans_head) {
	LIST_TRAVERSE(w3trans_head,curr)
	{
	    if (!(entry = elem_get_data(curr)))
		eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
	    else {
		addr_destroy(entry->output);
		netaddr_destroy(entry->network);
		free(entry);
	    }
	    
	    list_remove_elem(w3trans_head,curr);
	}
	
	list_destroy(w3trans_head);
	w3trans_head = NULL;
    }
    
    return 0;
}

extern int w3trans_reload(char const * filename)
{
    w3trans_unload();
    
    if(w3trans_load(filename)<0)
	return -1;
    
    return 0;
}


extern void w3trans_net(unsigned int clientaddr, unsigned int *w3ip, unsigned short *w3port)
{
    t_elem const	* curr;
    t_w3trans		* entry;

#ifdef DEBUGW3TRANS
    char		temp1[32], temp2[32];
#endif

#ifdef DEBUGW3TRANS
    eventlog(eventlog_level_debug,__FUNCTION__,"checking client '%s' for w3route ip ...",
	     addr_num_to_ip_str(clientaddr));	/* client ip */
#endif

    if (w3trans_head) {
	LIST_TRAVERSE_CONST(w3trans_head,curr)
	{
	    if (!(entry = elem_get_data(curr))) {
		eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		continue;
	    }

#ifdef DEBUGW3TRANS
	    eventlog(eventlog_level_debug,__FUNCTION__,"against entry: network '%s' output '%s' ",
		     netaddr_get_addr_str(entry->network,temp1,sizeof(temp1)),	/* network */
		     addr_get_addr_str(entry->output,temp2,sizeof(temp2)));	/* output */
#endif

	    if (!(netaddr_contains_addr_num(entry->network,clientaddr))) {
#ifdef DEBUGW3TRANS
		eventlog(eventlog_level_debug,__FUNCTION__,"client is not part of network");
#endif
		continue;
	    }

	    *w3ip = addr_get_ip(entry->output);
	    *w3port = addr_get_port(entry->output);
	}

    }
    return;
}
