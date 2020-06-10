/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "tableschema.h"

#include "geodiffutils.hpp"


bool TableSchema::hasPrimaryKey() const
{
  for ( const TableColumnInfo &c : columns )
  {
    if ( c.isPrimaryKey )
      return true;
  }
  return false;
}

void tableSchemaPostgresToSqlite( TableSchema &tbl )
{
  // SQLite does not really care much about column types
  // but it does not like "geometry(X, Y)" type due to parentheses so let's convert
  // that to a simple type

  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];
    if ( col.type.rfind( "geometry(", 0 ) == 0 )
    {
      col.type = col.geomType;
    }
  }
}

void tableSchemaConvert( const std::string &driverSrcName, const std::string &driverDstName, TableSchema &tbl )
{
  if ( driverSrcName == driverDstName )
    return;

  if ( driverSrcName == "postgres" && driverDstName == "sqlite" )
    tableSchemaPostgresToSqlite( tbl );
  else
    throw GeoDiffException( "Unable to convert table schema from " + driverSrcName + " to " + driverDstName );
}
