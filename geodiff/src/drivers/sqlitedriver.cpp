/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "sqlitedriver.h"

#include "changesetreader.h"
#include "changesetwriter.h"
#include "changesetutils.h"
#include "geodifflogger.hpp"

#include <memory.h>


static void logApplyConflict( const std::string &type, const ChangesetEntry &entry )
{
  Logger::instance().warn( "CONFLICT: " + type + ":\n" + changesetEntryToJSON( entry ) );
}

/**
 * Wrapper around SQLite database wide mutex.
 */
class Sqlite3DbMutexLocker
{
  public:
    Sqlite3DbMutexLocker( std::shared_ptr<Sqlite3Db> db )
      : mDb( db )
    {
      sqlite3_mutex_enter( sqlite3_db_mutex( mDb.get()->get() ) );
    }
    ~Sqlite3DbMutexLocker()
    {
      sqlite3_mutex_leave( sqlite3_db_mutex( mDb.get()->get() ) );
    }

  private:
    std::shared_ptr<Sqlite3Db> mDb;
};

/**
 * Wrapper around SQLite Savepoint Transactions.
 *
 * Constructor start a trasaction, it needs to be confirmed by a call to commitChanges() when
 * changes are ready to be written. If commitChanges() is not called, changes since the constructor
 * will be rolled back (so that on exception everything gets cleaned up properly).
 */
class Sqlite3SavepointTransaction
{
  public:
    Sqlite3SavepointTransaction( std::shared_ptr<Sqlite3Db> db )
      : mDb( db )
    {
      if ( sqlite3_exec( mDb.get()->get(), "SAVEPOINT changeset_apply", 0, 0, 0 ) != SQLITE_OK )
        throw GeoDiffException( "Unable to start savepoint" );
    }

    ~Sqlite3SavepointTransaction()
    {
      if ( mDb )
      {
        // we had some problems - roll back any pending changes
        sqlite3_exec( mDb.get()->get(), "ROLLBACK TO changeset_apply", 0, 0, 0 );
        sqlite3_exec( mDb.get()->get(), "RELEASE changeset_apply", 0, 0, 0 );
      }
    }

    void commitChanges()
    {
      assert( mDb );
      // there were no errors - release the savepoint and our changes get saved
      if ( sqlite3_exec( mDb.get()->get(), "RELEASE changeset_apply", 0, 0, 0 ) != SQLITE_OK )
        throw GeoDiffException( "Failed to release savepoint" );
      // reset handler to the database so that the destructor does nothing
      mDb.reset();
    }

  private:
    std::shared_ptr<Sqlite3Db> mDb;
};


///////


