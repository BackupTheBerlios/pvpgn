/*
 * Copyright (C) 2002 TheUndying
 * Copyright (C) 2002 zap-zero
 * Copyright (C) 2002,2003 Dizzy 
 * Copyright (C) 2002 Zzzoom
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

#ifdef WITH_SQL

#include "common/setup_before.h"
#include <stdio.h>

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

#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"

#define ACCOUNT_INTERNAL_ACCESS
#include "account.h"
#undef ACCOUNT_INTERNAL_ACCESS

#include "compat/strdup.h"
#include "common/tag.h"
#include "storage_sql.h"
#ifdef WITH_SQL_MYSQL
#include "sql_mysql.h"
#endif
#include "common/setup_after.h"

#define CURRENT_DB_VERSION 150

#define DB_MAX_ATTRKEY	128
#define DB_MAX_ATTRVAL  180
#define DB_MAX_TAB	64

#define SQL_UID_FIELD		"uid"
#define STORAGE_SQL_DEFAULT_UID	0

static int sql_init(const char *);
static int sql_close(void);
static t_storage_info * sql_create_account(char const *);
static t_storage_info * sql_get_defacct(void);
static int sql_free_info(t_storage_info *);
static int sql_read_attrs(t_storage_info *, t_read_attr_func);
static int sql_write_attrs(t_storage_info *, void *);
static int sql_read_accounts(t_read_accounts_func);
static int sql_cmp_info(t_storage_info *, t_storage_info *);
static const char * sql_escape_key(const char *);

t_storage storage_sql = {
    sql_init,
    sql_close,
    sql_create_account,
    sql_get_defacct,
    sql_free_info,
    sql_read_attrs,
    sql_write_attrs,
    sql_read_accounts,
    sql_cmp_info,
    sql_escape_key
};

static t_sql_engine *sql = NULL;

static char * tables[] = {"BNET", "Record", "profile", "friend", "Team", NULL};

static int _dbcreator_init();
static int _dbcreator_createdatabase(char const *database);
static int _dbcreator_filldb(char const *database);

void update_DB_v0_to_v150(void);

static const char * _db_add_tab(const char *tab, const char *key)
{
   static char nkey[DB_MAX_ATTRKEY];
   char *p;
   
   strncpy(nkey, tab, sizeof(nkey) - 1);
   nkey[strlen(nkey) + 1] = '\0'; nkey[strlen(nkey)] = '_';
   strncpy(nkey + strlen(nkey), key, sizeof(nkey) - strlen(nkey));
/*
   for(p = nkey; *p; p++)
     if (*p == '_') *p = '\\';
*/
   return nkey;
}

static int _db_get_tab(const char *key, char **ptab, char **pcol)
{
   static char tab[DB_MAX_ATTRKEY];
   static char col[DB_MAX_ATTRKEY];
   char *p;
   
   strncpy(tab, key, DB_MAX_TAB-1);
   tab[DB_MAX_TAB-1]=0;

   if(!strchr(tab, '_')) return -1;

   
   *(strchr(tab, '_')) = 0;
   strncpy(col, key+strlen(tab)+1, DB_MAX_TAB-1);
   col[DB_MAX_TAB-1]=0;
/*
   for(p=col; *p; p++)
     if(*p == '\\' || *p == ' ' || *p == '-')
       *p = '_';
*/
   /* return tab and col as 2 static buffers */
   *ptab = tab;
   *pcol = col;
   return 0;
}

