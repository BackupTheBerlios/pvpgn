/*
 * Copyright (C) 2004      ls_sons  (ls@gamelife.org)
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

typedef struct  d2charlist_internal{
    char const			* name;
    unsigned int		exp;
    unsigned int		mtime;
    unsigned int		level;
    struct d2charlist_internal	*next;
    struct d2charlist_internal	*prev;
} t_d2charlist_internal;

typedef struct  d2charlist{
    t_d2charlist_internal	*first;
    t_d2charlist_internal	*last;
} t_d2charlist;

extern void d2charlist_init(t_d2charlist *);
extern void d2charlist_destroy(t_d2charlist *);

extern int d2charlist_add_char(t_d2charlist *, char const *, bn_int, bn_int, bn_int);

void do_add_char (t_d2charlist *, t_d2charlist_internal *, t_d2charlist_internal *);
