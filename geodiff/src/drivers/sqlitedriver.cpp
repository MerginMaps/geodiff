/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "sqlitedriver.h"

#include "changesetreader.h"
#include "changesetwriter.h"
#include "changesetutils.h"

#include <memory.h>

void SqliteDriver::open( const DriverParametersMap &conn )
{
  auto connBaseIt = conn.find( "base" );
  if ( connBaseIt == conn.end() )
    throw GeoDiffException( "Missing 'base' file" );

  auto connModifiedIt = conn.find( "modified" );
  mHasModified = connModifiedIt != conn.end();

  std::string base = connBaseIt->second;

  mDb = std::make_shared<Sqlite3Db>();
  if ( mHasModified )
  {
    mDb->open( connModifiedIt->second );

    Buffer sqlBuf;
    sqlBuf.printf( "ATTACH '%s' AS aux", base.c_str() );
    mDb->exec( sqlBuf );
  }
  else
  {
    mDb->open( base );
  }

  // GeoPackage triggers require few functions like ST_IsEmpty() to be registered
  // in order to be able to apply changesets
  if ( isGeoPackage( mDb ) )
  {
    bool success = register_gpkg_extensions( mDb );
    if ( !success )
    {
      throw GeoDiffException( "Unable to enable sqlite3/gpkg extensions" );
    }
  }

}

std::vector<std::string> SqliteDriver::listTables( bool useModified )
{
  std::string dbName;
  if ( mHasModified )
  {
    dbName = useModified ? "main" : "aux";
  }
  else
  {
    if ( useModified )
      throw GeoDiffException( "'modified' table not open" );
    dbName = "main";
  }

  std::vector<std::string> tableNames;
  std::string all_tables_sql = "SELECT name FROM " + dbName + ".sqlite_master\n"
                               " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
                               " ORDER BY name";
  Sqlite3Stmt statament;
  statament.prepare( mDb, "%s", all_tables_sql.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const char *name = ( const char * )sqlite3_column_text( statament.get(), 0 );
    if ( !name )
      continue;

    std::string tableName( name );
    /* typically geopackage from ogr would have these (table name is simple)
    gpkg_contents
    gpkg_extensions
    gpkg_geometry_columns
    gpkg_ogr_contents
    gpkg_spatial_ref_sys
    gpkg_tile_matrix
    gpkg_tile_matrix_set
    rtree_simple_geometry_node
    rtree_simple_geometry_parent
    rtree_simple_geometry_rowid
    simple (or any other name(s) of layers)
    sqlite_sequence
    */

    // table handled by triggers trigger_*_feature_count_*
    if ( startsWith( tableName, "gpkg_ogr_contents" ) )
      continue;
    // table handled by triggers rtree_*_geometry_*
    if ( startsWith( tableName, "rtree_" ) )
      continue;
    // internal table for AUTOINCREMENT
    if ( tableName == "sqlite_sequence" )
      continue;

    tableNames.push_back( tableName );
  }

  // result is ordered by name
  return tableNames;
}

TableSchema SqliteDriver::tableSchema( const std::string &tableName, bool useModified )
{
  std::string dbName;
  if ( mHasModified )
  {
    dbName = useModified ? "main" : "aux";
  }
  else
  {
    if ( useModified )
      throw GeoDiffException( "'modified' table not open" );
    dbName = "main";
  }

  TableSchema tbl;

  Sqlite3Stmt statament;
  statament.prepare( mDb, "PRAGMA '%q'.table_info('%q')", dbName.c_str(), tableName.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const unsigned char *zName = sqlite3_column_text( statament.get(), 1 );
    if ( zName == nullptr )
      throw GeoDiffException( "NULL column name in table schema: " + tableName );

    TableColumnInfo columnInfo;
    columnInfo.name = ( const char * )zName;
    columnInfo.isPrimaryKey = sqlite3_column_int( statament.get(), 5 );
    tbl.columns.push_back( columnInfo );
  }
  return tbl;
}

/**
 * printf() with sqlite extensions - see https://www.sqlite.org/printf.html
 * for extra format options like %q or %Q
 */
static std::string sqlitePrintf( const char *zFormat, ... )
{
  va_list ap;
  va_start( ap, zFormat );
  char *zSql = sqlite3_vmprintf( zFormat, ap );
  va_end( ap );

  if ( zSql == nullptr )
  {
    throw GeoDiffException( "out of memory" );
  }
  std::string res = ( const char * )zSql;
  sqlite3_free( zSql );
  return res;
}

