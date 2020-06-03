/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "postgresdriver.h"

#include "geodiffutils.hpp"
#include "changeset.h"
#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "postgresutils.h"

#include <iostream>
#include <memory.h>


PostgresDriver::~PostgresDriver()
{
  if ( mConn )
    PQfinish( mConn );
  mConn = nullptr;
}


void PostgresDriver::open( const DriverParametersMap &conn )
{
  auto connInfo = conn.find( "conninfo" );
  if ( connInfo == conn.end() )
    throw GeoDiffException( "Missing 'conninfo' parameter" );
  std::string connInfoStr = connInfo->second;

  auto baseSchema = conn.find( "base" );
  if ( baseSchema == conn.end() )
    throw GeoDiffException( "Missing 'base' parameter" );
  mBaseSchema = baseSchema->second;

  auto modifiedSchema = conn.find( "modified" );
  mModifiedSchema = ( modifiedSchema == conn.end() ) ? std::string() : modifiedSchema->second;

  if ( mConn )
    throw GeoDiffException( "Connection already opened" );

  PGconn *c = PQconnectdb( connInfoStr.c_str() );

  if ( PQstatus( c ) != CONNECTION_OK )
  {
    throw GeoDiffException( "Cannot connect to PostgreSQL database: " + std::string( PQerrorMessage( c ) ) );
  }

  mConn = c;
}


std::vector<std::string> PostgresDriver::listTables( bool useModified )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );
  if ( useModified && mModifiedSchema.empty() )
    throw GeoDiffException( "Should use modified schema, but it was not set" );

  std::string schemaName = useModified ? mModifiedSchema : mBaseSchema;
  std::string sql = "select tablename from pg_tables where schemaname=" + quotedString( schemaName );
  PostgresResult res( execSql( mConn, sql ) );

  std::vector<std::string> tables;
  for ( int i = 0; i < res.rowCount(); ++i )
  {
    tables.push_back( res.value( i, 0 ) );
  }
  return tables;
}


TableSchema PostgresDriver::tableSchema( const std::string &tableName, bool useModified )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );
  if ( useModified && mModifiedSchema.empty() )
    throw GeoDiffException( "Should use modified schema, but it was not set" );

  std::string schemaName = useModified ? mModifiedSchema : mBaseSchema;

  // try to figure out details of the geometry columns (if any)
  std::string sqlGeomDetails = "SELECT f_geometry_column, type, srid FROM geometry_columns WHERE f_table_schema = " + quotedString( schemaName ) + " AND f_table_name = " + quotedString( tableName );
  std::map<std::string, std::string> geomTypes;
  std::map<std::string, int> geomSrids;
  PostgresResult resGeomDetails( execSql( mConn, sqlGeomDetails ) );
  for ( int i = 0; i < resGeomDetails.rowCount(); ++i )
  {
    std::string name = resGeomDetails.value( i, 0 );
    std::string type = resGeomDetails.value( i, 1 );
    std::string srid = resGeomDetails.value( i, 2 );
    int sridInt = srid.empty() ? -1 : atoi( srid.c_str() );
    geomTypes[name] = type;
    geomSrids[name] = sridInt;
  }

  std::string sqlColumns =
    "SELECT a.attname, pg_catalog.format_type(a.atttypid, a.atttypmod), i.indisprimary"
    " FROM pg_catalog.pg_attribute a"
    " LEFT JOIN pg_index i ON a.attrelid = i.indrelid AND a.attnum = ANY(i.indkey)"
    " WHERE"
    "   a.attnum > 0"
    "   AND NOT a.attisdropped"
    "   AND a.attrelid = ("
    "      SELECT c.oid"
    "        FROM pg_catalog.pg_class c"
    "        LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace"
    "        WHERE c.relname = " + quotedString( tableName ) +
    "          AND n.nspname = " + quotedString( schemaName ) +
    "   )"
    "  ORDER BY a.attnum";

  PostgresResult res( execSql( mConn, sqlColumns ) );

  TableSchema schema;
  for ( int i = 0; i < res.rowCount(); ++i )
  {
    TableColumnInfo col;
    col.name = res.value( i, 0 );
    col.type = res.value( i, 1 );
    col.isPrimaryKey = ( res.value( i, 2 ) == "t" );

    if ( col.type.rfind( "geometry", 0 ) == 0 )
    {
      col.isGeometry = true;
      if ( geomTypes.find( col.name ) != geomTypes.end() )
      {
        col.geomType = geomTypes[col.name];
        col.geomSrsId = geomSrids[col.name];
      }
    }

    schema.columns.push_back( col );
  }
  return schema;
}


