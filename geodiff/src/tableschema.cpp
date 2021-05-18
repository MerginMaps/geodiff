/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "tableschema.h"
#include "driver.h"

#include "geodifflogger.hpp"


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

TableColumnType sqliteToBaseColumn( const std::string &columnType, bool isGeometry )
{
  TableColumnType type;
  type.dbType = columnType;

  if ( isGeometry )
  {
    type.baseType = TableColumnType::GEOMETRY;
    return type;
  }

  std::string dbType( lowercaseString( columnType ) );

  if ( dbType == "int" || dbType == "integer" || dbType == "smallint" ||
       dbType == "mediumint" || dbType == "bigint" || dbType == "tinyint" )
  {
    type.baseType = TableColumnType::INTEGER;
  }
  else if ( dbType == "double" || dbType == "real" || dbType == "double precision" || dbType == "float" )
  {
    type.baseType = TableColumnType::DOUBLE;
  }
  else if ( dbType == "bool" || dbType == "boolean" )
  {
    type.baseType = TableColumnType::BOOLEAN;
  }
  else if ( dbType == "text" || dbType.rfind( "text(" ) == 0 || dbType.rfind( "varchar(" ) == 0 )
  {
    type.baseType = TableColumnType::TEXT;
  }
  else if ( dbType == "blob" )
  {
    type.baseType = TableColumnType::BLOB;
  }
  else if ( dbType == "datetime" )
  {
    type.baseType = TableColumnType::DATETIME;
  }
  else if ( dbType == "date" )
  {
    type.baseType = TableColumnType::DATE;
  }
  else
  {
    Logger::instance().info( "Converting GeoPackage type " + columnType + " to base type unsuccessful, using text." );
    type.baseType = TableColumnType::TEXT;
  }

  return type;
}

TableColumnType postgresToBaseColumn( const std::string &columnType, bool isGeometry )
{
  TableColumnType type;
  type.dbType = columnType;

  if ( isGeometry )
  {
    type.baseType = TableColumnType::GEOMETRY;
    return type;
  }

  std::string dbType = lowercaseString( columnType );

  if ( dbType == "integer" || dbType == "smallint" || dbType == "bigint" )
  {
    type.baseType = TableColumnType::INTEGER;
  }
  else if ( dbType == "double precision" || dbType == "real" )
  {
    type.baseType = TableColumnType::DOUBLE;
  }
  else if ( dbType == "boolean" )
  {
    type.baseType = TableColumnType::BOOLEAN;
  }
  else if ( dbType == "text" || startsWith( dbType, "text(" ) ||
            dbType == "varchar" || startsWith( dbType, "varchar(" ) ||
            dbType == "character varying" || startsWith( dbType, "character varying(" ) ||
            dbType == "char" || dbType == "citetext" )
  {
    type.baseType = TableColumnType::TEXT;
  }
  else if ( dbType == "bytea" )
  {
    type.baseType = TableColumnType::BLOB;
  }
  else if ( dbType == "timestamp without time zone" )
  {
    type.baseType = TableColumnType::DATETIME;
  }
  else if ( dbType == "date" )
  {
    type.baseType = TableColumnType::DATE;
  }
  else
  {
    Logger::instance().warn( "Converting PostgreSQL type " + columnType + " to base type unsuccessful, using text." );
    type.baseType = TableColumnType::TEXT;
  }

  return type;
}

void baseToSqlite( TableSchema &tbl )
{
  // SQLite has very easy going approach to data types - it looks like anything is accepted
  // as a data type name, and SQLite only does some basic string matching to figure out
  // preferred data type for column (see "Column Affinity" in the docs - for example, "INT"
  // substring in data type name implies integer affinity, "DOUB" substring implies real number
  // affinity. (But columns can contain any data value type anyway.)

  // TYPES: http://www.geopackage.org/spec/#table_column_data_types
  static std::map<TableColumnType::BaseType, std::string> mapping =
  {
    { TableColumnType::INTEGER,  "INTEGER"   },
    { TableColumnType::DOUBLE,   "DOUBLE"    },
    { TableColumnType::BOOLEAN,  "BOOLEAN"   },
    { TableColumnType::TEXT,     "TEXT"      },
    { TableColumnType::BLOB,     "BLOB"      },
    { TableColumnType::GEOMETRY, ""          },
    { TableColumnType::DATETIME, "DATETIME"  },
    { TableColumnType::DATE,     "DATE"      },
  };

  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];

    if ( col.type.baseType == TableColumnType::GEOMETRY )
    {
      col.type.dbType = col.geomType;
    }
    else
    {
      col.type.dbType = mapping.at( col.type.baseType );
    }
  }
}

void baseToPostgres( TableSchema &tbl )
{
  static std::map<TableColumnType::BaseType, std::string> mapping =
  {
    { TableColumnType::INTEGER,  "integer"            },
    { TableColumnType::DOUBLE,   "double precision"   },
    { TableColumnType::BOOLEAN,  "boolean"            },
    { TableColumnType::TEXT,     "text"               },
    { TableColumnType::BLOB,     "bytea"              },
    { TableColumnType::GEOMETRY, ""                   },
    { TableColumnType::DATETIME, "timestamp"          },
    { TableColumnType::DATE,     "date"               },
  };

  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];

    if ( col.type.baseType == TableColumnType::GEOMETRY )
    {
      std::string geomType = col.geomType;
      if ( col.geomHasZ )
        geomType += "Z";
      if ( col.geomHasM )
        geomType += "M";
      col.type.dbType = "geometry(" + geomType + ", " + std::to_string( col.geomSrsId ) + ")";
    }
    else
    {
      col.type.dbType = mapping.at( col.type.baseType );
    }

    // When 'serial' data type is used, PostgreSQL creates SEQUENCE object
    // named <tablename>_<colname>_seq and will set column's default value
    // to nextval('<tablename>_<colname>_seq')
    if ( col.type.baseType == TableColumnType::INTEGER && col.isAutoIncrement )
      col.type.dbType = "serial";
  }
}

void tableSchemaConvert( const std::string &driverDstName, TableSchema &tbl )
{
  if ( driverDstName == Driver::SQLITEDRIVERNAME )
    baseToSqlite( tbl );
  else if ( driverDstName == Driver::POSTGRESDRIVERNAME )
    baseToPostgres( tbl );
  else
    throw GeoDiffException( "Uknown driver name " + driverDstName );
}

TableColumnType columnType( const std::string &columnType, const std::string &driverName, bool isGeometry )
{
  if ( driverName == Driver::SQLITEDRIVERNAME )
    return sqliteToBaseColumn( columnType, isGeometry );
  else if ( driverName == Driver::POSTGRESDRIVERNAME )
    return postgresToBaseColumn( columnType, isGeometry );
  else
    throw GeoDiffException( "Uknown driver name " + driverName );
}

std::string TableColumnType::baseTypeToString( TableColumnType::BaseType t )
{
  switch ( t )
  {
    case TEXT:     return "text";
    case INTEGER:  return "integer";
    case DOUBLE:   return "double";
    case BOOLEAN:  return "boolean";
    case BLOB:     return "blob";
    case GEOMETRY: return "geometry";
    case DATE:     return "date";
    case DATETIME: return "datetime";
  }
  return "?";
}
