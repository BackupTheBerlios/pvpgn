/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#ifndef JUST_NEED_TYPES
/* FIXME: could probably be in PROTOS... does any other header need these? */
#ifndef INCLUDED_TAG_TYPES
#define INCLUDED_TAG_TYPES

/* Software tags */
#define CLIENTTAG_BNCHATBOT 		"CHAT" /* CHAT bot */
#define CLIENTTAG_BNCHATBOT_UINT 	0x43484154
#define CLIENTTAG_STARCRAFT 		"STAR" /* Starcraft (original) */
#define CLIENTTAG_STARCRAFT_UINT 	0x53544152
#define CLIENTTAG_BROODWARS 		"SEXP" /* Starcraft EXpansion Pack */
#define CLIENTTAG_BROODWARS_UINT 	0x53455850
#define CLIENTTAG_SHAREWARE 		"SSHR" /* Starcraft Shareware */
#define CLIENTTAG_SHAREWARE_UINT	0x53534852
#define CLIENTTAG_DIABLORTL 		"DRTL" /* Diablo ReTaiL */
#define CLIENTTAG_DIABLORTL_UINT	0x4452544C
#define CLIENTTAG_DIABLOSHR 		"DSHR" /* Diablo SHaReware */
#define CLIENTTAG_DIABLOSHR_UINT	0x44534852
#define CLIENTTAG_WARCIIBNE 		"W2BN" /* WarCraft II Battle.net Edition */
#define CLIENTTAG_WARCIIBNE_UINT	0x5732424E
#define CLIENTTAG_DIABLO2DV 		"D2DV" /* Diablo II Diablo's Victory */
#define CLIENTTAG_DIABLO2DV_UINT	0x44324456
#define CLIENTTAG_STARJAPAN 		"JSTR" /* Starcraft (Japan) */
#define CLIENTTAG_STARJAPAN_UINT	0x4A535452
#define CLIENTTAG_DIABLO2ST 		"D2ST" /* Diablo II Stress Test */
#define CLIENTTAG_DIABLO2ST_UINT	0x44325354
#define CLIENTTAG_DIABLO2XP 		"D2XP" /* Diablo II Extension Pack */
#define CLIENTTAG_DIABLO2XP_UINT	0x44325850
/* FIXME: according to FSGS:
    SJPN==Starcraft (Japanese)
    SSJP==Starcraft (Japanese,Spawn)
*/
#define CLIENTTAG_WARCRAFT3 		"WAR3" /* WarCraft III */
#define CLIENTTAG_WARCRAFT3_UINT	0x57415233
#define CLIENTTAG_WAR3XP    		"W3XP" /* WarCraft III Expansion */
#define CLIENTTAG_WAR3XP_UINT		0x57335850

/* BNETD-specific software tags - we try to use lowercase to avoid collisions  */
#define CLIENTTAG_FREECRAFT "free" /* FreeCraft http://www.freecraft.com/ */

#define CLIENTTAG_UNKNOWN		"UNKN"
#define CLIENTTAG_UNKNOWN_UINT		0x554E4B4E

/* Architecture tags */
#define ARCHTAG_WINX86       "IX86" /* MS Windows on Intel x86 */
#define ARCHTAG_MACPPC       "PMAC" /* MacOS   on PowerPC   */
#define ARCHTAG_OSXPPC       "XMAC" /* MacOS X on PowerPC   */

/* Server tag */
#define BNETTAG "bnet" /* Battle.net */

/* Filetype tags (note these are "backwards") */
#define EXTENSIONTAG_PCX "xcp."
#define EXTENSIONTAG_SMK "kms."
#define EXTENSIONTAG_MNG "gnm."

#endif
#endif
