#ifndef INCLUDED_SQL_DBCREATOR_TYPES
#define INCLUDED_SQL_DBCRETAOR_TYPES

#include "common/list.h"

typedef struct column
#ifdef SQL_DBCREATOR_INTERNAL_ACCESS
{
  char * name;
  char * value;
} 
#endif
t_column;

typedef struct table
#ifdef SQL_DBCREATOR_INTERNAL_ACCESS
{
  char   * name;
  t_list * columns;
} 
#endif
t_table;

typedef struct db_layout
#ifdef SQL_DBCREATOR_INTERNAL_ACCESS
{
  t_list * tables;
} 
#endif
t_db_layout;


#endif /* INCLUDED_SQL_DBCREATOR_TYPES */

#ifndef JUST_NEED_TYPES

#ifndef INCLUDED_SQL_DBCREATOR_PROTOS
#define INCLUDED_SQL_DBCRETAOR_PROTOS

#include "storage_sql.h"

int sql_dbcreator(t_sql_engine * sql);

#endif /* INCLUDED_SQL_DBCREATOR_PROTOS */
#endif /* JUST_NEED_TYPES */
