/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "tableschema.h"

#include "geodifflogger.hpp"
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

size_t TableSchema::columnFromName( const std::string &name )
{
  for ( size_t i = 0; i < columns.size(); ++i )
  {
    if ( name == columns[i].name )
      return i;
  }
  return SIZE_MAX;
}

size_t TableSchema::geometryColumn() const
{
  for ( size_t i = 0; i < columns.size(); ++i )
  {
    if ( columns[i].isGeometry )
      return i;
  }
  return SIZE_MAX;
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


void tableSchemaSqliteToPostgres( TableSchema &tbl )
{
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];

    // transform to lowercase first
    col.type = lowercaseString( col.type );

    // SQLite has very easy going approach to data types - it looks like anything is accepted
    // as a data type name, and SQLite only does some basic string matching to figure out
    // preferred data type for column (see "Column Affinity" in the docs - for example, "INT"
    // substring in data type name implies integer affinity, "DOUB" substring implies real number
    // affinity. (But columns can contain any data value type anyway.)

    if ( col.type == "int" || col.type == "integer" || col.type == "smallint" || col.type == "mediumint" || col.type == "bigint" )
    {
      col.type = "integer";
    }
    else if ( col.type == "double" || col.type == "real" || col.type == "double precision" || col.type == "float" )
    {
      col.type = "double precision";
    }
    else if ( col.type == "bool" || col.type == "boolean" )
    {
      col.type = "boolean";
    }
    else if ( col.type.rfind( "text(" ) == 0 || col.type.rfind( "varchar(" ) == 0 )
    {
      col.type = "text";
    }
    else if ( col.type == "blob" )
    {
      col.type = "bytea";
    }
    else if ( col.type == "datetime" )
    {
      col.type = "timestamp";
    }
    else if ( col.isGeometry )
    {
      std::string geomType = col.geomType;
      if ( col.geomHasZ )
        geomType += "Z";
      if ( col.geomHasM )
        geomType += "M";
      col.type = "geometry(" + geomType + ", " + std::to_string( col.geomSrsId ) + ")";
    }
    else
    {
      Logger::instance().warn( "sqlite to postgres: table '" + tbl.name + "', column '" + col.name +
                               "': unknown data type '" + col.type + "' - using 'text' in postgres" );
      col.type = "text";
    }

    // When 'serial' data type is used, PostgreSQL creates SEQUENCE object
    // named <tablename>_<colname>_seq and will set column's default value
    // to nextval('<tablename>_<colname>_seq')
    if ( col.type == "integer" && col.isAutoIncrement )
      col.type = "serial";
  }
}


void tableSchemaConvert( const std::string &driverSrcName, const std::string &driverDstName, TableSchema &tbl )
{
  if ( driverSrcName == driverDstName )
    return;

  if ( driverSrcName == "postgres" && driverDstName == "sqlite" )
    tableSchemaPostgresToSqlite( tbl );
  else if ( driverSrcName == "sqlite" && driverDstName == "postgres" )
    tableSchemaSqliteToPostgres( tbl );
  else
    throw GeoDiffException( "Unable to convert table schema from " + driverSrcName + " to " + driverDstName );
}