static std::string allColumnNames( const TableSchema &tbl, const std::string &prefix = "" )
{
  std::string columns;
  for ( auto c : tbl.columns )
  {
    if ( !columns.empty() )
      columns += ", ";

    std::string name;
    if ( !prefix.empty() )
      name = prefix + ".";
    name += quotedIdentifier( c.name );

    if ( c.isGeometry )
      columns += "ST_AsBinary(" + name + ")";
    else
      columns += name;
  }
  return columns;
}


//! Constructs SQL query to get all rows that do not exist in the other table (used for insert and delete)
static std::string sqlFindInserted( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, bool reverse )
{
  std::string exprPk;
  for ( auto c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name ) + "=" +
                quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name );
    }
  }

  std::string sql = "SELECT " + allColumnNames( tbl ) + " FROM " +
                    quotedIdentifier( reverse ? schemaNameBase : schemaNameModified ) + "." + tableName +
                    " WHERE NOT EXISTS ( SELECT 1 FROM " +
                    quotedIdentifier( reverse ? schemaNameModified : schemaNameBase ) + "." + tableName +
                    " WHERE " + exprPk + ")";
  return sql;
}


static std::string sqlFindModified( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl )
{
  std::string exprPk;
  std::string exprOther;
  for ( auto c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += "b." + quotedIdentifier( c.name ) + "=" +
                "a." + quotedIdentifier( c.name );
    }
    else // not a primary key column
    {
      if ( !exprOther.empty() )
        exprOther += " OR ";

      std::string colBase = "b." + quotedIdentifier( c.name );
      std::string colModified = "a." + quotedIdentifier( c.name );
      exprOther += "NOT ((" + colBase + " IS NULL AND " + colModified + " IS NULL) OR (" + colBase + " = " + colModified + "))";
    }
  }

  std::string sql = "SELECT " + allColumnNames( tbl, "a" ) + ", " + allColumnNames( tbl, "b" ) + " FROM " +
                    quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + " a, " +
                    quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + " b" +
                    " WHERE " + exprPk + " AND (" + exprOther + ")";
  return sql;
}


static bool isColumnInt( const TableColumnInfo &col )
{
  return col.type == "integer";
}

static bool isColumnDouble( const TableColumnInfo &col )
{
  return col.type == "real" || col.type == "double precision";
}

static bool isColumnText( const TableColumnInfo &col )
{
  return col.type == "char" || col.type == "varchar" || col.type == "text" || col.type == "citext";
}

static bool isColumnGeometry( const TableColumnInfo &col )
{
  return col.type.rfind( "geometry", 0 ) == 0; // starts with "geometry" prefix
}


static Value resultToValue( const PostgresResult &res, int r, size_t i, const TableColumnInfo &col )
{
  Value v;
  if ( res.isNull( r, i ) )
  {
    v.setNull();
  }
  else
  {
    std::string valueStr = res.value( r, i );
    if ( col.type == "bool" )
    {
      v.setInt( valueStr == "t" );  // PostgreSQL uses 't' for true and 'f' for false
    }
    else if ( isColumnInt( col ) )
    {
      v.setInt( atol( valueStr.c_str() ) );
    }
    else if ( isColumnDouble( col ) )
    {
      v.setDouble( atof( valueStr.c_str() ) );
    }
    else if ( isColumnText( col ) )
    {
      v.setString( Value::TypeText, valueStr.c_str(), valueStr.size() );
    }
    else if ( isColumnGeometry( col ) )
    {
      // the value we get should have this format: "\x01234567890abcdef"
      if ( valueStr.size() < 2 )
        throw GeoDiffException( "Unexpected geometry value" );
      if ( valueStr[0] != '\\' || valueStr[1] != 'x' )
        throw GeoDiffException( "Unexpected prefix in geometry value" );

      // 1. convert from hex representation to proper binary stream
      std::string binString = hex2bin( valueStr.substr( 2 ) );
      // 2. convert WKB binary (or GeoPackage geom. encoding)
      std::string outBinString( 8 + binString.size(), 0 );
      memcpy( &outBinString[0], "GP\x00\x01", 4 );
      // write SRID in little endian
      outBinString[4] = 0xff & ( col.geomSrsId >> 0 );
      outBinString[5] = 0xff & ( col.geomSrsId >> 8 );
      outBinString[6] = 0xff & ( col.geomSrsId >> 16 );
      outBinString[7] = 0xff & ( col.geomSrsId >> 24 );

      memcpy( &outBinString[8], binString.data(), binString.size() );
      v.setString( Value::TypeBlob, outBinString.data(), outBinString.size() );
    }
    else
    {
      // TODO: handling of other types (date/time, list, blob, ...)
      throw GeoDiffException( "unknown value type: " + col.type );
    }
  }
  return v;
}