static int sql_init(const char *dbpath)
{
    char *tok, *path, *tmp, *p;
    const char *dbhost = NULL;
    const char *dbname = NULL;
    const char *dbuser = NULL;
    const char *dbpass = NULL;
    const char *driver = NULL;
    const char *dbport = NULL;
    const char *dbsocket = NULL;

    if ((path = strdup(dbpath)) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "could not duplicate dbpath");
	return -1;
    }

    tmp = path;
    while((tok = strtok(tmp, ";")) != NULL) {
	tmp = NULL;
	if ((p = strchr(tok, '=')) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "invalid storage_path, no '=' present in token");
	    free((void*)path);
	    return -1;
	}
	*p = '\0';
	if (strcasecmp(tok, "host") == 0)
	    dbhost = p + 1;
	else if (strcasecmp(tok, "mode") == 0)
	    driver = p + 1;
	else if (strcasecmp(tok, "name") == 0)
	    dbname = p + 1;
	else if (strcasecmp(tok, "port") == 0)
	    dbport = p + 1;
	else if (strcasecmp(tok, "socket") == 0)
	    dbsocket = p + 1;
	else if (strcasecmp(tok, "user") == 0)
	    dbuser = p + 1;
	else if (strcasecmp(tok, "pass") == 0)
	    dbpass = p + 1;
    }

    if (driver == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "no mode specified");
	free((void*)path);
	return -1;
    }

    do {
#ifdef WITH_SQL_MYSQL
	if (strcasecmp(driver, "mysql") == 0) {
	    sql = &sql_mysql;
	    if (sql->init(dbhost, dbport, dbsocket, dbname, dbuser, dbpass)) {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		free((void*)path);
		return -1;
	    }
	    break;
	} 
#endif /* WITH_SQL_MYSQL */
#ifdef WITH_SQL_PGSQL
	if (strcasecmp(driver, "pgsql") == 0) {
	    sql = &sql_pgsql;
	    if (sql->init(dbhost, dbport, dbsocket, dbname, dbuser, dbpass)) {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		free((void*)path);
		return -1;
	    }
	    break;
	}
#endif /* WITH_SQL_PGSQL */
	eventlog(eventlog_level_error, __FUNCTION__, "no driver found for '%s'", driver);
	free((void*)path);
	return -1;
    }while(0);

    free((void*)path);

    return 0;
}

static int sql_close(void)
{
    if (sql == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "sql not initilized");
	return -1;
    }

    sql->close();
    sql = NULL;
    return 0;
}

static t_storage_info * sql_create_account(char const * username)
{
    char query[1024];
    t_sql_res * result = NULL;
    t_sql_row * row;
    int uid = maxuserid + 1;
    char str_uid[32];
    t_storage_info *info;

    if(!sql) {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return NULL;
    }

    sprintf(str_uid, "%u", uid);

    sprintf(query, "SELECT count(*) FROM BNET WHERE acct_username='%s'", username);
    if((result = sql->query_res(query)) != NULL) {
            int num;

            row = sql->fetch_row(result);
	    if (row == NULL || row[0] == NULL) {
		sql->free_result(result);
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL count");
		return NULL;
	    }
            num = atol(row[0]);
            sql->free_result(result);
            if (num>0) {
		eventlog(eventlog_level_error, __FUNCTION__, "got existant username");
		return NULL;
	    }
        }
    else {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	return NULL;
    }

    if ((info = malloc(sizeof(t_sql_info))) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for sql info");
	return NULL;
    }

    *((unsigned int*)info) = uid;
/*
    sprintf(query, "UPDATE counters SET max_uid='%s';", str_uid);
    if (sql->query(query)) {
        eventlog(eventlog_level_error, __FUNCTION__, "uid update failed");
	free((void*)info->sql);
	free((void*)info);
	return NULL;
    }
*/
    sprintf(query, "INSERT INTO BNET (uid) VALUES('%s');", str_uid);
    if (sql->query(query)) {
        eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	free((void*)info);
	return NULL;
    }
    sprintf(query, "INSERT INTO profile (uid) VALUES('%s');", str_uid);
    if (sql->query(query)) {
        eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	free((void*)info);
	return NULL;
    }
    sprintf(query, "INSERT INTO Record (uid) VALUES('%s');", str_uid);
    if (sql->query(query)) {
        eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	free((void*)info);
	return NULL;
    }
    sprintf(query, "INSERT INTO friend (uid) VALUES('%s');", str_uid);
    if (sql->query(query)) {
        eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	free((void*)info);
	return NULL;
    }

    return info;
}

