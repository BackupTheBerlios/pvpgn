#ifndef INCLUDED_SQL_MYSQL_TYPES
#define INCLUDED_SQL_MYSQL_TYPES

#include <mysql.h>

typedef MYSQL_RES t_mysql_res;
typedef MYSQL_ROW t_mysql_row;

#endif /* INCLUDED_SQL_MYSQL_TYPES */

#ifndef INCLUDED_SQL_MYSQL_PROTOS
#define INCLUDED_SQL_MYSQL_PROTOS

#include "storage.h"

extern t_sql_engine sql_mysql;

#endif /* INCLUDED_SQL_MYSQL_PROTOS */
