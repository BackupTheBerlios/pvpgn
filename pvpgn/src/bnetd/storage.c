/*
   * Copyright (C) 2002,2003 Dizzy 
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
#include "compat/strcasecmp.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "storage.h"
#include "storage_file.h"
#ifdef WITH_SQL
#include "storage_sql.h"
#endif

#include "compat/strdup.h"
#include "common/eventlog.h"
#include "common/setup_after.h"

t_storage *storage = NULL;

extern int storage_init(const char *spath)
{
    char *temp, *p;
    int res;

    if (spath == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL spath");
	return -1;
    }

    if ((temp = strdup(spath)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not duplicate spath");
	return -1;
    }

    if ((p = strchr(spath, ':')) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "malformed storage_path , driver not found");
	free((void*)temp);
	return -1;
    }

    *p = '\0';
    if (strcasecmp(spath, "file") == 0) {
	storage = &storage_file;
	res = storage->init(p + 1);
    }
#ifdef WITH_SQL
    else if (strcasecmp(spath, "sql") == 0) {
	storage = &storage_sql;
	res = storage->init(p + 1);
    }
#endif

    free((void*)temp);

    return res;
}

extern void storage_close(void)
{
    storage->close();
}