static int sql_read_attrs(t_storage_info *info, t_read_attr_func cb)
{
    char query[1024];
    t_sql_res * result = NULL;
    t_sql_row * row;
    char **tab;
    unsigned int maxlen, uid;

    if(!sql) {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }
   
    if (info == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL storage info");
	return -1;
    }
   
    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL callback");
	return -1;
    }

    uid = *((unsigned int *)info);

    for(tab = tables; *tab ; tab++) {
	sprintf(query,"SELECT * FROM %s WHERE uid='%u'", *tab, uid);

//	eventlog(eventlog_level_trace, __FUNCTION__, "query: \"%s\"",query);

	if ((result = sql->query_res(query)) != NULL && sql->num_rows(result) == 1 && sql->num_fields(result)>1) {
	    unsigned int i;
	    t_sql_field *fields, *fentry;

	    if ((fields = sql->fetch_fields(result)) == NULL) {
		eventlog(eventlog_level_error,"db_get_attributes","could not fetch the fields");
		sql->free_result(result);
		return -1;
	    }

	    if (!(row = sql->fetch_row(result))) {
		eventlog(eventlog_level_error, __FUNCTION__, "could not fetch row");
		sql->free_fields(fields);
		sql->free_result(result);
		return -1;
	    }
	 
	    for(i = 0, fentry = fields; *fentry ; fentry++, i++) { /* we have to skip "uid" */
		/* we ignore the field used internally by sql */
		if (strcmp(*fentry, SQL_UID_FIELD) == 0) continue;

//		eventlog(eventlog_level_trace, __FUNCTION__, "read key (step1): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
		if (row[i] == NULL) continue; /* its an NULL value sql field */

//		eventlog(eventlog_level_trace, __FUNCTION__, "read key (step2): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
		cb(_db_add_tab(*tab, *fentry), unescape_chars(row[i]));
//		eventlog(eventlog_level_trace, __FUNCTION__, "read key (final): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
	    }

	    sql->free_fields(fields);
	    sql->free_result(result);
	}
    }

    return 0;
}

/* write ONLY dirty attributes */
int sql_write_attrs(t_storage_info *info, void *attrs)
{
    char query[1024];
    char escape[DB_MAX_ATTRVAL * 2 + 1]; /* sql docs say the escape can take a maximum of double original size + 1*/
    char safeval[DB_MAX_ATTRVAL];
    char *p, *tab, *col;
    t_attribute *attr;
    unsigned int uid;

    if(!sql) {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
        return -1;
    }

    if (info == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL sql info");
        return -1;
    }

    if (attrs == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL attributes list");
        return -1;
    }

    uid = *((unsigned int *)info);

    for(attr=(t_attribute *)attrs; attr; attr = attr->next) {
	if (!attr->dirty) continue; /* save ONLY dirty attributes */

	if (attr->key == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "found NULL key in attributes list");
	    continue;
	}

	if (attr->val == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "found NULL value in attributes list");
	    continue;
	}

	if (_db_get_tab(attr->key, &tab, &col)<0) {
	    eventlog(eventlog_level_error, __FUNCTION__, "error from db_get_tab");
	    continue;
	}

        strncpy(safeval, attr->val, DB_MAX_ATTRVAL-1);
	safeval[DB_MAX_ATTRVAL-1]=0;
	for(p=safeval; *p; p++)
	    if(*p == '\'')              /* value shouldn't contain ' */
		*p = '"';
   
	sql->escape_string(escape, safeval, strlen(safeval));

	strcpy(query, "UPDATE ");
	strncat(query,tab,64);
	strcat(query," SET ");
	strncat(query,col,64);
	strcat(query,"='");
	strcat(query,escape);
	strcat(query,"' WHERE uid='");
	sprintf(query + strlen(query), "%u", uid);
	strcat(query,"'");
   
//	eventlog(eventlog_level_trace, "db_set", "query: %s", query);

	if(sql->query(query)) {
	    char query2[512];
	    eventlog(eventlog_level_info, __FUNCTION__, "trying to insert new column %s", col);
	    strcpy(query2, "ALTER TABLE ");
	    strncat(query2,tab,DB_MAX_TAB);
	    strcat(query2," ADD COLUMN ");
	    strncat(query2,col,DB_MAX_TAB);
	    strcat(query2," VARCHAR(128);");

//	    eventlog(eventlog_level_trace, __FUNCTION__, "query: %s", query2);
	    if (sql->query(query2)) {
		eventlog(eventlog_level_error, __FUNCTION__, "ALTER failed, bailing out");
		continue;
	    }

	    /* try query again */
//	    eventlog(eventlog_level_trace, "db_set", "query: %s", query);
	    if (sql->query(query)) {
		// Tried everything, now trying to insert that user to the table for the first time
		sprintf(query2,"INSERT INTO %s (uid,%s) VALUES ('%u','%s')",tab,col,uid,escape);
//		eventlog(eventlog_level_error, __FUNCTION__, "update failed so tried INSERT for the last chance");
		if (sql->query(query2)) {
            	    eventlog(eventlog_level_error, __FUNCTION__, "could not INSERT attribute");
		    continue;
    		}
	    }
	}
    }

   return 0;
}