static std::string valueToSql( const Value &v, const TableColumnInfo &col )
{
  if ( v.type() == Value::TypeUndefined )
  {
    throw GeoDiffException( "this should not happen!" );
  }
  else if ( v.type() == Value::TypeNull )
  {
    return "NULL";
  }
  else if ( v.type() == Value::TypeInt )
  {
    return std::to_string( v.getInt() );
  }
  else if ( v.type() == Value::TypeDouble )
  {
    return std::to_string( v.getDouble() );
  }
  else if ( v.type() == Value::TypeText || v.type() == Value::TypeBlob )
  {
    if ( col.isGeometry )
    {
      // handling of geometries - they are encoded with GPKG header
      std::string gpkgWkb = v.getString();
      std::string wkb( gpkgWkb.size() - 8, 0 );
      memcpy( &wkb[0], &gpkgWkb[8], gpkgWkb.size() - 8 );
      return "ST_GeomFromWKB('\\x" + bin2hex( wkb ) + "', " + std::to_string( col.geomSrsId ) + ")";
    }
    return quotedString( v.getString() );
  }
  else
  {
    throw GeoDiffException( "unexpected value" );
  }
}


static void handleInserted( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, bool reverse, PGconn *conn, ChangesetWriter &writer, bool &first )
{
  std::string sqlInserted = sqlFindInserted( schemaNameBase, schemaNameModified, tableName, tbl, reverse );
  PostgresResult res( execSql( conn, sqlInserted ) );

  int rows = res.rowCount();
  for ( int r = 0; r < rows; ++r )
  {
    if ( first )
    {
      ChangesetTable chTable = schemaToChangesetTable( tableName, tbl );
      writer.beginTable( chTable );
      first = false;
    }

    ChangesetEntry e;
    e.op = reverse ? ChangesetEntry::OpDelete : ChangesetEntry::OpInsert;

    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Value v( resultToValue( res, r, i, tbl.columns[i] ) );
      if ( reverse )
        e.oldValues.push_back( v );
      else
        e.newValues.push_back( v );
    }

    writer.writeEntry( e );
  }
}


static void handleUpdated( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, PGconn *conn, ChangesetWriter &writer, bool &first )
{
  std::string sqlModified = sqlFindModified( schemaNameBase, schemaNameModified, tableName, tbl );
  PostgresResult res( execSql( conn, sqlModified ) );

  int rows = res.rowCount();
  for ( int r = 0; r < rows; ++r )
  {
    if ( first )
    {
      ChangesetTable chTable = schemaToChangesetTable( tableName, tbl );
      writer.beginTable( chTable );
      first = false;
    }

    /*
    ** Within the old.* record associated with an UPDATE change, all fields
    ** associated with table columns that are not PRIMARY KEY columns and are
    ** not modified by the UPDATE change are set to "undefined". Other fields
    ** are set to the values that made up the row before the UPDATE that the
    ** change records took place. Within the new.* record, fields associated
    ** with table columns modified by the UPDATE change contain the new
    ** values. Fields associated with table columns that are not modified
    ** are set to "undefined".
    */

    ChangesetEntry e;
    e.op = ChangesetEntry::OpUpdate;

    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Value v1( resultToValue( res, r, i + numColumns, tbl.columns[i] ) );
      Value v2( resultToValue( res, r, i, tbl.columns[i] ) );
      bool pkey = tbl.columns[i].isPrimaryKey;
      bool updated = v1 != v2;
      e.oldValues.push_back( ( pkey || updated ) ? v1 : Value() );
      e.newValues.push_back( updated ? v2 : Value() );
    }

    writer.writeEntry( e );
  }
}


