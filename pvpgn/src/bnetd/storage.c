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
#ifdef WITH_MYSQL

#include "common/setup_before.h"
#include "prefs.h"
#include "pvpgn_mysql.h"
#include "compat/strchr.h"
#include "compat/char_bit.h"
#include "common/introtate.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "storage.h"
#include "common/setup_after.h"

extern int storage_init(void)
{
   if (db_check() < 0) {
      eventlog(eventlog_level_error, "storage_init", "Could not initialize sql layer");
      return -1;
   }
   return 0;
}

extern void storage_destroy(void)
{
    if (prefs_get_mysql_persistent()) {
	db_close();
    }
}

extern unsigned int storage_create_account(const char * username)
{
   /* there is nothing to cache here, there should not be too many
    * new accounts per second
    */
   unsigned int result;

   if (db_init()<0) {
      eventlog(eventlog_level_error, "storage_create_account", "db_init() failed");
      return 0;
   }

   result = db_new_user(username);
   
   if (!prefs_get_mysql_persistent()) {
     db_close();
   }
   
   return result;
}

extern t_readattr * storage_attr_getfirst(unsigned int sid, char **pkey, char **pvalue)
{
   t_readattr * readattr;

   eventlog(eventlog_level_trace, "storage_attr_getfirst", "<<<< ENTER ! (uid: %u)", sid);
   if (db_init()<0) {
      eventlog(eventlog_level_error, "storage_attr_getfirst", "db_init() failed");
      return NULL;
   }

   if ((readattr = malloc(sizeof(t_readattr))) == NULL) {
      eventlog(eventlog_level_error,"storage_attr_getfirst","could not allocate for attr list");
      if (!prefs_get_mysql_persistent()) {
         db_close();
      }
      return NULL;
   }
   
   if ((readattr->alist = db_get_attributes(sid)) == NULL) {
      eventlog(eventlog_level_error,"storage_attr_getfirst","could not retrieve attribute list");
      free(readattr);
      if (!prefs_get_mysql_persistent()) {
         db_close();
      }
      return NULL;
   }
   
   if (!prefs_get_mysql_persistent()) {
      db_close();
   }
   readattr->pos = 0; /* start from beginning */
   
   *pkey = readattr->alist->keys[0];
   *pvalue = readattr->alist->values[0];
   
   return readattr;
}

extern int storage_attr_getnext(t_readattr * readattr, char **pkey, char **pvalue)
{
   if (readattr == NULL) {
      eventlog(eventlog_level_error,"storage_attr_getnext", "Got NULL readattr");
      return -1;
   }
   
   if (readattr->pos >= readattr->alist->len -1) /* we finished browsing */
     return -1;
   
   readattr->pos++;
   *pkey = readattr->alist->keys[readattr->pos];
   *pvalue = readattr->alist->values[readattr->pos];
   
   return 0;
}

extern int storage_attr_close(t_readattr * readattr)
{
   if (readattr == NULL) {
      eventlog(eventlog_level_error,"storage_attr_close", "Got NULL readattr");
      return -1;
   }
   
   if (readattr->alist != NULL) db_free_attributes(readattr->alist);
   free(readattr);
   return 0;
}

extern t_readacct * storage_account_getfirst(unsigned int *puid)
{
   t_readacct *readacct;
   
   if (db_init()<0) {
      eventlog(eventlog_level_error, "storage_account_getfirst", "db_init() failed");
      return NULL;
   }

   if ((readacct = malloc(sizeof(t_readacct))) == NULL) {
      eventlog(eventlog_level_error,"storage_account_getfirst","could not allocate for readacct");
      if (!prefs_get_mysql_persistent()) {
         db_close();
      }
      return NULL;
   }
   
   if ((readacct->ulist = db_get_accounts()) == NULL) {
      eventlog(eventlog_level_error,"storage_account_getfirst","could not get the account list");
      free(readacct);
      if (!prefs_get_mysql_persistent()) {
         db_close();
      }
      return NULL;
   }
   
   if (!prefs_get_mysql_persistent()) {
      db_close();
   }
   
   readacct->pos = 0;
   *puid = readacct->ulist->uids[0];

   return readacct;
}

extern int storage_account_getnext(t_readacct *readacct, unsigned int *puid)
{
   if (readacct == NULL) {
      eventlog(eventlog_level_error,"storage_account_getnext", "Got NULL readacct");
      return -1;
   }
   
   if (readacct->pos >= readacct->ulist->len - 1)
     return -1;
   
   readacct->pos++;
   *puid = readacct->ulist->uids[readacct->pos];
   
   return 0;
}

extern int storage_account_close(t_readacct * readacct)
{
   if (readacct == NULL) {
      eventlog(eventlog_level_error,"storage_account_close","Got NULL readacct");
      return -1;
   }
   
   if (readacct->ulist) db_free_accounts(readacct->ulist);
   free(readacct);

   return 0;
}

extern int storage_set(unsigned int sid, const char *key, const char *val)
{
   int result;
   
   if (key == NULL) {
      eventlog(eventlog_level_error,"storage_set", "got NULL key");
      return -1;
   }
   
   if (val == NULL) {
      eventlog(eventlog_level_error,"storage_set", "got NULL val");
      return -1;
   }
   
   if (db_init()<0) {
      eventlog(eventlog_level_error,"storage_set","faild to init db");
      return -1;
   }
   
   result = db_set(sid, key, val);
   
   if (!prefs_get_mysql_persistent()) {
      db_close();
   }
   
   return result;
}

#endif