static int sql_read_accounts(t_read_accounts_func cb)
{
    char query[1024];
    t_sql_res * result = NULL;
    t_sql_row * row;
    t_storage_info *info;

    if(!sql) {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (cb == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "get NULL callback");
	return -1;
    }

    strcpy(query,"SELECT uid FROM BNET");
    if((result = sql->query_res(query)) != NULL && (sql->num_rows(result) > 1)) {
	while((row = sql->fetch_row(result)) != NULL) {
	    if (row[0] == NULL) {
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL uid from db");
		continue;
	    }

	    if (atoi(row[0]) == STORAGE_SQL_DEFAULT_UID) continue; /* skip default account */

	    if ((info = malloc(sizeof(t_sql_info))) == NULL) {
		eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for sql info");
		continue;
	    }

	    *((unsigned int *)info) = atoi(row[0]);
	    cb(info);
        }
	sql->free_result(result);
    } else {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	if (result) sql->free_result(result);
	return -1;
    }

    return 0;
}

static int sql_cmp_info(t_storage_info *info1, t_storage_info *info2)
{
    return *((unsigned int *)info1) == *((unsigned int *)info2);
}

static int sql_free_info(t_storage_info *info)
{
    if (info) free((void*)info);

    return 0;
}

static t_storage_info * sql_get_defacct(void)
{
    t_storage_info *info;

    if ((info = malloc(sizeof(t_sql_info))) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for sql info");
	return NULL;
    }

    *((unsigned int *)info) = STORAGE_SQL_DEFAULT_UID;

    return info;
}

static const char * sql_escape_key(const char *key)
{
    const char *newkey = key;

    if (strchr(key, '\\') || strchr(key, ' ') || strchr(key, '-')) {
	char *p;

	if ((newkey = strdup(key)) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for escaped key");
	    return key;
	}
	for(p = (char *)newkey; *p; p++)
	    if (*p == '\\' || *p == '-' || *p == ' ') *p = '_';
    }

    return newkey;
}

#ifdef KAKAMAKA

static unsigned int sql_max_uid()
{
   MYSQL_RES* result = NULL;
   MYSQL_ROW row;
   int uid;
   
   if(!mysql) {
      eventlog(eventlog_level_error, "db_max_uid", "NULL mysql");
      return 0;
   }
   
   mysql_query(mysql,"SELECT max_uid FROM counters;");
   result = mysql_store_result(mysql);
   if(result) {
      row = mysql_fetch_row(result);
      /* Dizzy: 16-07-2002 : check to see if its NULL in db */
      if (row == NULL || row[0] == NULL) {
	 eventlog(eventlog_level_error, "db_max_uid", "got NULL maxuid");
	 mysql_free_result(result);
	 return 0;
      }
      uid = atol(row[0]);
      mysql_free_result(result);
   }
   else return 0;
   
   eventlog(eventlog_level_trace, "db_max_uid", "uid: %d", uid);
   
   return uid;
}

int db_get_version()
{
  MYSQL_RES * result = NULL;
  MYSQL_ROW row;
  int version = 0;
	
  db_init();
  if (!mysql) return -1;
  mysql_query(mysql,"SELECT version FROM Version");
  result = mysql_store_result(mysql);
  if (!result) return 0;
  if (mysql_num_rows(result) != 1) { mysql_free_result(result); return 0; }
  if (!(row = mysql_fetch_row(result))) { mysql_free_result(result); return 0; }
  if (row[0]==NULL) { mysql_free_result(result); return 0; }
  version = atoi(row[0]);
  mysql_free_result(result);
  db_close();
  return version;
}

