/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "tableschema.h"
#include "driver.h"

#include "geodifflogger.hpp"

// ----- TableSchema ----

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


// ---- TableColumnType ----


std::string toBaseType( const std::string &type )
{
  std::string baseType( lowercaseString( type ) );

  if ( baseType == "int" || baseType == "integer" || baseType == "smallint" ||
       baseType == "mediumint" || baseType == "bigint" || baseType == "tinyint" )
  {
    return "integer";
  }
  else if ( baseType == "double" || baseType == "real" || baseType == "double precision" || baseType == "float" )
  {
    return "double precision";
  }
  else if ( baseType == "bool" || baseType == "boolean" )
  {
    return "boolean";
  }
  else if ( baseType == "text" || baseType.rfind( "text(" ) == 0 || baseType.rfind( "varchar(" ) == 0 )
  {
    return "text";
  }
  else if ( baseType == "blob" )
  {
    return "bytea";
  }
  else if ( baseType == "datetime" )
  {
    return "timestamp";
  }
  else if ( baseType == "date" )
  {
    return "date";
  }
  // Do we need to check for geometry too?

  return "";
}

void tableSchemaPostgresToSqlite( TableSchema & )
{
//  // SQLite does not really care much about column types
//  // but it does not like "geometry(X, Y)" type due to parentheses so let's convert
//  // that to a simple type. We also need to set date related columns to geopackage
//  // types DATE and DATETIME otherwise QGIS would assume these columns are text and
//  // set text widget for them (instead of calendar).

//  for ( size_t i = 0; i < tbl.columns.size(); ++i )
//  {
//    TableColumnInfo &col = tbl.columns[i];
//    if ( col.type.dbType.rfind( "geometry(", 0 ) == 0 )
//    {
//      col.type = col.geomType;
//    }
//    else if ( col.type == "timestamp without time zone" )
//    {
//      col.type = "DATETIME";
//    }
//    else if ( col.type == "date" )
//    {
//      col.type = "DATE";
//    }
//  }
}


void tableSchemaSqliteToPostgres( TableSchema & )
{
//  for ( size_t i = 0; i < tbl.columns.size(); ++i )
//  {
//    TableColumnInfo &col = tbl.columns[i];

//    // SQLite has very easy going approach to data types - it looks like anything is accepted
//    // as a data type name, and SQLite only does some basic string matching to figure out
//    // preferred data type for column (see "Column Affinity" in the docs - for example, "INT"
//    // substring in data type name implies integer affinity, "DOUB" substring implies real number
//    // affinity. (But columns can contain any data value type anyway.)

//    std::string baseType = toBaseType( col.type );

//    if ( !baseType.empty() )
//    {
//      col.type = baseType;
//    }
//    else if ( col.isGeometry )
//    {
//      std::string geomType = col.geomType;
//      if ( col.geomHasZ )
//        geomType += "Z";
//      if ( col.geomHasM )
//        geomType += "M";
//      col.type = "geometry(" + geomType + ", " + std::to_string( col.geomSrsId ) + ")";
//    }
//    else
//    {
//      Logger::instance().warn( "sqlite to postgres: table '" + tbl.name + "', column '" + col.name +
//                               "': unknown data type '" + col.type.name() + "' - using 'text' in postgres" );
//      col.type = "text";
//    }

//    // When 'serial' data type is used, PostgreSQL creates SEQUENCE object
//    // named <tablename>_<colname>_seq and will set column's default value
//    // to nextval('<tablename>_<colname>_seq')
//    if ( col.type == "integer" && col.isAutoIncrement )
//      col.type = "serial";
//  }
}

void TableColumnType::convertToBaseType()
{
  std::string type = lowercaseString( dbType );
  // TODO: set base type
}

void sqliteToBase( TableSchema &tbl )
{
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];
    std::string dbType ( lowercaseString( col.type.dbType ) );

    if ( dbType == "int" || dbType == "integer" || dbType == "smallint" ||
         dbType == "mediumint" || dbType == "bigint" || dbType == "tinyint" )
    {
      col.type.baseType = TableColumnType::INTEGER;
    }
    else if ( dbType == "double" || dbType == "real" || dbType == "double precision" || dbType == "float" )
    {
      col.type.baseType = TableColumnType::DOUBLE;
    }
    else if ( dbType == "bool" || dbType == "boolean" )
    {
      col.type.baseType = TableColumnType::BOOLEAN;
    }
    else if ( dbType == "text" || dbType.rfind( "text(" ) == 0 || dbType.rfind( "varchar(" ) == 0 )
    {
      col.type.baseType = TableColumnType::TEXT;
    }
    else if ( dbType == "blob" )
    {
      col.type.baseType = TableColumnType::BLOB;
    }
    else if ( dbType == "datetime" )
    {
      col.type.baseType = TableColumnType::DATETIME;
    }
    else if ( dbType == "date" )
    {
      col.type.baseType = TableColumnType::DATE;
    }
    else if ( col.isGeometry )
    {
      col.type.baseType = TableColumnType::GEOMETRY;
    }
    else
    {
      Logger::instance().warn( "sqlite to base type: table '" + tbl.name + "', column '" + col.name +
                               "': unknown data type '" + col.type.dbType + "' - using 'text'" );
      col.type.baseType = TableColumnType::TEXT;
    }
  }
}

