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

#ifndef INCLUDED_TOPIC_TYPES
#define INCLUDED_TOPIC_TYPES

#ifdef TOPIC_INTERNAL_ACCESS

#ifdef JUST_NEED_TYPES
# include <stdio.h>
# include "common/list.h"
#else
# define JUST_NEED_TYPES
# include <stdio.h>
# include "common/list.h"
# undef JUST_NEED_TYPES
#endif

#endif

typedef struct topic
#ifdef TOPIC_INTERNAL_ACCESS
{
  char *   channel_name;
  char *   topic;
}
#endif
t_topic;

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_TOPIC_PROTOS
#define INCLUDED_TOPIC_PROTOS

#define JUST_NEED_TYPES
#include "common/list.h"
#undef JUST_NEED_TYPES

int    topiclist_load(char const * topicfile);
int    topiclist_unload();
int    channel_set_topic(char * channel_name, char * topic_text);
char * channel_get_topic(char * channel_name);

#endif
#endif