//! Constructs SQL query to get all rows that do not exist in the other table (used for insert and delete)
static std::string sqlFindInserted( const std::string &tableName, const TableSchema &tbl, bool reverse )
{
  std::string exprPk;
  for ( auto c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += sqlitePrintf( "\"%w\".\"%w\".\"%w\"=\"%w\".\"%w\".\"%w\"",
                              "main", tableName.c_str(), c.name.c_str(), "aux", tableName.c_str(), c.name.c_str() );
    }
  }

  std::string sql = sqlitePrintf( "SELECT * FROM \"%w\".\"%w\" WHERE NOT EXISTS ( SELECT 1 FROM \"%w\".\"%w\" WHERE %s)",
                                  reverse ? "aux" : "main", tableName.c_str(),
                                  reverse ? "main" : "aux", tableName.c_str(), exprPk.c_str() );
  return sql;
}

//! Constructs SQL query to get all modified rows for a single table
static std::string sqlFindModified( const std::string &tableName, const TableSchema &tbl )
{
  std::string exprPk;
  std::string exprOther;
  for ( auto c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += sqlitePrintf( "\"%w\".\"%w\".\"%w\"=\"%w\".\"%w\".\"%w\"",
                              "main", tableName.c_str(), c.name.c_str(), "aux", tableName.c_str(), c.name.c_str() );
    }
    else // not a primary key column
    {
      if ( !exprOther.empty() )
        exprOther += " OR ";
      exprOther += sqlitePrintf( "\"%w\".\"%w\".\"%w\" IS NOT \"%w\".\"%w\".\"%w\"",
                                 "main", tableName.c_str(), c.name.c_str(), "aux", tableName.c_str(), c.name.c_str() );
    }
  }

  std::string sql = sqlitePrintf( "SELECT * FROM \"%w\".\"%w\", \"%w\".\"%w\" WHERE %s AND (%s)",
                                  "main", tableName.c_str(), "aux", tableName.c_str(), exprPk.c_str(), exprOther.c_str() );
  return sql;
}


static bool valuesEqual( sqlite3_value *v1, sqlite3_value *v2 )
{
  int type1 = sqlite3_value_type( v1 );
  int type2 = sqlite3_value_type( v2 );
  if ( type1 != type2 )
    return false;

  if ( type1 == SQLITE_NULL )
    return true;
  else if ( type1 == SQLITE_INTEGER )
    return sqlite3_value_int64( v1 ) == sqlite3_value_int64( v2 );
  else if ( type1 == SQLITE_FLOAT )
    return sqlite3_value_double( v1 ) == sqlite3_value_double( v2 );
  else if ( type1 == SQLITE_TEXT )
  {
    return strcmp( ( const char * ) sqlite3_value_text( v1 ), ( const char * ) sqlite3_value_text( v2 ) ) == 0;
  }
  else if ( type1 == SQLITE_BLOB )
  {
    int len1 = sqlite3_value_bytes( v1 );
    int len2 = sqlite3_value_bytes( v2 );
    if ( len1 != len2 )
      return false;
    return memcmp( sqlite3_value_blob( v1 ), sqlite3_value_blob( v2 ), len1 ) == 0;
  }
  else
  {
    throw GeoDiffException( "Unexpected value type" );
  }
}

static Value changesetValue( sqlite3_value *v )
{
  Value x;
  int type = sqlite3_value_type( v );
  if ( type == SQLITE_NULL )
    x.setNull();
  else if ( type == SQLITE_INTEGER )
    x.setInt( sqlite3_value_int64( v ) );
  else if ( type == SQLITE_FLOAT )
    x.setDouble( sqlite3_value_double( v ) );
  else if ( type == SQLITE_TEXT )
    x.setString( Value::TypeText, ( const char * )sqlite3_value_text( v ), sqlite3_value_bytes( v ) );
  else if ( type == SQLITE_BLOB )
    x.setString( Value::TypeBlob, ( const char * )sqlite3_value_blob( v ), sqlite3_value_bytes( v ) );
  else
    throw GeoDiffException( "Unexpected value type" );

  return x;
}

