/*
 * Copyright (C) 2004 Dizzy 
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

#ifndef __ATTRTYPE_INCLUDED__
#define __ATTRTYPE_INCLUDED__

#include "common/elist.h"

typedef struct attr_struct {
    const char 		*key;
    const char 		*val;
    int			dirty;
    t_hlist		link;
} t_attr;

#endif /* __ATTRTYPE_INCLUDED__ */

#ifndef __ATTR_H_INCLUDED__
#define __ATTR_H_INCLUDED__

#ifdef HAVE_TIME_H
# include <time.h>
#endif
#include "common/elist.h"

#ifndef JUST_NEED_TYPES
#define JUST_NEED_TYPES
#include "storage.h"
#undef JUST_NEED_TYPES
#else
#include "storage.h"
#endif

#define ATTRLIST_FLAG_NONE	0
#define ATTRLIST_FLAG_LOADED	1
#define ATTRLIST_FLAG_ACCESSED	2
#define ATTRLIST_FLAG_DIRTY	4

/* flags controlling flush/save operations */
#define	FS_NONE		0
#define FS_FORCE	1	/* force save/flush no matter of time */
#define FS_ALL		2	/* save/flush all, not in steps */

typedef struct attrlist_struct 
#ifdef ATTRLIST_INTERNAL_ACCESS
{
    t_hlist		list;
    t_storage_info	*storage;
    int			flags;
    time_t		lastaccess;
    time_t		dirtytime;
    t_elist		loadedlist;
    t_elist		dirtylist;
} 
#endif
t_attrlist;

typedef int (*t_attr_cb)(t_attrlist *, void *);

#include "common/xalloc.h"

extern int attrlayer_init(void);
extern int attrlayer_cleanup(void);
extern int attrlayer_load_default(void);
extern int attrlayer_save(int flags);
extern int attrlayer_flush(int flags);

extern t_attrlist *attrlist_create_newuser(const char *name);
extern t_attrlist *attrlist_create_nameuid(const char *name, unsigned uid);
extern int attrlist_destroy(t_attrlist *attrlist);
extern int attrlist_read_accounts(int flag, t_attr_cb cb, void *data);
extern const char *attrlist_get_attr(t_attrlist *attrlist, const char *key);
extern int attrlist_set_attr(t_attrlist *attrlist, const char *key, const char *val);
extern int attrlist_save(t_attrlist *attrlist, int flags);
extern int attrlist_flush(t_attrlist *attrlist, int flags);

static inline t_attr *attr_create(const char *key, const char *val)
{
    t_attr *attr;

    attr = xmalloc(sizeof(t_attr));
    attr->dirty = 0;
    hlist_init(&attr->link);
    attr->key = key ? xstrdup(key) : NULL;
    attr->val = val ? xstrdup(val) : NULL;

    return attr;
}

static inline int attr_destroy(t_attr *attr)
{
    if (attr->key) xfree((void*)attr->key);
    if (attr->val) xfree((void*)attr->val);

    xfree((void*)attr);

    return 0;
}

static inline int attr_get_dirty(t_attr *attr)
{
    return attr->dirty;
}

static inline void attr_clear_dirty(t_attr *attr)
{
    attr->dirty = 0;
}

static inline const char *attr_get_key(t_attr *attr)
{
    return attr->key;
}

static inline const char *attr_get_val(t_attr *attr)
{
    return attr->val;
}

#endif /* __ATTR_H_INCLUDED__ */
