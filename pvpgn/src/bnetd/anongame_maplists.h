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
#ifndef INCLUDED_ANONGAME_MAPLISTS_PROTOS
#define INCLUDED_ANONGAME_MAPLISTS_PROTOS

extern int anongame_maplists_create(void);
extern void anongame_maplists_destroy(void);

extern t_list * anongame_get_w3xp_maplist(int gametype, const char * clienttag);
extern void anongame_add_maps_to_packet(t_packet * packet, int gametype, const char * clienttag);

extern int anongame_add_tournament_map(char * ctag, char * mname);
extern void anongame_tournament_maplists_destroy(void);

#endif