void db_set_version(int version)
{
  char query[1024];
       
  db_init();
  if (!mysql) return;
  
  sprintf(query,"UPDATE Version SET version=%d;",version);
  eventlog(eventlog_level_info,__FUNCTION__,"query:%s",query);
  mysql_query(mysql,query);
  if (mysql_affected_rows(mysql)!=1) 
    {
      mysql_query(mysql,"CREATE TABLE Version (version int(11) NOT NULL PRIMARY KEY);");
      sprintf(query,"INSERT INTO Version VALUES (%d);",version);
      mysql_query(mysql,query);
    }
  db_close();
}

extern int db_check()
{
  int version = 0;
   if (prefs_get_mysql_host() == NULL || prefs_get_mysql_dbname() == NULL ||
       prefs_get_mysql_account() == NULL || prefs_get_mysql_password() == NULL) {
      eventlog(eventlog_level_error, "db_check", "Invalid mysql settings! Please check your config!");
      return -1;
   }
   
   /* settings seem fine, lets try to connect */
   if ((mysql = mysql_init(NULL)) == NULL) {
      eventlog(eventlog_level_error, "db_check", "Could not init mysql");
      return -1;
   }
   
   if(!mysql_real_connect(mysql, prefs_get_mysql_host(), prefs_get_mysql_account(), prefs_get_mysql_password(), prefs_get_mysql_dbname(), 0, prefs_get_mysql_sock(), CLIENT_FOUND_ROWS)) 
     {
	eventlog(eventlog_level_info, "db_check", "mysql connection failed, lets try to build the db (%s)",mysql_error(mysql));
	db_close(); /* dbcreator opens its own connection */
	
	if (dbcreator_start() <0) {
	   eventlog(eventlog_level_error,"db_check","could not build db");
	   return -1;
	}

	return 0;
     }
   
   db_close();
   
   while ((version=db_get_version())!=CURRENT_DB_VERSION)
     {
       switch (version)
	 {
	 case 0:
	   {
             update_DB_v0_to_v150();
	     break;
	   }
	 default:
	   {
	     eventlog(eventlog_level_error,__FUNCTION__,"unknown PvPGN DB version, aborting");
	     return -1;
	     
	   }
	 }
     }
   
   return 0;
}

t_attr_from_all * db_get_attr_from_all(char const * attr)
{
   char query[1024];
   char *tab, *col;
   MYSQL_RES* result = NULL;
   MYSQL_ROW row;
   t_attr_from_all * attr_from_all;
   unsigned int * uids;
   char ** values;
   
   if(!mysql) {
      eventlog(eventlog_level_error, __FUNCTION__, "NULL mysql");
      return NULL;
   }

   if (_db_get_tab(attr, &tab, &col)<0) {
      eventlog(eventlog_level_error,__FUNCTION__,"error from db_get_tab");
      return NULL;
   }
   
   sprintf(query, "SELECT uid , %s FROM %s;",col,tab);
   
   eventlog(eventlog_level_trace, __FUNCTION__, "query: %s", query);
   mysql_query(mysql,query);

   result = mysql_store_result(mysql);

   if(result && (mysql_num_rows(result) > 1)) {
      int norows, count;
      unsigned int num;
      
      norows = mysql_num_rows(result) - 1;
      if ((attr_from_all = malloc(sizeof(t_attr_from_all))) == NULL) {
	 eventlog(eventlog_level_error, __FUNCTION__,"could not allocate for list");
	 mysql_free_result(result);
	 return NULL;
      }
      
      if ((uids = malloc(sizeof(unsigned int) * norows)) == NULL) {
	 eventlog(eventlog_level_error, __FUNCTION__,"could not allocate for uids");
	 free(attr_from_all);
	 mysql_free_result(result);
	 return NULL;
      }

      if ((values = malloc(sizeof(char *) * norows)) == NULL) {
	eventlog(eventlog_level_error,__FUNCTION__,"could not allocate for values");
	free(attr_from_all->uids);
	free(attr_from_all);
	mysql_free_result(result);
	return NULL;
      }

      for (count = 0; count < norows; count++)
      {
        values[count]=NULL;
      }

      attr_from_all->uids = uids;
      attr_from_all->values = values;
      attr_from_all->len    = norows;

      count = 0;
      for(count = 0; ((row = mysql_fetch_row(result))) && count < norows; count++) {
	 if (row[0] == NULL) {
	    db_free_attrs(attr_from_all);
	    mysql_free_result(result);
	    eventlog(eventlog_level_error, __FUNCTION__, "got NULL uid");
	    return NULL;
	 }
	 num = atol(row[0]);
	 
	 if (num == 0) {count--; continue;}
	 attr_from_all->uids[count] = num;
	 if (row[1]==NULL)
	   attr_from_all->values[count] = NULL;
	 else
	   attr_from_all->values[count] = strdup(row[1]);
      }

      mysql_free_result(result);
   } else {
      eventlog(eventlog_level_error,__FUNCTION__,"error query db (query:\"%s\")", query);
      if (result) mysql_free_result(result);
      return NULL;
   }
   
   if (!result) return NULL;
 
   return attr_from_all;   
}