static void handleInserted( const std::string &tableName, const TableSchema &tbl, bool reverse, std::shared_ptr<Sqlite3Db> db, ChangesetWriter &writer, bool &first )
{
  std::string sqlInserted = sqlFindInserted( tableName, tbl, reverse );
  Sqlite3Stmt statementI;
  statementI.prepare( db, "%s", sqlInserted.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statementI.get() ) )
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
      Sqlite3Value v( sqlite3_column_value( statementI.get(), static_cast<int>( i ) ) );
      if ( reverse )
        e.oldValues.push_back( changesetValue( v.value() ) );
      else
        e.newValues.push_back( changesetValue( v.value() ) );
    }

    writer.writeEntry( e );
  }
}

static void handleUpdated( const std::string &tableName, const TableSchema &tbl, std::shared_ptr<Sqlite3Db> db, ChangesetWriter &writer, bool &first )
{
  std::string sqlModified = sqlFindModified( tableName, tbl );

  Sqlite3Stmt statement;
  statement.prepare( db, "%s", sqlModified.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statement.get() ) )
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
      Sqlite3Value v1( sqlite3_column_value( statement.get(), static_cast<int>( i + numColumns ) ) );
      Sqlite3Value v2( sqlite3_column_value( statement.get(), static_cast<int>( i ) ) );
      bool pkey = tbl.columns[i].isPrimaryKey;
      bool updated = !valuesEqual( v1.value(), v2.value() );
      e.oldValues.push_back( ( pkey || updated ) ? changesetValue( v1.value() ) : Value() );
      e.newValues.push_back( updated ? changesetValue( v2.value() ) : Value() );
    }

    writer.writeEntry( e );
  }
}


void SqliteDriver::createChangeset( ChangesetWriter &writer )
{
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

    handleInserted( tableName, tbl, false, mDb, writer, first );  // INSERT
    handleInserted( tableName, tbl, true, mDb, writer, first );   // DELETE
    handleUpdated( tableName, tbl, mDb, writer, first );          // UPDATE
  }

}

static std::string sqlForInsert( const std::string &tableName, const TableSchema &tbl )
{
  /*
   * For a table defined like this: CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
   *
   * INSERT INTO x (a, b, c, d) VALUES (?, ?, ?, ?)
   */

  std::string sql;
  sql += sqlitePrintf( "INSERT INTO \"%w\" (", tableName.c_str() );
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += sqlitePrintf( "\"%w\"", tbl.columns[i].name.c_str() );
  }
  sql += ") VALUES (";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += "?";
  }
  sql += ")";
  return sql;
}

static std::string sqlForUpdate( const std::string &tableName, const TableSchema &tbl )
{
  /*
  ** For a table defined like this: CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
  **
  **     UPDATE x SET
  **     a = CASE WHEN ?2  THEN ?3  ELSE a END,
  **     b = CASE WHEN ?5  THEN ?6  ELSE b END,
  **     c = CASE WHEN ?8  THEN ?9  ELSE c END,
  **     d = CASE WHEN ?11 THEN ?12 ELSE d END
  **     WHERE a = ?1 AND c = ?7 AND (?13 OR
  **       (?5==0 OR b IS ?4) AND (?11==0 OR d IS ?10) AND
  **     )
  **
  ** For each column in the table, there are three variables to bind:
  **
  **     ?(i*3+1)    The old.* value of the column, if any.
  **     ?(i*3+2)    A boolean flag indicating that the value is being modified.
  **     ?(i*3+3)    The new.* value of the column, if any.
  */

  std::string sql;
  sql += sqlitePrintf( "UPDATE \"%w\" SET ", tableName.c_str() );

  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += sqlitePrintf( "\"%w\" = CASE WHEN ?%d THEN ?%d ELSE \"%w\" END", tbl.columns[i].name.c_str(), i * 3 + 2, i * 3 + 3, tbl.columns[i].name.c_str() );
  }
  sql += " WHERE ";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += " AND ";
    if ( tbl.columns[i].isPrimaryKey )
      sql += sqlitePrintf( " \"%w\" = ?%d ", tbl.columns[i].name.c_str(), i * 3 + 1 );
    else
      sql += sqlitePrintf( " ( ?%d = 0 OR \"%w\" IS ?%d ) ", i * 3 + 2, tbl.columns[i].name.c_str(), i * 3 + 1 );
  }

  return sql;
}