void PostgresDriver::createChangeset( ChangesetWriter &writer )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  std::vector<std::string> tables = listTables();

  for ( auto tableName : tables )
  {
    TableSchema tbl = tableSchema( tableName );
    TableSchema tblNew = tableSchema( tableName, true );

    // test that table schema in the modified is the same
    if ( tbl != tblNew )
      throw GeoDiffException( "table schemas are not the same" );

    bool hasPkey = false;
    for ( auto c : tbl.columns )
    {
      if ( c.isPrimaryKey )
        hasPkey = true;
    }

    if ( !hasPkey )
      continue;  // ignore tables without primary key - they can't be compared properly

    bool first = true;

    handleInserted( mBaseSchema, mModifiedSchema, tableName, tbl, false, mConn, writer, first );  // INSERT
    handleInserted( mBaseSchema, mModifiedSchema, tableName, tbl, true, mConn, writer, first );   // DELETE
    handleUpdated( mBaseSchema, mModifiedSchema, tableName, tbl, mConn, writer, first );          // UPDATE
  }
}


static std::string sqlForInsert( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &values )
{
  /*
   * For a table defined like this: CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
   *
   * INSERT INTO x (a, b, c, d) VALUES (?, ?, ?, ?)
   */

  std::string sql;
  sql += "INSERT INTO " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " (";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += quotedIdentifier( tbl.columns[i].name );
  }
  sql += ") VALUES (";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += valueToSql( values[i], tbl.columns[i] );
  }
  sql += ")";
  return sql;
}


static std::string sqlForUpdate( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &oldValues, const std::vector<Value> &newValues )
{
  std::string sql;
  sql += "UPDATE " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " SET ";

  bool first = true;
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( newValues[i].type() != Value::TypeUndefined )
    {
      if ( !first )
        sql += ", ";
      first = false;
      sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( newValues[i], tbl.columns[i] );
    }
  }
  first = true;
  sql += " WHERE ";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( oldValues[i].type() != Value::TypeUndefined )
    {
      if ( !first )
        sql += " AND ";
      first = false;
      sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( oldValues[i], tbl.columns[i] );
    }
  }

  return sql;
}


static std::string sqlForDelete( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &values )
{
  std::string sql;
  sql += "DELETE FROM " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " WHERE ";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += " AND ";
    if ( tbl.columns[i].isPrimaryKey )
      sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( values[i], tbl.columns[i] );
    else
    {
      if ( values[i].type() == Value::TypeNull )
        sql += quotedIdentifier( tbl.columns[i].name ) + " IS NULL";
      else
        sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( values[i], tbl.columns[i] ) ;
    }
  }
  return sql;
}


void PostgresDriver::applyChangeset( ChangesetReader &reader )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  std::string lastTableName;
  TableSchema tbl;

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( entry.table->name != lastTableName )
    {
      std::string tableName = entry.table->name;
      lastTableName = tableName;
      tbl = tableSchema( tableName );

      if ( tbl.columns.size() == 0 )
        throw GeoDiffException( "No such table: " + tableName );

      if ( tbl.columns.size() != entry.table->primaryKeys.size() )
        throw GeoDiffException( "Wrong number of columns for table: " + tableName );

      for ( size_t i = 0; i < entry.table->primaryKeys.size(); ++i )
      {
        if ( tbl.columns[i].isPrimaryKey != entry.table->primaryKeys[i] )
          throw GeoDiffException( "Mismatch of primary keys in table: " + tableName );
      }
    }

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      std::string sql = sqlForInsert( mBaseSchema, entry.table->name, tbl, entry.newValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        throw GeoDiffException( "Failure doing INSERT: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        throw GeoDiffException( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() );
      }
    }
    else if ( entry.op == ChangesetEntry::OpUpdate )
    {
      std::string sql = sqlForUpdate( mBaseSchema, entry.table->name, tbl, entry.oldValues, entry.newValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        throw GeoDiffException( "Failure doing UPDATE: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        throw GeoDiffException( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() );
      }
    }
    else if ( entry.op == ChangesetEntry::OpDelete )
    {
      std::string sql = sqlForDelete( mBaseSchema, entry.table->name, tbl, entry.oldValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        throw GeoDiffException( "Failure doing DELETE: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        throw GeoDiffException( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() );
      }
    }
    else
      throw GeoDiffException( "Unexpected operation" );
  }
}
