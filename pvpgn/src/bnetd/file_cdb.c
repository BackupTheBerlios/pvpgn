/*
 * Copyright (C) 2003,2004 Dizzy 
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
#include <stdio.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include <ctype.h>
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include "compat/char_bit.h"
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <errno.h>
#include "compat/strerror.h"
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
# ifdef WIN32
#  include <io.h>
# endif
#endif
#include "compat/pdir.h"
#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"
#include "common/field_sizes.h"
#include "common/bnethash.h"
#include "common/introtate.h"
#define CLAN_INTERNAL_ACCESS
#define ACCOUNT_INTERNAL_ACCESS
#include "account.h"
#include "common/hashtable.h"
#include "storage.h"
#include "storage_file.h"
#include "common/list.h"
#include "common/xalloc.h"
#include "connection.h"
#include "watch.h"
#include "clan.h"
#undef ACCOUNT_INTERNAL_ACCESS
#undef CLAN_INTERNAL_ACCESS
#include "common/tag.h"
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
# endif
#endif
#include "tinycdb/cdb.h"
#include "common/setup_after.h"

/* cdb file storage API functions */

static int cdb_read_attrs(const char *filename, t_read_attr_func cb, void *data);
static void * cdb_read_attr(const char *filename, const char *key);
static int cdb_write_attrs(const char *filename, void *attributes);

/* file_engine struct populated with the functions above */

t_file_engine file_cdb = {
    cdb_read_attr,
    cdb_read_attrs,
    cdb_write_attrs
};

/* start of actual cdb file storage code */

//#define CDB_ON_DEMAND	1

static int cdb_write_attrs(const char *filename, void *attributes)
{
    FILE           *cdbfile;
    t_attribute    *attr;
    struct cdb_make cdbm;

    if ((cdbfile = fopen(filename, "w+b")) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "unable to open file \"%s\" for writing ",filename);
	return -1;
    }

    cdb_make_start(&cdbm, cdbfile);
    for (attr=(t_attribute *)attributes; attr; attr=attr->next) {
	if (attr->key && attr->val) {
	    if (strncmp("BNET\\CharacterDefault\\", attr->key, 20) == 0) {
		eventlog(eventlog_level_debug, __FUNCTION__, "skipping attribute key=\"%s\"",attr->key);
	    } else {
		eventlog(eventlog_level_debug, __FUNCTION__, "saving attribute key=\"%s\" val=\"%s\"",attr->key,attr->val);
		if (cdb_make_add(&cdbm, attr->key, strlen(attr->key), attr->val, strlen(attr->val))<0)
		{
		    eventlog(eventlog_level_error, __FUNCTION__, "got error on cdb_make_add ('%s' = '%s')", attr->key, attr->val);
		    cdb_make_finish(&cdbm); /* try to bail out nicely */
		    fclose(cdbfile);
		    return -1;
		}
	    }
	} else eventlog(eventlog_level_error, __FUNCTION__,"could not save attribute key=\"%s\"",attr->key);

    }

    if (cdb_make_finish(&cdbm)<0) {
	eventlog(eventlog_level_error, __FUNCTION__, "got error on cdb_make_finish");
	fclose(cdbfile);
	return -1;
    }

    if (fclose(cdbfile)<0) {
	eventlog(eventlog_level_error, __FUNCTION__, "got error on fclose()");
	return -1;
    }

    return 0;
}

#ifndef CDB_ON_DEMAND
/* code adapted from tinycdb-0.73/cdb.c */

static int fget(FILE * fd, unsigned char *b, cdbi_t len, cdbi_t *posp, cdbi_t limit)
{
    if (posp && limit - *posp < len) {
	eventlog(eventlog_level_error, __FUNCTION__, "invalid cdb database format");
	return -1;
    }

    if (fread(b, 1, len, fd) != len) {
	if (ferror(fd)) {
	    eventlog(eventlog_level_error, __FUNCTION__, "got error reading from db file");
	    return -1;
	}
	eventlog(eventlog_level_error, __FUNCTION__, "unable to read from cdb file, incomplete file");
	return -1;
    }

    if (posp) *posp += len;
    return 0;
}