extern char const * db_get_attribute(unsigned int sid, char const * key)
{
   char query[1024];
   char *tab, *col;
   MYSQL_RES* result = NULL;
   MYSQL_ROW row;
   char const * val;
   
   if(!mysql) {
      eventlog(eventlog_level_error, "db_get_attribute", "NULL mysql");
      return NULL;
   }

   if (_db_get_tab(key, &tab, &col)<0) {
      eventlog(eventlog_level_error,"db_get_attribute","error from db_get_tab");
      return NULL;
   }
   
   sprintf(query, "SELECT %s FROM %s WHERE uid = '%u';",col,tab,sid);
   
   eventlog(eventlog_level_trace, "db_get_attribute", "query: %s", query);
   mysql_query(mysql,query);

   result = mysql_store_result(mysql);
   
   if (!result) return NULL;
   if (mysql_num_rows(result) != 1) { mysql_free_result(result); return NULL; }
   
   if (!(row = mysql_fetch_row(result))) {
     eventlog(eventlog_level_error,"db_get_attribute","could not fetch row");
     mysql_free_result(result);
     return NULL;
   }
   
   if (row[0]==NULL)
   {
     mysql_free_result(result);
     return NULL;	
   }

   if ((val = (unescape_chars(row[0]))) == NULL) {
     eventlog(eventlog_level_error,"db_get_attribute","could not duplicate value");
     mysql_free_result(result);
     return NULL;
   }

   mysql_free_result(result);
   
   return val;
   


}

// aaron ---->
extern int db_drop_column(const char * key)
{
  char query[1024];
  char *tab, *col;
  
  if(!mysql) {
    eventlog(eventlog_level_error, "db_set", "NULL mysql");
    return -1;
  }

  if (_db_get_tab(key, &tab, &col)<0) {
    eventlog(eventlog_level_error,"db_set","error from db_get_tab");
    return -1;
  }
   
  strcpy(query, "ALTER TABLE ");
  strncat(query,tab,64);
  strcat(query," DROP ");
  strncat(query,col,64);
  strcat(query,"';");
  
  eventlog(eventlog_level_trace, "db_drop_column", "query: %s", query);
  mysql_query(mysql,query);
  
  if(mysql_affected_rows(mysql) != 1) 
    {
      eventlog(eventlog_level_fatal,"db_drop_column","Failed to drop column \"%s\" on table \"%s\".",col,tab);
      return -1;
    }  
  
  
  return 0;
}
// <---


extern int dbcreator_start()
{
   if(_dbcreator_init()<0)
     {
	eventlog(eventlog_level_error,"dbcreator_start","Error connection to MySQL - Check your conf File!");
	return -1;
     }
   
   eventlog(eventlog_level_info,"pvpgn_mysql","Creating Database %s",prefs_get_mysql_dbname());
   
   if(_dbcreator_createdatabase(prefs_get_mysql_dbname())<0) {
      eventlog(eventlog_level_error, "dbcreator_start","Error trying to create the database: %s", prefs_get_mysql_dbname());
      db_close();
      return -1;	
   }
   if(_dbcreator_filldb(prefs_get_mysql_dbname())<0) {
      eventlog(eventlog_level_error, "dbcreator_start", "Error trying to init db struct");
      db_close();
      return -1;
   }
   
   db_close();

   return 0;
   
}

