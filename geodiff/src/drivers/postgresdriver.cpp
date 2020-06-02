/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "postgresdriver.h"

#include "geodiffutils.hpp"
#include "changeset.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "postgresutils.h"

#include <iostream>


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

  PostgresResult r( execSql( c, "SELECT 1+1" ) );
  std::cout << "num tuples" << r.rowCount() << std::endl;
  std::cout << "res " << r.value( 0, 0 ) << std::endl;

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
    schema.columns.push_back( col );
  }
  return schema;
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

  std::string sql = "SELECT * FROM " +
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
      exprPk += quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name ) + "=" +
                quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name );
    }
    else // not a primary key column
    {
      if ( !exprOther.empty() )
        exprOther += " OR ";

      // TODO: rewrite as not ( (a=b) or (a is null and b is null) )

      std::string colBase = quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name );
      std::string colModified = quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name );
      exprOther += "NOT ((" + colBase + " IS NULL AND " + colModified + " IS NULL) OR (" + colBase + " = " + colModified + "))";
    }
  }

  std::string sql = "SELECT * FROM " +
                    quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + ", " +
                    quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) +
                    " WHERE " + exprPk + " AND (" + exprOther + ")";
  return sql;
}


static Value changesetValue( const std::string &v )
{
  Value w;
  w.setString( Value::TypeText, v.c_str(), v.size() );
  return w;
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
      std::string val = res.value( r, i );
      if ( reverse )
        e.oldValues.push_back( changesetValue( val ) );
      else
        e.newValues.push_back( changesetValue( val ) );
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
      std::string v1 = res.value( r, i + numColumns );
      std::string v2 = res.value( r, i );
      bool pkey = tbl.columns[i].isPrimaryKey;
      bool updated = v1 != v2;
      e.oldValues.push_back( ( pkey || updated ) ? changesetValue( v1 ) : Value() );
      e.newValues.push_back( updated ? changesetValue( v2 ) : Value() );
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


void PostgresDriver::applyChangeset( ChangesetReader &reader )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  // TODO
}