static const char * fcpy(FILE *fd, cdbi_t len, cdbi_t *posp, cdbi_t limit, unsigned char * buf)
{
    static char *str;
    static unsigned strl;
    unsigned int res = 0, no = 0;

    if (strl < len + 1) {
	char *tmp;
	if ((tmp = xmalloc(len + 1)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "could not alloc memory to read from db");
	    return NULL;
	}

	if (str) xfree((void*)str);
	str = tmp;
	strl = len + 1;
    }

    while(len - res > 0) {
	if (len > 2048) no = 2048;
	else no = len;

	fget(fd, buf, no, posp, limit);
	memmove(str + res, buf, no);
	res += no;
    }

    if (res > strl - 1) {
	eventlog(eventlog_level_error, __FUNCTION__, "BUG, this should nto happen");
	return NULL;
    }

    str[res] = '\0';
    return str;
}

static int cdb_read_attrs(const char *filename, t_read_attr_func cb, void *data)
{
    cdbi_t eod, klen, vlen;
    cdbi_t pos = 0;
    const char *key;
    const char *val;
    unsigned char buf[2048];
    FILE *f;

    if ((f = fopen(filename, "rb")) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got error opening file '%s'", filename);
	return -1;
    }

    fget(f, buf, 2048, &pos, 2048);
    eod = cdb_unpack(buf);
    while(pos < eod) {
	fget(f, buf, 8, &pos, eod);
	klen = cdb_unpack(buf);
	vlen = cdb_unpack(buf + 4);
	if ((key = fcpy(f, klen, &pos, eod, buf)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "error reading attribute key");
	    fclose(f);
	    return -1;
	}

	if ((key = xstrdup(key)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "error duplicating attribute key");
	    fclose(f);
	    return -1;
	}

	if ((val = fcpy(f, vlen, &pos, eod, buf)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "error reading attribute val");
	    xfree((void *)key);
	    fclose(f);
	    return -1;
	}

//	eventlog(eventlog_level_trace, __FUNCTION__, "read atribute : '%s' -> '%s'", key, val);
	if (cb(key, val, data))
	    eventlog(eventlog_level_error, __FUNCTION__, "got error from callback on account file '%s'", filename);
	xfree((void *)key);
    }

    fclose(f);
    return 0;
}
#else /* CDB_ON_DEMAND */
static int cdb_read_attrs(const char *filename, t_read_attr_func cb, void *data)
{
    return 0;
}
#endif

static void * cdb_read_attr(const char *filename, const char *key)
{
#ifdef CDB_ON_DEMAND
    FILE	*cdbfile;
    t_attribute	*attr;
    char	*val;
    unsigned	vlen = 1;

//    eventlog(eventlog_level_trace, __FUNCTION__, "reading key '%s'", key);
    if ((cdbfile = fopen(filename, "rb")) == NULL) {
//	eventlog(eventlog_level_debug, __FUNCTION__, "unable to open file \"%s\" for reading ",filename);
	return NULL;
    }

    if (cdb_seek(cdbfile, key, strlen(key), &vlen) <= 0) {
//	eventlog(eventlog_level_debug, __FUNCTION__, "could not find key '%s'", key);
	fclose(cdbfile);
	return NULL;
    }

    if ((attr = xmalloc(sizeof(t_attribute))) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for attr result");
	fclose(cdbfile);
	return NULL;
    }

    if ((attr->key = xstrdup(key)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for attr key result");
	xfree((void*)attr);
	fclose(cdbfile);
	return NULL;
    }

    if ((val = xmalloc(vlen + 1)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for attr val result");
	xfree((void*)attr->key);
	xfree((void*)attr);
	fclose(cdbfile);
	return NULL;
    }

    cdb_bread(cdbfile, val, vlen);
    fclose(cdbfile);

    val[vlen] = '\0';

    attr->val = val;
    attr->dirty = 0;
//    eventlog(eventlog_level_trace, __FUNCTION__, "read key '%s' value '%s'", attr->key, attr->val);
    return attr;
#else
    return NULL;
#endif
}