static int _dbcreator_init()
{
   if (!((mysql=mysql_init(NULL)))) 
     return -1; 
   
   if(!mysql_real_connect(mysql, prefs_get_mysql_host(), prefs_get_mysql_account(), prefs_get_mysql_password(), "mysql", 0, prefs_get_mysql_sock(), CLIENT_FOUND_ROWS))
   {
     eventlog(eventlog_level_error,__FUNCTION__,"error: %s",mysql_error(mysql));
     return -1;
   }

   return 0;
}

static int _dbcreator_createdatabase(char const *database)
{
	char query[1024];
	
	strcpy(query,"CREATE DATABASE ");
	strncat(query,database, sizeof(query) - strlen(query) - 2);
	strcat(query,";");
	
	mysql_query(mysql,query);

	if(mysql_affected_rows(mysql) != 1) 
	{
		eventlog(eventlog_level_fatal,"dbcreator_createdatabase","Failed to Create the database.");
		return -1;
	}

   eventlog(eventlog_level_info,"dbcreator_createdatabase","Database Created...creating Tables..");
   
   return 0;
}

static int _dbcreator_filldb(char const *database)
{
   char **pstr;
   char * querys[] = {
      "CREATE TABLE BNET (uid int(11) NOT NULL default '0' PRIMARY KEY,acct_username varchar(128) default NULL,acct_userid varchar(128) default NULL,acct_passhash1 varchar(128) default NULL,flags_initial varchar(128) default NULL,auth_admin varchar(128) default 'false',auth_normallogin varchar(128) default NULL,auth_changepass varchar(128) default NULL,auth_changeprofile varchar(128) default NULL,auth_botlogin varchar(128) default 'true',auth_operator varchar(128) default NULL,new_at_team_flag varchar(128) default '0', auth_lockk varchar(128) default '0', auth_command_groups varchar(128) NOT NULL default '1');",
      "INSERT INTO BNET VALUES (0,NULL,NULL,NULL,NULL,'false',NULL,NULL,NULL,'true',NULL,'0','0','1');",
      "CREATE TABLE counters (max_uid int(11) NOT NULL default '0' PRIMARY KEY);",
      "INSERT INTO counters VALUES (0);",
      "CREATE TABLE friend (uid int(11) NOT NULL default '0' PRIMARY KEY);",
      "INSERT INTO friend VALUES (0);",
      "CREATE TABLE profile (uid int(11) NOT NULL default '0' PRIMARY KEY);",
      "INSERT INTO profile VALUES (0);",
      "CREATE TABLE Record (uid int(11) NOT NULL default '0' PRIMARY KEY, WAR3_solo_xp int(11) default '0', WAR3_solo_level int(11) default '0', WAR3_solo_wins int(11) default '0', WAR3_solo_rank int(11) default '0', WAR3_solo_losses int(11) default '0', WAR3_team_xp int(11) default '0', WAR3_team_level int(11) default '0', WAR3_team_rank int(11) default '0', WAR3_team_wins int(11) default '0', WAR3_team_losses int(11) default '0', WAR3_ffa_xp int(11) default '0', WAR3_ffa_rank int(11) default '0', WAR3_ffa_level int(11) default '0', WAR3_ffa_wins int(11) default '0', WAR3_ffa_losses int(11) default '0', WAR3_orcs_wins int(11) default '0', WAR3_orcs_losses int(11) default '0', WAR3_humans_wins int(11) default '0', WAR3_humans_losses int(11) default '0', WAR3_undead_wins int(11) default '0', WAR3_undead_losses int(11) default '0', WAR3_nightelves_wins int(11) default '0', WAR3_nightelves_losses int(11) default '0', WAR3_random_wins int(11) default '0', WAR3_random_losses int(11) default '0', WAR3_teamcount int(11) default '0', W3XP_solo_xp int(11) default '0', W3XP_solo_level int(11) default '0', W3XP_solo_wins int(11) default '0', W3XP_solo_rank int(11) default '0', W3XP_solo_losses int(11) default '0', W3XP_team_xp int(11) default '0', W3XP_team_level int(11) default '0', W3XP_team_rank int(11) default '0', W3XP_team_wins int(11) default '0', W3XP_team_losses int(11) default '0', W3XP_ffa_xp int(11) default '0', W3XP_ffa_rank int(11) default '0', W3XP_ffa_level int(11) default '0', W3XP_ffa_wins int(11) default '0', W3XP_ffa_losses int(11) default '0', W3XP_orcs_wins int(11) default '0', W3XP_orcs_losses int(11) default '0', W3XP_humans_wins int(11) default '0', W3XP_humans_losses int(11) default '0', W3XP_undead_wins int(11) default '0', W3XP_undead_losses int(11) default '0', W3XP_nightelves_wins int(11) default '0', W3XP_nightelves_losses int(11) default '0', W3XP_random_wins int(11) default '0', W3XP_random_losses int(11) default '0', W3XP_teamcount int(11) default '0');",
      "INSERT INTO Record VALUES (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);",
	  "CREATE TABLE Team (uid int(11) NOT NULL PRIMARY KEY);",
	  "INSERT INTO Team VALUES (0);",
	  "CREATE TABLE Version (version int(11) NOT NULL PRIMARY KEY);",
	  "INSERT INTO Version VALUES(150);",
      NULL
   };
	              
   if (mysql_select_db(mysql, database)) return -1;
   
   for(pstr = querys; *pstr; pstr++)
     if (mysql_query(mysql, *pstr)) return -1;
    
    return 0;
}

