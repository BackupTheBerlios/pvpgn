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
#ifndef INCLUDED_ANONGAME_MAPLISTS_TYPES
#define INCLUDED_ANONGAME_MAPLISTS_TYPES

#endif

/*****/
#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_ANONGAME_MAPLISTS_PROTOS
#define INCLUDED_ANONGAME_MAPLISTS_PROTOS

#define JUST_NEED_TYPES
#include "common/packet.h"
#undef JUST_NEED_TYPES

extern int	anongame_maplists_create(void);
extern void	anongame_maplists_destroy(void);

extern int	maplists_get_totalmaps(char const * clienttag);
extern int	maplists_get_totalmaps_by_queue(char const * clienttag, int queue);

extern void	maplists_add_maps_to_packet(t_packet * packet, char const * clienttag);
extern void	maplists_add_map_info_to_packet(t_packet * rpacket, char const * clienttag, int queue);

extern char *	maplists_get_map(int queue, char const * clienttag, int mapnumber);

extern int	anongame_add_tournament_map(char const * clienttag, char * mapname);
extern void	anongame_tournament_maplists_destroy(void);

#endif
#endif