static std::string sqlForDelete( const std::string &tableName, const TableSchema &tbl )
{
  /*
   * For a table defined like this: CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
   *
   * DELETE FROM x WHERE a = ? AND b IS ? AND c = ? AND d IS ?
   */

  std::string sql;
  sql += sqlitePrintf( "DELETE FROM \"%w\" WHERE ", tableName.c_str() );
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += " AND ";
    if ( tbl.columns[i].isPrimaryKey )
      sql += sqlitePrintf( "\"%w\" = ?", tbl.columns[i].name.c_str() );
    else
      sql += sqlitePrintf( "\"%w\" IS ?", tbl.columns[i].name.c_str() );
  }
  return sql;
}

static void bindValue( sqlite3_stmt *stmt, int index, const Value &v )
{
  int rc;
  if ( v.type() == Value::TypeInt )
    rc = sqlite3_bind_int64( stmt, index, v.getInt() );
  else if ( v.type() == Value::TypeDouble )
    rc = sqlite3_bind_double( stmt, index, v.getDouble() );
  else if ( v.type() == Value::TypeNull )
    rc = sqlite3_bind_null( stmt, index );
  else if ( v.type() == Value::TypeText )
    rc = sqlite3_bind_text( stmt, index, v.getString().c_str(), -1, SQLITE_TRANSIENT );
  else if ( v.type() == Value::TypeBlob )
    rc = sqlite3_bind_blob( stmt, index, v.getString().c_str(), v.getString().size(), SQLITE_TRANSIENT );
  else
    throw GeoDiffException( "unexpected bind type" );

  if ( rc != SQLITE_OK )
    throw GeoDiffException( "bind failed" );
}

void SqliteDriver::applyChangeset( ChangesetReader &reader )
{
  std::string lastTableName;
  std::unique_ptr<Sqlite3Stmt> stmtInsert, stmtUpdate, stmtDelete;
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

      stmtInsert.reset( new Sqlite3Stmt );
      stmtInsert->prepare( mDb, sqlForInsert( tableName, tbl ) );

      stmtUpdate.reset( new Sqlite3Stmt );
      stmtUpdate->prepare( mDb, sqlForUpdate( tableName, tbl ) );

      stmtDelete.reset( new Sqlite3Stmt );
      stmtDelete->prepare( mDb, sqlForDelete( tableName, tbl ) );
    }

    if ( entry.op == SQLITE_INSERT )
    {
      for ( size_t i = 0; i < tbl.columns.size(); ++i )
      {
        const Value &v = entry.newValues[i];
        bindValue( stmtInsert->get(), static_cast<int>( i ) + 1, v );
      }
      if ( sqlite3_step( stmtInsert->get() ) != SQLITE_DONE )
        throw GeoDiffException( "failed to insert" );
      if ( sqlite3_changes( mDb->get() ) != 1 )
        throw GeoDiffException( "nothing inserted, but should have" );
      sqlite3_reset( stmtInsert->get() );
    }
    else if ( entry.op == SQLITE_UPDATE )
    {
      for ( size_t i = 0; i < tbl.columns.size(); ++i )
      {
        const Value &vOld = entry.oldValues[i];
        const Value &vNew = entry.newValues[i];
        sqlite3_bind_int( stmtUpdate->get(), static_cast<int>( i ) * 3 + 2, vNew.type() != Value::TypeUndefined );
        if ( vOld.type() != Value::TypeUndefined )
          bindValue( stmtUpdate->get(), static_cast<int>( i ) * 3 + 1, vOld );
        if ( vNew.type() != Value::TypeUndefined )
          bindValue( stmtUpdate->get(), static_cast<int>( i ) * 3 + 3, vNew );
      }
      if ( sqlite3_step( stmtUpdate->get() ) != SQLITE_DONE )
        throw GeoDiffException( "failed to update" );
      if ( sqlite3_changes( mDb->get() ) == 0 )
        throw GeoDiffException( "nothing updated, but should have" );
      sqlite3_reset( stmtUpdate->get() );
    }
    else if ( entry.op == SQLITE_DELETE )
    {
      for ( size_t i = 0; i < tbl.columns.size(); ++i )
      {
        const Value &v = entry.oldValues[i];
        bindValue( stmtDelete->get(), static_cast<int>( i ) + 1, v );
      }
      if ( sqlite3_step( stmtDelete->get() ) != SQLITE_DONE )
        throw GeoDiffException( "failed to delete" );
      if ( sqlite3_changes( mDb->get() ) == 0 )
        throw GeoDiffException( "nothing deleted, but should have" );
      sqlite3_reset( stmtDelete->get() );
    }
    else
      throw GeoDiffException( "Unexpected operation" );
  }
}