void update_DB_v0_to_v150()
{
  MYSQL_RES   * result;
  MYSQL_FIELD * field;
  char query[1024];
  
  db_init();

  if (!mysql) return;
  
  eventlog(eventlog_level_info,__FUNCTION__,"updating your PvPGN mySQL DB...");
  
  mysql_query(mysql,"SELECT * FROM Record;");
  if ((result = mysql_store_result(mysql)))
  {
    while ((field = mysql_fetch_field(result)))
    {
      if (field->name==NULL) continue;
      if (strncmp(field->name,"WAR3_",5)==0) continue;   // prevent converting over and over again
      if (strncmp(field->name,"W3XP_",5)==0) continue;
      if (strncmp(field->name,CLIENTTAG_STARCRAFT,4)==0) continue;
      if (strncmp(field->name,CLIENTTAG_BROODWARS,4)==0) continue;
      if (strncmp(field->name,CLIENTTAG_WARCIIBNE,4)==0) continue;
      if (strncmp(field->name,CLIENTTAG_DIABLO2DV,4)==0) continue;
      if (strncmp(field->name,CLIENTTAG_DIABLO2XP,4)==0) continue;
      if (strncmp(field->name,CLIENTTAG_DIABLORTL,4)==0) continue;
      if (strcmp(field->name,MYSQL_UID_FIELD)==0) continue;
      if (strcmp(field->name,"uid")==0) continue;

      sprintf(query,"ALTER TABLE Record CHANGE %s WAR3_%s int(11) default '0';",field->name,field->name);
      mysql_query(mysql,query);
      sprintf(query,"ALTER TABLE Record ADD W3XP_%s int(11) default '0';",field->name);
      mysql_query(mysql,query); 
    }
  mysql_free_result(result);
  }

  mysql_query(mysql,"SELECT * FROM Team;");
  if ((result = mysql_store_result(mysql)))
  {
    while ((field = mysql_fetch_field(result)))
    {
      if (field->name==NULL) continue;
      if (strncmp(field->name,"WAR3_",5)==0) continue;
      if (strncmp(field->name,"W3XP_",5)==0) continue;
      if (strcmp(field->name,MYSQL_UID_FIELD)==0) continue;
      if (strcmp(field->name,"uid")==0) continue;

      sprintf(query,"ALTER TABLE Team CHANGE %s WAR3_%s varchar(128);",field->name,field->name);
      mysql_query(mysql,query);
    }
    mysql_free_result(result);
  }

  db_close();
  db_set_version(150);

  eventlog(eventlog_level_info,__FUNCTION__,"successfully updated your DB");
}

#endif

#endif /* WITH_SQL */
