/*
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

#define  SQL_DBCREATOR_INTERNAL_ACCESS
#include "sql_dbcreator.h"
#undef   SQL_DBCREATOR_INTERNAL_ACCESS
#include "storage_sql.h"

#include "common/eventlog.h"
#include "common/util.h"

#include "compat/strdup.h"
#include "common/list.h"
#include "prefs.h"
#include "common/setup_after.h"

t_elem  * curr_table  = NULL;
t_elem  * curr_column = NULL;

t_db_layout * db_layout;

t_column * create_column(char * name, char * value)
{
  t_column * column;
  
  if (!(name))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL column name");
      return NULL;
    }
  
  if (!(value))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL column value");
      return NULL;
    }
  
  if (!(column = malloc(sizeof(t_column))))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not alloc mem for column");
      return NULL;
    }
  
  if (!(column->name  = strdup(name)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not strdup column name");
      free((void *)column);
      return NULL;
    }
  
  if (!(column->value = strdup(value)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not strdup column value");
      free((void *)column->name);
      free((void *)column);
      return NULL;
    }
  
  return column;
};

void dispose_column(t_column * column)
{
  if (column)
    {
      if (column->name)  free((void *)column->name);
      if (column->value) free((void *)column->value);
      free((void *)column);
    }
}

t_table * create_table(char * name)
{
  t_table * table;
  
  if (!(name))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL table name");
      return NULL;
    }
  
  if (!(table = malloc(sizeof(t_table))))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not alloc mem for table");
      return NULL;
    }
  
  if (!(table->name = strdup(name)))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not strdup table name");
      free((void *)table);
    }
  
  if (!(table->columns = list_create()))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not create list");
      free((void *)table->name);
      free((void *)table);
    }
  
  return table;
}

void dispose_table(t_table * table)
{
  t_elem * curr;
  t_column * column;
  
  if (table)
    {
      if (table->name) free((void *)table->name);
      // free list
      if (table->columns)
        {
          LIST_TRAVERSE(table->columns,curr)
	    {
	      if (!(column = elem_get_data(curr)))
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		  continue;
		}
	      dispose_column(column);
	      list_remove_elem(table->columns,curr);
	    }
	  
	  list_destroy(table->columns);
	  
        }
      
      free((void *)table);
    }
}

void table_add_column(t_table * table, t_column * column)
{
  if ((table) && (column))
    {
      if (list_append_data(table->columns,column)<0)
        {
          eventlog(eventlog_level_error,__FUNCTION__,"failed to add column");
          dispose_column(column);
        }
    }
}

t_db_layout * create_db_layout()
{
  t_db_layout * db_layout;
  
  if (!(db_layout = malloc(sizeof(t_db_layout))))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not alloc mem for db_layout");
      return NULL;
    }
  
  if (!(db_layout->tables = list_create()))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"could not create list");
      free((void *)db_layout);
      return NULL;
    }
  
  return db_layout;
}

void dispose_db_layout(t_db_layout * db_layout)
{
  t_elem  * curr;
  t_table * table;
 

  if (db_layout)
    {
      if (db_layout->tables)
        {
          LIST_TRAVERSE(db_layout->tables,curr)
	    {
	      if (!(table = elem_get_data(curr)))
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		  continue;
		}
	      dispose_table(table);
	      list_remove_elem(db_layout->tables,curr);
	    }
          list_destroy(db_layout->tables);
        }
      free((void *)db_layout);
    }
  
}

void db_layout_add_table(t_db_layout * db_layout, t_table * table)
{
  if ((db_layout) && (table))
    {
      if (list_append_data(db_layout->tables,table)<0)
        {
          eventlog(eventlog_level_error,__FUNCTION__,"failed to add table");
          dispose_table(table);
        }
    }
}

t_table * db_layout_get_first_table(t_db_layout * db_layout)
{
  t_table * table;

  curr_column = NULL;

  if (!(db_layout))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL db_layout");
    return NULL;
  }

  if (!(db_layout->tables))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"found NULL db_layout->tables");
    return NULL;
  }

  if (!(curr_table = list_get_first(db_layout->tables)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"db_layout has no tables");
    return NULL;
  }

  if (!(table = elem_get_data(curr_table)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return table;
}

t_table * db_layout_get_next_table()
{
  t_table * table;

  curr_column = NULL;

  if (!(curr_table))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL curr_table");
    return NULL;
  }

  if (!(curr_table = elem_get_next(curr_table))) return NULL;

  if (!(table = elem_get_data(curr_table)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return table;
}

t_column * table_get_first_column(t_table * table)
{
  t_column * column;

  if (!(table))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table");
    return NULL;
  }

  if (!(table->columns))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table->columns");
    return NULL;
  }

  if (!(curr_column = list_get_first(table->columns)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"table has no columns");
    return NULL;
  }

  if (!(column = elem_get_data(curr_column)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return column;
}

t_column * table_get_next_column()
{
  t_column * column;

  if (!(curr_column))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL curr_column");
    return NULL;
  }

  if (!(curr_column = elem_get_next(curr_column))) return NULL;

  if (!(column = elem_get_data(curr_column)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return column;
}

int load_db_layout(char const * filename)
{
  FILE * fp;
  int    lineno;
  char * line        = NULL;
  char * tmp         = NULL;
  char * table       = NULL;
  char * column      = NULL;
  char * value       = NULL;
  t_table * _table   = NULL;
  t_column * _column = NULL;

  if (!(filename))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
    return -1;
  }

  if (!(fp = fopen(filename,"r")))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"can't open sql_DB_layout file");
    return -1;
  }

  if (!(db_layout = create_db_layout()))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"failed to create db_layout");
    fclose(fp);
    return -1;
  }

  for (lineno=1; (line = file_get_line(fp)) ; lineno++)
  {
    switch (line[0])
    {
      case '[':
        table = &line[1];
        if (!(tmp = strchr(table,']')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing ']' in line %i",lineno);
          free((void *)line);
          continue;
        }
        tmp[0]='\0';
        if (_table)  db_layout_add_table(db_layout,_table);

        _table = create_table(table);

        break;
      case '"':
        if (!(_table))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"found a column without previous table in line %i",lineno);
          free((void *)line);
          continue;
        }
        column = &line[1];
        if (!(tmp = strchr(column,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing '\"' at the end of column definitioni in line %i",lineno);
          free((void *)line);
          continue;
        }
        tmp[0]='\0';
        tmp++;
        if (!(tmp = strchr(tmp,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing default value in line %i",lineno);
          free((void *)line);
          continue;
        }
        value = ++tmp;
        if (!(tmp = strchr(value,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing '\"' at the end of default value in line %i",lineno);
          free((void *)line);
          continue;
        }
        tmp[0]='\0';
        _column = create_column(column,value);
        table_add_column(_table,_column);
        _column = NULL;

        break;
      case '#':
        break;
      default:
        eventlog(eventlog_level_error,__FUNCTION__,"illegal starting symbol at line %i",lineno);
    }
    free((void *)line);
  }
  if (_table) db_layout_add_table(db_layout,_table);

  fclose(fp);
  return 0;
}

int sql_dbcreator(t_sql_engine * sql)
{
  t_table     * table;
  t_column    * column;
  char        _column[1024];
  char        query[1024];

  load_db_layout(prefs_get_DBlayoutfile());

  eventlog(eventlog_level_info, __FUNCTION__,"Creating missing tables and columns (if any)");
 
  for (table = db_layout_get_first_table(db_layout);table;table = db_layout_get_next_table())
  {
     column = table_get_first_column(table);
     sprintf(query,"CREATE TABLE %s (%s default %s)",table->name,column->name,column->value); 
     //create table if missing
     if (!(sql->query(query)))
     {
       eventlog(eventlog_level_info,__FUNCTION__,"added missing table %s to DB",table->name);
       eventlog(eventlog_level_info,__FUNCTION__,"added missing column %s to table %s",column->name,table->name);
     }
     sscanf(column->name,"%s",_column); //get column name without format infos
     sprintf(query,"INSERT INTO %s (%s) VALUES (%s)",table->name,_column,column->value);
     if (!(sql->query(query)))
     {
       eventlog(eventlog_level_info,__FUNCTION__,"added missing default account to table %s",table->name);
     }

    for (;column;column = table_get_next_column())
    {
      sprintf(query,"ALTER TABLE %s ADD %s",table->name,column->name);
      // add missing columns
      if (!(sql->query(query)))
      {
        eventlog(eventlog_level_info,__FUNCTION__,"added missing column %s to table %s",column->name,table->name);
	sscanf(column->name,"%s",_column); //get column name without format infos
	sprintf(query,"ALTER TABLE %s ALTER %s SET DEFAULT %s",table->name,_column,column->value);
	sql->query(query);
      }
    }
  }
  
  dispose_db_layout(db_layout);
 
  eventlog(eventlog_level_info,__FUNCTION__,"finished adding missing tables and columns");  
  return 0;
}

#endif /* WITH_SQL */
