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

#include "common/setup_before.h"
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "common/eventlog.h"
#include "common/flags.h"
#include "common/xalloc.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "storage.h"
#include "prefs.h"
#include "server.h"
#define ATTRLIST_INTERNAL_ACCESS
#include "attr.h"
#include "common/setup_after.h"

static t_attrlist *defattrs = NULL;

static DECLARE_ELIST_INIT(loadedlist);
static DECLARE_ELIST_INIT(dirtylist);

static int attrlayer_unload_default(void);
static int attrlist_load(t_attrlist *);
static int attrlist_unload(t_attrlist *);

static inline void attr_set_val(t_attr *attr, const char *val)
{
    if (attr->val) xfree((void*)attr->val);

    if (val) attr->val = xstrdup(val);
    else attr->val = NULL;
}

static inline void attr_set_dirty(t_attr *attr)
{
    attr->dirty = 1;
}

static inline void attrlist_set_accessed(t_attrlist *attrlist)
{
    FLAG_SET(&attrlist->flags, ATTRLIST_FLAG_ACCESSED);
    attrlist->lastaccess = now;
}

static inline void attrlist_clear_accessed(t_attrlist *attrlist)
{
    FLAG_CLEAR(&attrlist->flags, ATTRLIST_FLAG_ACCESSED);
}

static inline void attrlist_set_dirty(t_attrlist *attrlist)
{
    if (FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_DIRTY)) return;

    attrlist->dirtytime = now;
    FLAG_SET(&attrlist->flags, ATTRLIST_FLAG_DIRTY);
    elist_add_tail(&dirtylist, &attrlist->dirtylist);
}

static inline void attrlist_clear_dirty(t_attrlist *attrlist)
{
    if (!FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_DIRTY)) return;

    FLAG_CLEAR(&attrlist->flags, ATTRLIST_FLAG_DIRTY);
    elist_del(&attrlist->dirtylist);
}

static inline void attrlist_set_loaded(t_attrlist *attrlist)
{
    if (FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_LOADED)) return;
    FLAG_SET(&attrlist->flags, ATTRLIST_FLAG_LOADED);

    if (attrlist == defattrs) return;	/* don't add defattrs to loadedlist */
    elist_add_tail(&loadedlist, &attrlist->loadedlist);
}

static inline void attrlist_clear_loaded(t_attrlist *attrlist)
{
    if (!FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_LOADED)) return;

    /* clear this because they are not valid if attrlist is unloaded */
    attrlist_clear_dirty(attrlist);
    attrlist_clear_accessed(attrlist);

    FLAG_CLEAR(&attrlist->flags, ATTRLIST_FLAG_LOADED);
    if (attrlist == defattrs) return;	/* defattrs is not in loadedlist */
    elist_del(&attrlist->loadedlist);
}

static t_attrlist * attrlist_create(void)
{
    t_attrlist *attrlist;

    attrlist = xmalloc(sizeof(t_attrlist));

    hlist_init(&attrlist->list);
    attrlist->storage = NULL;
    attrlist->flags = ATTRLIST_FLAG_NONE;
    attrlist->lastaccess = 0;
    attrlist->dirtytime = 0;
    elist_init(&attrlist->loadedlist);
    elist_init(&attrlist->dirtylist);

    return attrlist;
}

static t_attrlist * attrlist_create_storage(t_storage_info *storage)
{
    t_attrlist *attrlist;

    attrlist = attrlist_create();
    attrlist->storage = storage;

    return attrlist;
}

extern t_attrlist * attrlist_create_newuser(const char *name)
{
    t_attrlist *attrlist;
    t_storage_info *stmp;

    stmp = storage->create_account(name);
    if (!stmp) {
	eventlog(eventlog_level_error,__FUNCTION__,"failed to add user '%s' to storage", name);
	return NULL;
    }

    attrlist = attrlist_create_storage(stmp);

    /* new accounts are born loaded */
    attrlist_set_loaded(attrlist);

    return attrlist;
}

extern t_attrlist * attrlist_create_nameuid(const char *name, unsigned uid)
{
    t_storage_info *info;
    t_attrlist *attrlist;

    info = storage->read_account(name,uid);
    if (!info) return NULL;

    attrlist = attrlist_create_storage(info);

    return attrlist;
}

extern int attrlist_destroy(t_attrlist *attrlist)
{
    if (!attrlist) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attrlist");
	return -1;
    }

    attrlist_unload(attrlist);
    if (attrlist->storage) storage->free_info(attrlist->storage);
    xfree(attrlist);

    return 0;
}

extern int attrlist_save(t_attrlist *attrlist, int flags)
{
    if (!attrlist) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attrlist");
	return -1;
    }

    if (!FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_LOADED))
	return 0;

    if (!FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_DIRTY))
	return 0;

    if (!FLAG_ISSET(flags, FS_FORCE) && now - attrlist->dirtytime < prefs_get_user_sync_timer())
	return 0;

    assert(attrlist->storage);

    storage->write_attrs(attrlist->storage, &attrlist->list);
    attrlist_clear_dirty(attrlist);

    return 1;
}