void postgresToBase( TableSchema &tbl )
{
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    TableColumnInfo &col = tbl.columns[i];
    std::string dbType = lowercaseString( col.type.dbType );

    if ( dbType == "integer" )
    {
      col.type.baseType = TableColumnType::INTEGER;
    }
    else if ( dbType == "double precision" || dbType == "real" )
    {
      col.type.baseType = TableColumnType::DOUBLE;
    }
    else if ( dbType == "boolean" )
    {
      col.type.baseType = TableColumnType::BOOLEAN;
    }
    else if ( dbType == "text" )
    {
      col.type.baseType = TableColumnType::TEXT;
    }
    else if ( dbType == "bytea" )
    {
      col.type.baseType = TableColumnType::BLOB;
    }
    else if ( dbType == "timestamp without time zone" )
    {
      col.type.baseType = TableColumnType::DATETIME;
    }
    else if ( dbType == "date" )
    {
      col.type.baseType = TableColumnType::DATE;
    }
    else if ( dbType.rfind( "geometry(", 0 ) == 0 )
    {
      col.type.baseType = TableColumnType::GEOMETRY;
    }
    else
    {
      Logger::instance().warn( "postgres to base type: table '" + tbl.name + "', column '" + col.name +
                               "': unknown data type '" + col.type.dbType + "' - using 'text'" );
      col.type.baseType = TableColumnType::TEXT;
    }
  }
}

void baseToSqlite( TableSchema &tbl )
{
  // SQLite has very easy going approach to data types - it looks like anything is accepted
  // as a data type name, and SQLite only does some basic string matching to figure out
  // preferred data type for column (see "Column Affinity" in the docs - for example, "INT"
  // substring in data type name implies integer affinity, "DOUB" substring implies real number
  // affinity. (But columns can contain any data value type anyway.)

  // TYPES: http://www.geopackage.org/spec/#table_column_data_types
  static std::map<TableColumnType::BaseType, std::string> mapping = {
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
      col.type = col.geomType;
    }
    else
    {
      col.type = mapping.at( col.type.baseType );
    }
  }
}

void baseToPostgres( TableSchema &tbl )
{
  static std::map<TableColumnType::BaseType, std::string> mapping = {
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
      col.type = "geometry(" + geomType + ", " + std::to_string( col.geomSrsId ) + ")";
    }
    else
    {
      col.type = mapping.at( col.type.baseType );
    }

    // When 'serial' data type is used, PostgreSQL creates SEQUENCE object
    // named <tablename>_<colname>_seq and will set column's default value
    // to nextval('<tablename>_<colname>_seq')
    if ( col.type.baseType == TableColumnType::INTEGER && col.isAutoIncrement )
      col.type = "serial";
  }
}


void tableSchemaConvert( const std::string &driverSrcName, const std::string &driverDstName, TableSchema &tbl )
{
  if ( driverSrcName == Driver::SQLITEDRIVERNAME )
    sqliteToBase( tbl );
  else if ( driverSrcName == Driver::POSTGRESDRIVERNAME )
    postgresToBase( tbl );
  else
    throw GeoDiffException( "Uknown driver name " + driverSrcName );

  if ( driverDstName == Driver::SQLITEDRIVERNAME )
    baseToSqlite( tbl );
  else if ( driverDstName == Driver::POSTGRESDRIVERNAME )
    baseToPostgres( tbl );
  else
    throw GeoDiffException( "Uknown driver name " + driverDstName );

//  if ( driverSrcName == driverDstName )
//    return;

//  if ( driverSrcName == "postgres" && driverDstName == "sqlite" )
//    tableSchemaPostgresToSqlite( tbl );
//  else if ( driverSrcName == "sqlite" && driverDstName == "postgres" )
//    tableSchemaSqliteToPostgres( tbl );
}

std::string TableColumnType::name()
{
  static std::map<TableColumnType::BaseType, std::string> mapping {
    { TableColumnType::TEXT,     "text"     },
    { TableColumnType::INTEGER,  "integer"  },
    { TableColumnType::DOUBLE,   "double"   },
    { TableColumnType::BOOLEAN,  "boolean"  },
    { TableColumnType::BLOB,     "blob"     },
    { TableColumnType::GEOMETRY, "geometry" },
    { TableColumnType::DATE,     "date"     },
    { TableColumnType::DATETIME, "datetime" },
  };

  auto it = mapping.find( baseType );
  if ( it != mapping.end() )
    return mapping.at( baseType );

  return dbType;
}
