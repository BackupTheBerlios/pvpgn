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
#ifndef INCLUDED_HANDLE_ANONGAME_TYPES
#define INCLUDED_HANDLE_ANONGAME_TYPES

typedef struct
{
    unsigned int start_preliminary;
    unsigned int end_signup;
    unsigned int end_preliminary;
    unsigned int start_round_1;
    unsigned int start_round_2;
    unsigned int start_round_3;
    unsigned int start_round_4;
    unsigned int tournament_end;
    unsigned int game_selection;
    unsigned int game_type;
    char *	 format;
    char *	 sponsor;	/* format: "ricon,sponsor"
				 * ricon = W3+icon reversed
				 * ie. "4R3W,The PvPGN Team"
				 */
    unsigned int thumbs_down;
} t_tournament_info;

#endif

#ifndef INCLUDED_HANDLE_ANONGAME_PROTOS
#define INCLUDED_HANDLE_ANONGAME_PROTOS

extern int tournament_init(char const * filename);
extern int tournament_destroy(void);
extern int handle_anongame_packet(t_connection * c, t_packet const * const packet);
extern int tournament_get_totalplayers(void);
extern int tournament_is_arranged(void);

#endif