extern int attrlist_flush(t_attrlist *attrlist, int flags)
{
    t_attr *attr;
    t_hlist *curr, *save;

    if (!attrlist) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attrlist");
	return -1;
    }

    if (!FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_LOADED))
	return 0;

    if (!FLAG_ISSET(flags, FS_FORCE) &&
	FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_ACCESSED) &&
	now - attrlist->lastaccess < prefs_get_user_flush_timer())
	return 0;

    /* sync data to disk if dirty */
    attrlist_save(attrlist,FS_FORCE);

    hlist_for_each_safe(curr,&attrlist->list,save) {
	attr = hlist_entry(curr,t_attr,link);
	attr_destroy(attr);
    }
    hlist_init(&attrlist->list);	/* reset list */

    attrlist_clear_loaded(attrlist);

    return 1;
}

static int _cb_load_attr(const char *key, const char *val, void *data)
{
    t_attrlist *attrlist = (t_attrlist *)data;

    return attrlist_set_attr(attrlist, key, val);
}

static int attrlist_load(t_attrlist *attrlist)
{
    assert(attrlist);
    assert(attrlist->storage);

    if (FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_LOADED))	/* already done */
	return 0;
    if (FLAG_ISSET(attrlist->flags, ATTRLIST_FLAG_DIRTY)) { /* if not loaded, how dirty ? */
	eventlog(eventlog_level_error, __FUNCTION__, "can't load modified account");
	return -1;
    }

    attrlist_set_loaded(attrlist);
    if (storage->read_attrs(attrlist->storage, _cb_load_attr, attrlist)) {
	eventlog(eventlog_level_error, __FUNCTION__, "got error loading attributes");
	return -1;
    }
    attrlist_clear_dirty(attrlist);

    return 0;
}

static int attrlist_unload(t_attrlist *attrlist)
{
    attrlist_flush(attrlist,FS_FORCE);

    return 0;
}

typedef struct {
    void *data;
    t_attr_cb cb;
} t_attr_cb_data;

static int _cb_read_accounts(t_storage_info *info, void *data)
{
    t_attrlist *attrlist;
    t_attr_cb_data *cbdata = (t_attr_cb_data*)data;

    attrlist = attrlist_create_storage(info);
    return cbdata->cb(attrlist,cbdata->data);
}

extern int attrlist_read_accounts(int flag, t_attr_cb cb, void *data)
{
    t_attr_cb_data cbdata;

    cbdata.cb = cb;
    cbdata.data = data;

    return storage->read_accounts(flag, _cb_read_accounts, &cbdata);
}

extern int attrlayer_init(void)
{
    elist_init(&loadedlist);
    elist_init(&dirtylist);
    attrlayer_load_default();

    return 0;
}

extern int attrlayer_cleanup(void)
{
    attrlayer_flush(FS_FORCE | FS_ALL);
    attrlayer_unload_default();

    return 0;
}

extern int attrlayer_load_default(void)
{
    if (defattrs) attrlayer_unload_default();

    defattrs = attrlist_create_storage(storage->get_defacct());
    if (!defattrs) {
	eventlog(eventlog_level_error,__FUNCTION__,"could not create attrlist");
	return -1;
    }

    return attrlist_load(defattrs);
}

static int attrlayer_unload_default(void)
{
    attrlist_destroy(defattrs);
    defattrs = NULL;

    return 0;
}

/* FIXME: dont copy most of the functionality, a good place for a C++ template ;) */

extern int attrlayer_flush(int flags)
{
    static t_elist *curr = &loadedlist;
    static t_elist *next = NULL;
    t_attrlist *attrlist;
    unsigned int fcount;
    unsigned int tcount;

    fcount = tcount = 0;
    if (curr == &loadedlist || FLAG_ISSET(flags, FS_ALL)) {
	curr = elist_next(&loadedlist);
	next = elist_next(curr);
    }

    /* elist_for_each_safe splitted into separate startup for userstep function */
    for (; curr != &loadedlist; curr = next, next = hlist_next(curr)) {
	if (!FLAG_ISSET(flags, FS_ALL) && tcount >= prefs_get_user_step()) break;

	attrlist = elist_entry(curr, t_attrlist, loadedlist);
	switch(attrlist_flush(attrlist, flags)) {
	    case -1:
		eventlog(eventlog_level_error, __FUNCTION__, "could not flush account");
		break;
	    case 1:
		fcount++;
		break;
	    case 0:
	    default:
		break;
	}
	tcount++;
    }

    if (fcount>0)
	eventlog(eventlog_level_debug, __FUNCTION__, "flushed %u of %u user accounts", fcount, tcount);

    if (!FLAG_ISSET(flags, FS_ALL) && curr != &loadedlist) return 1;

    return 0;
}