void SqliteDriver::open( const DriverParametersMap &conn )
{
  DriverParametersMap::const_iterator connBaseIt = conn.find( "base" );
  if ( connBaseIt == conn.end() )
    throw GeoDiffException( "Missing 'base' file" );

  DriverParametersMap::const_iterator connModifiedIt = conn.find( "modified" );
  mHasModified = connModifiedIt != conn.end();

  std::string base = connBaseIt->second;
  if ( !fileexists( base ) )
  {
    throw GeoDiffException( "Missing 'base' file when opening sqlite driver: " + base );
  }

  mDb = std::make_shared<Sqlite3Db>();
  if ( mHasModified )
  {
    std::string modified = connModifiedIt->second;

    if ( !fileexists( modified ) )
    {
      throw GeoDiffException( "Missing 'modified' file when opening sqlite driver: " + modified );
    }

    mDb->open( modified );

    Buffer sqlBuf;
    sqlBuf.printf( "ATTACH '%q' AS aux", base.c_str() );
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

void SqliteDriver::create( const DriverParametersMap &conn, bool overwrite )
{
  DriverParametersMap::const_iterator connBaseIt = conn.find( "base" );
  if ( connBaseIt == conn.end() )
    throw GeoDiffException( "Missing 'base' file" );

  std::string base = connBaseIt->second;

  if ( overwrite )
  {
    fileremove( base );  // remove if the file exists already
  }

  mDb = std::make_shared<Sqlite3Db>();
  mDb->create( base );

  // register geopackage related functions in the newly created sqlite database
  if ( !register_gpkg_extensions( mDb ) )
  {
    throw GeoDiffException( "Unable to enable sqlite3/gpkg extensions" );
  }
}

std::string SqliteDriver::databaseName( bool useModified )
{
  if ( mHasModified )
  {
    return useModified ? "main" : "aux";
  }
  else
  {
    if ( useModified )
      throw GeoDiffException( "'modified' table not open" );
    return "main";
  }
}

std::vector<std::string> SqliteDriver::listTables( bool useModified )
{
  std::string dbName = databaseName( useModified );
  std::vector<std::string> tableNames;
  std::string all_tables_sql = "SELECT name FROM " + dbName + ".sqlite_master\n"
                               " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
                               " ORDER BY name";
  Sqlite3Stmt statement;
  statement.prepare( mDb, "%s", all_tables_sql.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statement.get() ) )
  {
    const char *name = ( const char * )sqlite3_column_text( statement.get(), 0 );
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
    if ( startsWith( tableName, "gpkg_" ) )
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

bool tableExists( std::shared_ptr<Sqlite3Db> db, const std::string &tableName, const std::string &dbName )
{
  Sqlite3Stmt stmtHasGeomColumnsInfo;
  stmtHasGeomColumnsInfo.prepare( db, "SELECT name FROM \"%w\".sqlite_master WHERE type='table' "
                                  "AND name='%q'", dbName.c_str(), tableName.c_str() );
  return sqlite3_step( stmtHasGeomColumnsInfo.get() ) == SQLITE_ROW;
}

TableSchema SqliteDriver::tableSchema( const std::string &tableName, bool useModified )
{
  std::string dbName = databaseName( useModified );

  if ( !tableExists( mDb, tableName, dbName ) )
    throw GeoDiffException( "Table does not exist: " + tableName );

  TableSchema tbl;
  tbl.name = tableName;
  std::map<std::string, std::string> columnTypes;

  Sqlite3Stmt statement;
  statement.prepare( mDb, "PRAGMA '%q'.table_info('%q')", dbName.c_str(), tableName.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statement.get() ) )
  {
    const unsigned char *zName = sqlite3_column_text( statement.get(), 1 );
    if ( zName == nullptr )
      throw GeoDiffException( "NULL column name in table schema: " + tableName );

    TableColumnInfo columnInfo;
    columnInfo.name = ( const char * )zName;
    columnInfo.isNotNull = sqlite3_column_int( statement.get(), 3 );
    columnInfo.isPrimaryKey = sqlite3_column_int( statement.get(), 5 );
    columnTypes[columnInfo.name] = ( const char * ) sqlite3_column_text( statement.get(), 2 );

    tbl.columns.push_back( columnInfo );
  }

  // check if the geometry columns table is present (it may not be if this is a "pure" sqlite file)
  if ( tableExists( mDb, "gpkg_geometry_columns", dbName ) )
  {
    //
    // get geometry column details (geometry type, whether it has Z/M values, CRS id)
    //

    int srsId = -1;
    Sqlite3Stmt stmtGeomCol;
    stmtGeomCol.prepare( mDb, "SELECT * FROM \"%w\".gpkg_geometry_columns WHERE table_name = '%q'", dbName.c_str(), tableName.c_str() );
    if ( SQLITE_ROW == sqlite3_step( stmtGeomCol.get() ) )
    {
      const unsigned char *chrColumnName = sqlite3_column_text( stmtGeomCol.get(), 1 );
      const unsigned char *chrTypeName = sqlite3_column_text( stmtGeomCol.get(), 2 );
      if ( chrColumnName == nullptr )
        throw GeoDiffException( "NULL column name in gpkg_geometry_columns: " + tableName );
      if ( chrTypeName == nullptr )
        throw GeoDiffException( "NULL type name in gpkg_geometry_columns: " + tableName );

      std::string geomColName = ( const char * ) chrColumnName;
      std::string geomTypeName = ( const char * ) chrTypeName;
      srsId = sqlite3_column_int( stmtGeomCol.get(), 3 );
      bool hasZ = sqlite3_column_int( stmtGeomCol.get(), 4 );
      bool hasM = sqlite3_column_int( stmtGeomCol.get(), 5 );

      size_t i = tbl.columnFromName( geomColName );
      if ( i == SIZE_MAX )
        throw GeoDiffException( "Inconsistent entry in gpkg_geometry_columns - geometry column not found: " + geomColName );

      TableColumnInfo &col = tbl.columns[i];
      col.setGeometry( geomTypeName, srsId, hasM, hasZ );
    }

    //
    // get CRS information
    //

    if ( srsId != -1 )
    {
      Sqlite3Stmt stmtCrs;
      stmtCrs.prepare( mDb, "SELECT * FROM \"%w\".gpkg_spatial_ref_sys WHERE srs_id = %d", dbName.c_str(), srsId );
      if ( SQLITE_ROW != sqlite3_step( stmtCrs.get() ) )
        throw GeoDiffException( "Unable to find entry in gpkg_spatial_ref_sys for srs_id = " + std::to_string( srsId ) );

      const unsigned char *chrAuthName = sqlite3_column_text( stmtCrs.get(), 2 );
      const unsigned char *chrWkt = sqlite3_column_text( stmtCrs.get(), 4 );
      if ( chrAuthName == nullptr )
        throw GeoDiffException( "NULL auth name in gpkg_spatial_ref_sys: " + tableName );
      if ( chrWkt == nullptr )
        throw GeoDiffException( "NULL definition in gpkg_spatial_ref_sys: " + tableName );

      tbl.crs.srsId = srsId;
      tbl.crs.authName = ( const char * ) chrAuthName;
      tbl.crs.authCode = sqlite3_column_int( stmtCrs.get(), 3 );
      tbl.crs.wkt = ( const char * ) chrWkt;
    }
  }

  // update column types
  for ( auto const &it : columnTypes )
  {
    size_t i = tbl.columnFromName( it.first );
    TableColumnInfo &col = tbl.columns[i];
    tbl.columns[i].type = columnType( it.second, Driver::SQLITEDRIVERNAME, col.isGeometry );

    if ( col.isPrimaryKey && ( lowercaseString( col.type.dbType ) == "integer" ) )
    {
      // sqlite uses auto-increment automatically for INTEGER PRIMARY KEY - https://sqlite.org/autoinc.html
      col.isAutoIncrement = true;
    }
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
  for ( const TableColumnInfo &c : tbl.columns )
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
  for ( const TableColumnInfo &c : tbl.columns )
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
  std::string sql;

  if ( exprOther.empty() )
  {
    sql = sqlitePrintf( "SELECT * FROM \"%w\".\"%w\", \"%w\".\"%w\" WHERE %s",
                        "main", tableName.c_str(), "aux", tableName.c_str(), exprPk.c_str() );
  }
  else
  {
    sql = sqlitePrintf( "SELECT * FROM \"%w\".\"%w\", \"%w\".\"%w\" WHERE %s AND (%s)",
                        "main", tableName.c_str(), "aux", tableName.c_str(), exprPk.c_str(), exprOther.c_str() );
  }

  return sql;
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

    bool hasUpdates = false;
    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Sqlite3Value v1( sqlite3_column_value( statement.get(), static_cast<int>( i + numColumns ) ) );
      Sqlite3Value v2( sqlite3_column_value( statement.get(), static_cast<int>( i ) ) );
      bool pkey = tbl.columns[i].isPrimaryKey;
      bool updated = ( v1 != v2 );
      if ( updated )
      {
        // Let's do a secondary check for some column types to avoid false positives, for example
        // multiple different string representations could be used for a single datetime value,
        // see "Time Values" section in https://sqlite.org/lang_datefunc.html
        if ( tbl.columns[i].type == TableColumnType::DATETIME )
        {
          Sqlite3Stmt stmtDatetime;
          stmtDatetime.prepare( db, "SELECT datetime(?1) IS NOT datetime(?2)" );
          sqlite3_bind_value( stmtDatetime.get(), 1, v1.value() );
          sqlite3_bind_value( stmtDatetime.get(), 2, v2.value() );
          if ( SQLITE_ROW == sqlite3_step( stmtDatetime.get() ) )
          {
            updated = sqlite3_column_int( stmtDatetime.get(), 0 );
          }
        }

        if ( updated )
        {
          hasUpdates = true;
        }
      }
      e.oldValues.push_back( ( pkey || updated ) ? changesetValue( v1.value() ) : Value() );
      e.newValues.push_back( updated ? changesetValue( v2.value() ) : Value() );
    }

    if ( hasUpdates )
    {
      if ( first )
      {
        ChangesetTable chTable = schemaToChangesetTable( tableName, tbl );
        writer.beginTable( chTable );
        first = false;
      }

      writer.writeEntry( e );
    }
  }
}

void SqliteDriver::createChangeset( ChangesetWriter &writer )
{
  std::vector<std::string> tablesBase = listTables( false );
  std::vector<std::string> tablesModified = listTables( true );

  if ( tablesBase != tablesModified )
  {
    throw GeoDiffException( "Table names are not matching between the input databases.\n"
                            "Base:     " + concatNames( tablesBase ) + "\n" +
                            "Modified: " + concatNames( tablesModified ) );
  }

  for ( const std::string &tableName : tablesBase )
  {
    TableSchema tbl = tableSchema( tableName );
    TableSchema tblNew = tableSchema( tableName, true );

    // test that table schema in the modified is the same
    if ( tbl != tblNew )
    {
      if ( !tbl.compareWithBaseTypes( tblNew ) )
        throw GeoDiffException( "GeoPackage Table schemas are not the same for table: " + tableName );
    }

    if ( !tbl.hasPrimaryKey() )
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
    else if ( tbl.columns[i].type.baseType == TableColumnType::DATETIME )
    {
      // compare date/time values using datetime() because they may have
      // multiple equivalent string representations (see #143)
      sql += sqlitePrintf( " ( ?%d = 0 OR datetime(\"%w\") IS datetime(?%d) ) ", i * 3 + 2, tbl.columns[i].name.c_str(), i * 3 + 1 );
    }
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
    else if ( tbl.columns[i].type.baseType == TableColumnType::DATETIME )
    {
      // compare date/time values using datetime() because they may have
      // multiple equivalent string representations (see #143)
      sql += sqlitePrintf( "datetime(\"%w\") IS datetime(?)", tbl.columns[i].name.c_str() );
    }
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

  // this will acquire DB mutex and release it when the function ends (or when an exception is thrown)
  Sqlite3DbMutexLocker dbMutexLocker( mDb );

  // start transaction!
  Sqlite3SavepointTransaction savepointTransaction( mDb );

  // TODO: when we deal with foreign keys, it may be useful to temporarily set "PRAGMA defer_foreign_keys = 1"
  // so that if a foreign key is violated, the constraint violation is tolerated until we commit the changes
  // (and at that point the violation may have been fixed by some other commands). Sqlite3session does that.

  // get all triggers sql commands
  // that we do not recognize (gpkg triggers are filtered)
  std::vector<std::string> triggerNames;
  std::vector<std::string> triggerCmds;
  sqliteTriggers( mDb, triggerNames, triggerCmds );

  Sqlite3Stmt statament;
  for ( std::string name : triggerNames )
  {
    statament.prepare( mDb, "drop trigger '%q'", name.c_str() );
    sqlite3_step( statament.get() );
    statament.close();
  }

  int conflictCount = 0;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    std::string tableName = entry.table->name;

    if ( startsWith( tableName, "gpkg_" ) )
      continue;   // skip any changes to GPKG meta tables

    if ( tableName != lastTableName )
    {
      lastTableName = tableName;
      tbl = tableSchema( tableName );

      if ( tbl.columns.size() == 0 )
        throw GeoDiffException( "No such table: " + tableName );

      if ( tbl.columns.size() != entry.table->columnCount() )
        throw GeoDiffException( "Wrong number of columns for table: " + tableName );

      for ( size_t i = 0; i < entry.table->columnCount(); ++i )
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
      {
        // it could be that the primary key already exists or some constraint violation (e.g. not null, unique)
        logApplyConflict( "insert_failed", entry );
        ++conflictCount;
      }
      if ( sqlite3_changes( mDb->get() ) != 1 )
        throw GeoDiffException( "Nothing inserted (this should never happen)" );
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
      {
        // a constraint must have been violated (e.g. foreign key, not null, unique)
        logApplyConflict( "update_failed", entry );
        ++conflictCount;
      }
      if ( sqlite3_changes( mDb->get() ) == 0 )
      {
        // either the row with such pkey does not exist or its data have been modified
        logApplyConflict( "update_nothing", entry );
        ++conflictCount;
      }
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
      {
        // a foreign key constraint must have been violated
        logApplyConflict( "delete_failed", entry );
        ++conflictCount;
      }
      if ( sqlite3_changes( mDb->get() ) == 0 )
      {
        // either the row with such pkey does not exist or its data have been modified
        logApplyConflict( "delete_nothing", entry );
        ++conflictCount;
      }
      sqlite3_reset( stmtDelete->get() );
    }
    else
      throw GeoDiffException( "Unexpected operation" );
  }

  // recreate triggers
  for ( std::string cmd : triggerCmds )
  {
    statament.prepare( mDb, "%s", cmd.c_str() );
    sqlite3_step( statament.get() );
    statament.close();
  }

  if ( !conflictCount )
  {
    savepointTransaction.commitChanges();
  }
  else
  {
    throw GeoDiffException( "Conflicts encountered while applying changes! Total " + std::to_string( conflictCount ) );
  }
}


static void addGpkgCrsDefinition( std::shared_ptr<Sqlite3Db> db, const CrsDefinition &crs )
{
  // gpkg_spatial_ref_sys
  //   srs_name TEXT NOT NULL, srs_id INTEGER NOT NULL PRIMARY KEY,
  //   organization TEXT NOT NULL, organization_coordsys_id INTEGER NOT NULL,
  //   definition  TEXT NOT NULL, description TEXT

  Sqlite3Stmt stmtCheck;
  stmtCheck.prepare( db, "select count(*) from gpkg_spatial_ref_sys where srs_id = %d;", crs.srsId );
  int res = sqlite3_step( stmtCheck.get() );
  if ( res != SQLITE_ROW )
    throw GeoDiffException( "Failed to access gpkg_spatial_ref_sys table" );

  if ( sqlite3_column_int( stmtCheck.get(), 0 ) )
    return;  // already there

  Sqlite3Stmt stmt;
  stmt.prepare( db, "INSERT INTO gpkg_spatial_ref_sys VALUES ('%q:%d', %d, '%q', %d, '%q', '')",
                crs.authName.c_str(), crs.authCode, crs.srsId, crs.authName.c_str(), crs.authCode,
                crs.wkt.c_str() );
  res = sqlite3_step( stmt.get() );
  if ( res != SQLITE_DONE )
    throw GeoDiffException( "Failed to insert CRS to gpkg_spatial_ref_sys table" );
}

static void addGpkgSpatialTable( std::shared_ptr<Sqlite3Db> db, const TableSchema &tbl, const Extent &extent )
{
  size_t i = tbl.geometryColumn();
  if ( i == SIZE_MAX )
    throw GeoDiffException( "Adding non-spatial tables is not supported: " + tbl.name );

  const TableColumnInfo &col = tbl.columns[i];
  std::string geomColumn = col.name;
  std::string geomType = col.geomType;
  int srsId = col.geomSrsId;
  bool hasZ = col.geomHasZ;
  bool hasM = col.geomHasM;

  // gpkg_contents
  //   table_name TEXT NOT NULL PRIMARY KEY, data_type TEXT NOT NULL,
  //   identifier TEXT, description TEXT DEFAULT '',
  //   last_change DATETIME NOT NULL DEFAULT (...),
  //   min_x DOUBLE, min_y DOUBLE, max_x DOUBLE, max_y DOUBLE,
  //   srs_id INTEGER

  Sqlite3Stmt stmt;
  stmt.prepare( db, "INSERT INTO gpkg_contents (table_name, data_type, identifier, min_x, min_y, max_x, max_y, srs_id) "
                "VALUES ('%q', 'features', '%q', %f, %f, %f, %f, %d)",
                tbl.name.c_str(), tbl.name.c_str(), extent.minX, extent.minY, extent.maxX, extent.maxY, srsId );
  int res = sqlite3_step( stmt.get() );
  if ( res != SQLITE_DONE )
    throw GeoDiffException( "Failed to insert row to gpkg_contents table" );

  // gpkg_geometry_columns
  //   table_name TEXT NOT NULL, column_name TEXT NOT NULL,
  //   geometry_type_name TEXT NOT NULL, srs_id INTEGER NOT NULL,
  //   z TINYINT NOT NULL,m TINYINT NOT NULL

  Sqlite3Stmt stmtGeomCol;
  stmtGeomCol.prepare( db, "INSERT INTO gpkg_geometry_columns VALUES ('%q', '%q', '%q', %d, %d, %d)",
                       tbl.name.c_str(), geomColumn.c_str(), geomType.c_str(), srsId, hasZ, hasM );
  res = sqlite3_step( stmtGeomCol.get() );
  if ( res != SQLITE_DONE )
    throw GeoDiffException( "Failed to insert row to gpkg_geometry_columns table" );
}

void SqliteDriver::createTables( const std::vector<TableSchema> &tables )
{
  // currently we always create geopackage meta tables. Maybe in the future we can skip
  // that if there is a reason, and have that optional if none of the tables are spatial.
  Sqlite3Stmt stmt;
  stmt.prepare( mDb, "SELECT InitSpatialMetadata('main');" );
  int res = sqlite3_step( stmt.get() );
  if ( res != SQLITE_ROW )
    throw GeoDiffException( "Failure initializing spatial metadata" );

  for ( const TableSchema &tbl : tables )
  {
    if ( startsWith( tbl.name, "gpkg_" ) )
      continue;

    if ( tbl.geometryColumn() != SIZE_MAX )
    {
      addGpkgCrsDefinition( mDb, tbl.crs );
      addGpkgSpatialTable( mDb, tbl, Extent() );   // TODO: is it OK to set zeros?
    }

    std::string sql, pkeyCols, columns;
    for ( const TableColumnInfo &c : tbl.columns )
    {
      if ( !columns.empty() )
        columns += ", ";

      columns += sqlitePrintf( "\"%w\" %s", c.name.c_str(), c.type.dbType.c_str() );

      if ( c.isNotNull )
        columns += " NOT NULL";

      // we have also c.isAutoIncrement, but the SQLite AUTOINCREMENT keyword only applies
      // to primary keys, and according to the docs, ordinary tables with INTEGER PRIMARY KEY column
      // (which becomes alias to ROWID) does auto-increment, and AUTOINCREMENT just prevents
      // reuse of ROWIDs from previously deleted rows.
      // See https://sqlite.org/autoinc.html

      if ( c.isPrimaryKey )
      {
        if ( !pkeyCols.empty() )
          pkeyCols += ", ";
        pkeyCols += sqlitePrintf( "\"%w\"", c.name.c_str() );
      }
    }

    sql = sqlitePrintf( "CREATE TABLE \"%w\".\"%w\" (", "main", tbl.name.c_str() );
    sql += columns;
    sql += ", PRIMARY KEY (" + pkeyCols + ")";
    sql += ");";

    Sqlite3Stmt stmt;
    stmt.prepare( mDb, sql );
    if ( sqlite3_step( stmt.get() ) != SQLITE_DONE )
      throw GeoDiffException( "Failure creating table: " + tbl.name );
  }
}


void SqliteDriver::dumpData( ChangesetWriter &writer, bool useModified )
{
  std::string dbName = databaseName( useModified );
  std::vector<std::string> tables = listTables();
  for ( const std::string &tableName : tables )
  {
    TableSchema tbl = tableSchema( tableName, useModified );
    if ( !tbl.hasPrimaryKey() )
      continue;  // ignore tables without primary key - they can't be compared properly

    bool first = true;
    Sqlite3Stmt statementI;
    statementI.prepare( mDb, "SELECT * FROM \"%w\".\"%w\"", dbName.c_str(), tableName.c_str() );
    while ( SQLITE_ROW == sqlite3_step( statementI.get() ) )
    {
      if ( first )
      {
        writer.beginTable( schemaToChangesetTable( tableName, tbl ) );
        first = false;
      }

      ChangesetEntry e;
      e.op = ChangesetEntry::OpInsert;
      size_t numColumns = tbl.columns.size();
      for ( size_t i = 0; i < numColumns; ++i )
      {
        Sqlite3Value v( sqlite3_column_value( statementI.get(), static_cast<int>( i ) ) );
        e.newValues.push_back( changesetValue( v.value() ) );
      }
      writer.writeEntry( e );
    }
  }
}

void SqliteDriver::checkCompatibleForRebase( bool useModified )
{
  std::string dbName = databaseName( useModified );

  // get all triggers sql commands
  // and make sure that there are only triggers we recognize
  // we deny rebase changesets with unrecognized triggers
  std::vector<std::string> triggerNames;
  std::vector<std::string> triggerCmds;
  sqliteTriggers( mDb, triggerNames, triggerCmds );  // TODO: use dbName
  if ( !triggerNames.empty() )
  {
    std::string msg = "Unable to perform rebase for database with unknown triggers:\n";
    for ( size_t i = 0; i < triggerNames.size(); ++i )
      msg += triggerNames[i] + "\n";
    throw GeoDiffException( msg );
  }

  ForeignKeys fks = sqliteForeignKeys( mDb, dbName );
  if ( !fks.empty() )
  {
    throw GeoDiffException( "Unable to perform rebase for database with foreign keys" );
  }
}
