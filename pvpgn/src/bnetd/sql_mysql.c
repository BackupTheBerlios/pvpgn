#ifdef WITH_SQL_MYSQL

#include "common/setup_before.h"
#ifdef WIN32
#include "mysql/config-win.h"
#include "mysql/mysql.h"
#else
#include <mysql.h>
#endif
#include <stdlib.h>
#include "common/eventlog.h"
#include "storage_sql.h"
#include "sql_mysql.h"
#include "common/setup_after.h"

static int sql_mysql_init(const char *, const char *, const char *, const char *, const char *, const char *);
static int sql_mysql_close(void);
static t_sql_res * sql_mysql_query_res(const char *);
static int sql_mysql_query(const char *);
static t_sql_row * sql_mysql_fetch_row(t_sql_res *);
static void sql_mysql_free_result(t_sql_res *);
static unsigned int sql_mysql_num_rows(t_sql_res *);
static unsigned int sql_mysql_num_fields(t_sql_res *);
static t_sql_field * sql_mysql_fetch_fields(t_sql_res *);
static int sql_mysql_free_fields(t_sql_field *);
static void sql_mysql_escape_string(char *, const char *, int);

t_sql_engine sql_mysql = {
    sql_mysql_init,
    sql_mysql_close,
    sql_mysql_query_res,
    sql_mysql_query,
    sql_mysql_fetch_row,
    sql_mysql_free_result,
    sql_mysql_num_rows,
    sql_mysql_num_fields,
    sql_mysql_fetch_fields,
    sql_mysql_free_fields,
    sql_mysql_escape_string
};

static MYSQL *mysql = NULL;

static int sql_mysql_init(const char *host, const char *port, const char *socket, const char *name, const char *user, const char *pass)
{
    if (name == NULL || user == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL parameter");
        return -1;
    }

    if ((mysql = mysql_init(NULL)) == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "got error from mysql_init");
        return -1;
    }

    if ((mysql = mysql_real_connect(mysql, host, user, pass, name, port ? atoi(port) : 0, socket, 0)) == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "error connecting to database");
        return -1;
    }

    return 0;
}

static int sql_mysql_close(void)
{
    if (mysql) {
	mysql_close(mysql);
	mysql = NULL;
    }

    return 0;
}

static t_sql_res * sql_mysql_query_res(const char * query)
{
    t_sql_res *res;

    if (mysql == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "mysql driver not initilized");
        return NULL;
    }

    if (query == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL query");
        return NULL;
    }

    if (mysql_query(mysql, query)) {
//        eventlog(eventlog_level_debug, __FUNCTION__, "got error from query (%s)", query);
	return NULL;
    }

    res = mysql_store_result(mysql);
    if (res == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "got error from store result");
        return NULL;
    }

    return res;
}

static int sql_mysql_query(const char * query)
{
    if (mysql == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "mysql driver not initilized");
        return -1;
    }

    if (query == NULL) {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL query");
        return -1;
    }

    return mysql_query(mysql, query) == 0 ? 0 : -1;
}

static t_sql_row * sql_mysql_fetch_row(t_sql_res *result)
{
    if (result == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL result");
	return NULL;
    }

    return mysql_fetch_row((MYSQL_RES *)result);
}

static void sql_mysql_free_result(t_sql_res *result)
{
    if (result == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL result");
	return;
    }

    mysql_free_result((MYSQL_RES *)result);
}

static unsigned int sql_mysql_num_rows(t_sql_res *result)
{
    if (result == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL result");
	return 0;
    }

    return mysql_num_rows((MYSQL_RES *)result);
}

static unsigned int sql_mysql_num_fields(t_sql_res *result)
{
    if (result == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL result");
	return 0;
    }

    return mysql_num_fields((MYSQL_RES *)result);
}

static t_sql_field * sql_mysql_fetch_fields(t_sql_res *result)
{
    MYSQL_FIELD *fields;
    unsigned fieldno, i;
    t_sql_field *rfields;

    if (result == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL result");
	return NULL;
    }

    fieldno = mysql_num_fields((MYSQL_RES *)result);
    fields = mysql_fetch_fields((MYSQL_RES *)result);

    if ((rfields = malloc(sizeof(t_sql_field) * (fieldno + 1))) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "not enough memory for field list");
	return NULL;
    }

    for(i = 0; i < fieldno; i++)
	rfields[i] = fields[i].name;
    rfields[i] = NULL;

    return rfields;
}

static int sql_mysql_free_fields(t_sql_field *fields)
{
    if (fields == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL fields");
	return -1;
    }

    free((void*)fields);
    return 0; /* mysql_free_result() should free the rest properly */
}

static void sql_mysql_escape_string(char *escape, const char *from, int len)
{
    if (mysql == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "mysql driver not initilized");
	return;
    }
    mysql_real_escape_string(mysql, escape, from, len);
}

#endif /* WITH_SQL_MYSQL */