extern int attrlayer_save(int flags)
{
    static t_elist *curr = &dirtylist;
    static t_elist *next = NULL;
    t_attrlist *attrlist;
    unsigned int scount;
    unsigned int tcount;

    scount = tcount = 0;
    if (curr == &dirtylist || FLAG_ISSET(flags, FS_ALL)) {
	curr = elist_next(&dirtylist);
	next = elist_next(curr);
    }

    /* elist_for_each_safe splitted into separate startup for userstep function */
    for (; curr != &dirtylist; curr = next, next = hlist_next(curr)) {
	if (!FLAG_ISSET(flags, FS_ALL) && tcount >= prefs_get_user_step()) break;

	attrlist = elist_entry(curr, t_attrlist, dirtylist);
	switch(attrlist_save(attrlist, flags)) {
	    case -1:
		eventlog(eventlog_level_error, __FUNCTION__, "could not save account");
		break;
	    case 1:
		scount++;
		break;
	    case 0:
	    default:
		break;
	}
	tcount++;
    }

    if (scount>0)
	eventlog(eventlog_level_debug, __FUNCTION__, "saved %u of %u user accounts", scount, tcount);

    if (!FLAG_ISSET(flags, FS_ALL) && curr != &dirtylist) return 1;

    return 0;
}

static const char *attrlist_escape_key(const char *key)
{
    const char *newkey, *newkey2;
    char *tmp;

    newkey = key;

    if (!strncasecmp(key,"DynKey",6)) {
	/* OLD COMMENT, MIGHT NOT BE VALID ANYMORE
	 * Recent Starcraft clients seems to query DynKey\*\1\rank instead of 
	 * Record\*\1\rank. So replace Dynkey with Record for key lookup.
	 */
	tmp = xstrdup(key);
	strncpy(tmp,"Record",6);
	newkey = tmp;
    } else if (!strncmp(key,"Star",4)) {
	/* OLD COMMENT
	 * Starcraft clients query Star instead of STAR on logon screen.
	 */
	tmp = xstrdup(key);
	strncpy(tmp,"STAR",4);
	newkey = tmp;
    }

    if (newkey != key) {
	newkey2 = storage->escape_key(newkey);
	if (newkey2 != newkey) {
	    xfree((void*)newkey);
	    newkey = newkey2;
	}
    } else newkey = storage->escape_key(key);

    return newkey;
}

static t_attr *attrlist_find_attr(t_attrlist *attrlist, const char *key)
{
    const char *newkey, *val;
    t_hlist *curr, *last, *last2;
    t_attr *attr;

    assert(attrlist);
    assert(key);

    /* trigger loading of attributes if not loaded already */
    if (attrlist_load(attrlist)) return NULL;	/* eventlog happens earlier */

    newkey = attrlist_escape_key(key);

    /* we are doing attribute lookup so we are accessing it */
    attrlist_set_accessed(attrlist);

    last = &attrlist->list;
    last2 = NULL;

    hlist_for_each(curr,&attrlist->list) {
	attr = hlist_entry(curr, t_attr, link);

	if (!strcasecmp(attr_get_key(attr),newkey)) {
	    val = attr_get_val(attr);
	    /* key found, promote it so it's found faster next time */
	    hlist_promote(curr, last, last2);
	    break;
	}

	last2 = last; last = curr;
    }

    if (curr == &attrlist->list) {	/* no key found in cached list */
	attr = (t_attr*)storage->read_attr(attrlist->storage, newkey);
	if (attr) hlist_add(&attrlist->list, &attr->link);
    }

    if (newkey != key) xfree((void*)newkey);

    /* "attr" here can either have a proper value found in the cached list, or
     * a value returned by storage->read_attr, or NULL */
    return attr;
}

extern const char *attrlist_get_attr(t_attrlist *attrlist, const char *key)
{
    const char *val = NULL;
    t_attr *attr;

    if (!attrlist) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attrlist");
	return NULL;
    }

    if (!key) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL key");
	return NULL;
    }

    attr = attrlist_find_attr(attrlist, key);

    if (attr) val = attr_get_val(attr);

    if (!val && attrlist != defattrs) val = attrlist_get_attr(defattrs, key);

    return val;
}

extern int attrlist_set_attr(t_attrlist *attrlist, const char *key, const char *val)
{
    t_attr *attr;

    if (!attrlist) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attrlist");
	return -1;
    }

    if (!key) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL key");
	return -1;
    }

    attr = attrlist_find_attr(attrlist, key);

    if (attr) {
	if (attr_get_val(attr) == val ||
	    (attr_get_val(attr) && val && !strcmp(attr_get_val(attr), val)))
	    return 0;	/* no need to modify anything, values are the same */

	/* new value for existent key, replace the old one */
	attr_set_val(attr, val);
    } else {	/* unknown key so add new attr */
	attr = attr_create(key, val);
	hlist_add(&attrlist->list, &attr->link);
    }

    /* we have modified this attr and attrlist */
    attr_set_dirty(attr);
    attrlist_set_dirty(attrlist);

    return 0;
}

