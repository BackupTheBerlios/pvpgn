/*
 * Copyright (C) 2003  Aaron
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
#ifndef INCLUDED_BINARY_LADDER_TYPES
#define INCLUDED_BINARY_LADDER_TYPES


// some stuff here

typedef enum 
{	WAR3_SOLO, WAR3_TEAM, WAR3_FFA, WAR3_AT, 
	W3XP_SOLO, W3XP_TEAM, W3XP_FFA, W3XP_AT
	// add SC/BW/Diablo later on
} e_binary_ladder_types;

typedef enum
{	load_success, illegal_checksum, load_failed
} e_binary_ladder_load_result;

#ifdef BINARY_LADDER_INTERNAL_ACCESS

#define magick 0xdeadbeef

#endif

#endif

/*****/
#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_BINARY_LADDER_PROTOS
#define INCLUDED_BINARY_LADDER_PROTOS

// some protos here

int binary_ladder_save(e_binary_ladder_types type, unsigned int paracount, int (*_cb_get_from_ladder)());
e_binary_ladder_load_result binary_ladder_load(e_binary_ladder_types type, unsigned int paracount, int (*_cb_add_to_ladder)());

#endif
#endif
