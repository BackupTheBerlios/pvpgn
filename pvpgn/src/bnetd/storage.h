#ifdef WITH_MYSQL
/*
  * Copyright (C) 2002 Dizzy 
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

#ifndef INCLUDED_STORAGE_H
#define INCLUDED_STORAGE_H

#include "common/eventlog.h"
#include "pvpgn_mysql.h"
#ifdef WIN32
 #include "compat/strcasecmp.h"
#endif

#define STORAGE_DEFAULT_ID 0

typedef struct readattr_struct {
    t_attrlist *alist;
    unsigned int pos;
} t_readattr;

typedef struct readacct_struct {
    t_uidlist *ulist;
    unsigned int pos;
} t_readacct;

extern int storage_init(void);
extern void storage_destroy(void);
extern unsigned int storage_create_account(const char *);

extern t_readattr * storage_attr_getfirst(unsigned int, char **, char **);
extern int storage_attr_getnext(t_readattr *, char **, char **);
extern int storage_attr_close(t_readattr *);

extern t_readacct * storage_account_getfirst(unsigned int*);
extern int storage_account_getnext(t_readacct *, unsigned int*);
extern int storage_account_close(t_readacct *);

extern int storage_set(unsigned int, const char *, const char *);
#endif

#endif
