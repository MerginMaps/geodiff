/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "sqlitedriver.h"

#include "changeset.h"
#include "changesetreader.h"
#include "changesetwriter.h"
#include "changesetutils.h"
#include "driver.h"
#include "geodiffcontext.hpp"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include "sqliteutils.h"
#include "tableschema.h"
#include "tableschemadiff.hpp"

#include <memory.h>
#include <sqlite3.h>
#include <unordered_map>
#include <variant>


void SqliteDriver::logApplyConflict( const std::string &type, const ChangesetEntry &entry, bool isDbErr ) const
{
  std::string msg = "CONFLICT: " + type;
  if ( isDbErr )
    msg += " (" + std::string( sqlite3_errmsg( mDb->get() ) ) + ")";
  msg += ":\n" + changesetEntryToJSON( entry ).dump( 2 );
  context()->logger().warn( msg );
}

/**
 * Wrapper around SQLite database wide mutex.
 */
class Sqlite3DbMutexLocker
{
  public:
    explicit Sqlite3DbMutexLocker( std::shared_ptr<Sqlite3Db> db )
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
    explicit Sqlite3SavepointTransaction( const Context *context, std::shared_ptr<Sqlite3Db> db )
      : mDb( db ), mContext( context )
    {
      if ( sqlite3_exec( mDb.get()->get(), "SAVEPOINT changeset_apply", 0, 0, 0 ) != SQLITE_OK )
      {
        throwSqliteError( mDb.get()->get(), "Unable to start savepoint transaction" );
      }
    }

    ~Sqlite3SavepointTransaction()
    {
      if ( mDb )
      {
        // we had some problems - roll back any pending changes
        if ( sqlite3_exec( mDb.get()->get(), "ROLLBACK TO changeset_apply", 0, 0, 0 ) != SQLITE_OK )
        {
          logSqliteError( mContext, mDb, "Unable to rollback savepoint transaction" );
        }
        if ( sqlite3_exec( mDb.get()->get(), "RELEASE changeset_apply", 0, 0, 0 ) != SQLITE_OK )
        {
          logSqliteError( mContext, mDb, "Unable to release savepoint" );
        }
      }
    }

    void commitChanges()
    {
      assert( mDb );
      // there were no errors - release the savepoint and our changes get saved
      if ( sqlite3_exec( mDb.get()->get(), "RELEASE changeset_apply", 0, 0, 0 ) != SQLITE_OK )
      {
        throwSqliteError( mDb.get()->get(), "Failed to release savepoint" );
      }
      // reset handler to the database so that the destructor does nothing
      mDb.reset();
    }

  private:
    std::shared_ptr<Sqlite3Db> mDb;
    const Context *mContext;
};


///////


SqliteDriver::SqliteDriver( const Context *context )
  : Driver( context )
{
}

// Opens 'base' DB (with implicit schema called 'main') and optionally
// 'modified' DB (with explicit schema 'modified')
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
  mDb->open( base );

  if ( mHasModified )
  {
    std::string modified = connModifiedIt->second;

    if ( !fileexists( modified ) )
    {
      throw GeoDiffException( "Missing 'modified' file when opening sqlite driver: " + modified );
    }

    {
      Buffer sqlBuf;
      sqlBuf.printf( "ATTACH '%q' AS modified", modified.c_str() );
      mDb->exec( sqlBuf );
    }
  }

  // GeoPackage triggers require few functions like ST_IsEmpty() to be registered
  // in order to be able to apply changesets
  if ( isGeoPackage( context(), mDb ) )
  {
    register_gpkg_extensions( mDb );
  }

  // Enable foreign key constraints (if the database has any)
  Buffer sqlBuf;
  sqlBuf.printf( "PRAGMA foreign_keys = 1" );
  mDb->exec( sqlBuf );
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
  register_gpkg_extensions( mDb );
}

std::string SqliteDriver::databaseName( bool useModified )
{
  if ( mHasModified )
  {
    return useModified ? "modified" : "main";
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
  int rc;
  while ( SQLITE_ROW == ( rc = sqlite3_step( statement.get() ) ) )
  {
    const char *name = reinterpret_cast<const char *>( sqlite3_column_text( statement.get(), 0 ) );
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

    if ( context()->isTableSkipped( tableName ) )
      continue;

    tableNames.push_back( tableName );
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context(), mDb, "Failed to list SQLite tables" );
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

TableSchema SqliteDriver::tableSchema( const std::string &tableName,
                                       bool useModified )
{
  std::string dbName = databaseName( useModified );

  if ( !tableExists( mDb, tableName, dbName ) )
    throw GeoDiffException( "Table does not exist: " + tableName );

  TableSchema tbl;
  tbl.name = tableName;
  std::map<std::string, std::string> columnTypes;

  Sqlite3Stmt statement;
  statement.prepare( mDb, "PRAGMA '%q'.table_info('%q')", dbName.c_str(), tableName.c_str() );
  int rc;
  while ( SQLITE_ROW == ( rc = sqlite3_step( statement.get() ) ) )
  {
    const unsigned char *zName = sqlite3_column_text( statement.get(), 1 );
    if ( zName == nullptr )
      throw GeoDiffException( "NULL column name in table schema: " + tableName );

    TableColumnInfo columnInfo;
    columnInfo.name = reinterpret_cast<const char *>( zName );
    columnInfo.isNotNull = sqlite3_column_int( statement.get(), 3 );
    columnInfo.isPrimaryKey = sqlite3_column_int( statement.get(), 5 );
    columnTypes[columnInfo.name] = reinterpret_cast<const char *>( sqlite3_column_text( statement.get(), 2 ) );

    tbl.columns.push_back( columnInfo );
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context(), mDb, "Failed to get list columns for table " + tableName );
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
    while ( SQLITE_ROW == ( rc = sqlite3_step( stmtGeomCol.get() ) ) )
    {
      const unsigned char *chrColumnName = sqlite3_column_text( stmtGeomCol.get(), 1 );
      const unsigned char *chrTypeName = sqlite3_column_text( stmtGeomCol.get(), 2 );
      if ( chrColumnName == nullptr )
        throw GeoDiffException( "NULL column name in gpkg_geometry_columns: " + tableName );
      if ( chrTypeName == nullptr )
        throw GeoDiffException( "NULL type name in gpkg_geometry_columns: " + tableName );

      std::string geomColName = reinterpret_cast<const char *>( chrColumnName );
      std::string geomTypeName = reinterpret_cast<const char *>( chrTypeName );
      srsId = sqlite3_column_int( stmtGeomCol.get(), 3 );
      bool hasZ = sqlite3_column_int( stmtGeomCol.get(), 4 );
      bool hasM = sqlite3_column_int( stmtGeomCol.get(), 5 );

      size_t i = tbl.columnFromName( geomColName );
      if ( i == SIZE_MAX )
        throw GeoDiffException( "Inconsistent entry in gpkg_geometry_columns - geometry column not found: " + geomColName );

      TableColumnInfo &col = tbl.columns[i];
      col.setGeometry( geomTypeName, srsId, hasM, hasZ );
    }
    if ( rc != SQLITE_DONE )
    {
      logSqliteError( context(), mDb, "Failed to get geometry column info for table " + tableName );
    }

    //
    // get CRS information
    //

    if ( srsId != -1 )
    {
      Sqlite3Stmt stmtCrs;
      stmtCrs.prepare( mDb, "SELECT * FROM \"%w\".gpkg_spatial_ref_sys WHERE srs_id = %d", dbName.c_str(), srsId );
      if ( SQLITE_ROW != sqlite3_step( stmtCrs.get() ) )
      {
        throwSqliteError( mDb->get(), "Unable to find entry in gpkg_spatial_ref_sys for srs_id = " + std::to_string( srsId ) );
      }

      const unsigned char *chrAuthName = sqlite3_column_text( stmtCrs.get(), 2 );
      const unsigned char *chrWkt = sqlite3_column_text( stmtCrs.get(), 4 );
      if ( chrAuthName == nullptr )
        throw GeoDiffException( "NULL auth name in gpkg_spatial_ref_sys: " + tableName );
      if ( chrWkt == nullptr )
        throw GeoDiffException( "NULL definition in gpkg_spatial_ref_sys: " + tableName );

      tbl.crs.srsId = srsId;
      tbl.crs.authName = reinterpret_cast<const char *>( chrAuthName );
      tbl.crs.authCode = sqlite3_column_int( stmtCrs.get(), 3 );
      tbl.crs.wkt = reinterpret_cast<const char *>( chrWkt );
    }
  }

  // update column types
  for ( auto const &it : columnTypes )
  {
    size_t i = tbl.columnFromName( it.first );
    TableColumnInfo &col = tbl.columns[i];
    tbl.columns[i].type = columnType( context(), it.second, Driver::SQLITEDRIVERNAME, col.isGeometry );

    if ( col.isPrimaryKey && ( lowercaseString( col.type.dbType ) == "integer" ) )
    {
      // sqlite uses auto-increment automatically for INTEGER PRIMARY KEY - https://sqlite.org/autoinc.html
      col.isAutoIncrement = true;
    }
  }

  return tbl;
}

DatabaseSchema SqliteDriver::getSchema( bool useModified )
{
  std::vector<TableSchema> tables;
  for ( const std::string &name : listTables( useModified ) )
  {
    tables.push_back( tableSchema( name, useModified ) );
  }
  return {tables};
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
  std::string res = reinterpret_cast<const char *>( zSql );
  sqlite3_free( zSql );
  return res;
}

struct TableDiffContext
{
  std::shared_ptr<Sqlite3Db> db;
  const TableSchema &schemaBase;
  const TableSchema &schemaModified;
  std::vector<TableColumnInfo> commonColumns;
  std::vector<TableColumnInfo> newColumns;
  ChangesetWriter &writer;
  bool tableEntryWritten = false;
};

static std::string sqlColumnsStr( const TableDiffContext &diffContext, bool reverse )
{
  const char *tableName = ( reverse ? diffContext.schemaBase.name : diffContext.schemaModified.name ).c_str();

  std::string colsStr; // Column list equivalent to modified schema
  for ( const TableColumnInfo &c : diffContext.schemaModified.columns )
  {
    if ( !colsStr.empty() )
      colsStr += ", ";
    if ( reverse )
    {
      // Check if this column also exists in base and NULL it out if not
      bool found = false;
      for ( const auto &commonCol : diffContext.commonColumns )
      {
        if ( commonCol.name == c.name )
        {
          found = true;
          break;
        }
      }
      if ( !found )
      {
        colsStr += sqlitePrintf( "NULL AS \"%w\"", c.name.c_str() );
        continue;
      }
    }
    colsStr += sqlitePrintf( "\"%w\".\"%w\".\"%w\"",
                             reverse ? "main" : "modified", tableName, c.name.c_str() );
  }
  return colsStr;
}

//! Constructs SQL query to get all rows that do not exist in the other table (used for insert and delete)
static std::string sqlFindInserted( const TableDiffContext &diffContext, bool reverse )
{
  const char *baseTableName = diffContext.schemaBase.name.c_str();
  const char *modifiedTableName = diffContext.schemaModified.name.c_str();

  std::string exprPk; // Filter expression checking primary key is equal
  for ( const TableColumnInfo &c : diffContext.commonColumns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += sqlitePrintf( "\"modified\".\"%w\".\"%w\"=\"main\".\"%w\".\"%w\"",
                              modifiedTableName, c.name.c_str(), baseTableName, c.name.c_str() );
    }
  }

  std::string sql = sqlitePrintf( "SELECT %s FROM \"%w\".\"%w\" WHERE NOT EXISTS ( SELECT 1 FROM \"%w\".\"%w\" WHERE %s)",
                                  sqlColumnsStr( diffContext, reverse ).c_str(),
                                  reverse ? "main" : "modified", reverse ? baseTableName : modifiedTableName,
                                  reverse ? "modified" : "main", reverse ? modifiedTableName : baseTableName, exprPk.c_str() );
  return sql;
}

//! Constructs SQL query to get all modified rows for a single table
static std::string sqlFindModified( const TableDiffContext &diffContext )
{
  const char *baseTableName = diffContext.schemaBase.name.c_str();
  const char *modifiedTableName = diffContext.schemaModified.name.c_str();

  std::string exprPk;
  std::string exprOther;
  for ( const TableColumnInfo &c : diffContext.commonColumns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += sqlitePrintf( "\"modified\".\"%w\".\"%w\"=\"main\".\"%w\".\"%w\"",
                              modifiedTableName, c.name.c_str(), baseTableName, c.name.c_str() );
    }
    else // not a primary key column
    {
      if ( !exprOther.empty() )
        exprOther += " OR ";

      exprOther += sqlitePrintf( "\"modified\".\"%w\".\"%w\" IS NOT \"main\".\"%w\".\"%w\"",
                                 modifiedTableName, c.name.c_str(), baseTableName, c.name.c_str() );
    }
  }

  // Check for non-NULL values in newly-added columns
  for ( const TableColumnInfo &c : diffContext.newColumns )
  {
    if ( !exprOther.empty() )
      exprOther += " OR ";

    exprOther += sqlitePrintf( "\"modified\".\"%w\".\"%w\" IS NOT NULL",
                               modifiedTableName, c.name.c_str() );
  }

  std::string colsStr = sqlColumnsStr( diffContext, false ) + ", " + sqlColumnsStr( diffContext, true );

  if ( exprOther.empty() )
  {
    return sqlitePrintf( "SELECT %s FROM \"modified\".\"%w\", \"main\".\"%w\" WHERE %s",
                         colsStr.c_str(), modifiedTableName, baseTableName, exprPk.c_str() );
  }
  else
  {
    return sqlitePrintf( "SELECT %s FROM \"modified\".\"%w\", \"main\".\"%w\" WHERE %s AND (%s)",
                         colsStr.c_str(), modifiedTableName, baseTableName, exprPk.c_str(), exprOther.c_str() );
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
    x.setString( Value::TypeText, reinterpret_cast<const char *>( sqlite3_value_text( v ) ), sqlite3_value_bytes( v ) );
  else if ( type == SQLITE_BLOB )
    x.setString( Value::TypeBlob, reinterpret_cast<const char *>( sqlite3_value_blob( v ) ), sqlite3_value_bytes( v ) );
  else
    throw GeoDiffException( "Unexpected value type" );

  return x;
}

static void handleInserted( const Context *context, TableDiffContext &diffContext, bool reverse )
{
  std::string sqlInserted = sqlFindInserted( diffContext, reverse );
  Sqlite3Stmt statementI;
  statementI.prepare( diffContext.db, "%s", sqlInserted.c_str() );
  int rc;
  while ( SQLITE_ROW == ( rc = sqlite3_step( statementI.get() ) ) )
  {
    if ( !diffContext.tableEntryWritten )
    {
      ChangesetTable chTable = schemaToChangesetTable( diffContext.schemaModified.name, diffContext.schemaModified );
      diffContext.writer.beginTable( chTable );
      diffContext.tableEntryWritten = true;
    }

    ChangesetDataEntry e;
    e.op = reverse ? ChangesetDataEntry::OpDelete : ChangesetDataEntry::OpInsert;

    size_t numColumns = diffContext.schemaModified.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Sqlite3Value v( sqlite3_column_value( statementI.get(), static_cast<int>( i ) ) );
      if ( reverse )
        e.oldValues.push_back( changesetValue( v.value() ) );
      else
        e.newValues.push_back( changesetValue( v.value() ) );
    }

    diffContext.writer.writeEntry( e );
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context, diffContext.db, "Failed to write information about inserted rows in table " + diffContext.schemaModified.name );
  }
}

static void handleUpdated( const Context *context, TableDiffContext &diffContext )
{
  std::string sqlModified = sqlFindModified( diffContext );

  Sqlite3Stmt statement;
  statement.prepare( diffContext.db, "%s", sqlModified.c_str() );
  int rc;
  while ( SQLITE_ROW == ( rc = sqlite3_step( statement.get() ) ) )
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

    ChangesetDataEntry e;
    e.op = ChangesetDataEntry::OpUpdate;

    bool hasUpdates = false;
    size_t numColumns = diffContext.schemaModified.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Sqlite3Value v1( sqlite3_column_value( statement.get(), static_cast<int>( i + numColumns ) ) );
      Sqlite3Value v2( sqlite3_column_value( statement.get(), static_cast<int>( i ) ) );
      bool pkey = diffContext.schemaModified.columns[i].isPrimaryKey;
      bool updated = ( v1 != v2 );
      if ( updated )
      {
        // Let's do a secondary check for some column types to avoid false positives, for example
        // multiple different string representations could be used for a single datetime value,
        // see "Time Values" section in https://sqlite.org/lang_datefunc.html
        // Use strftime() to take into account fractional seconds
        if ( diffContext.schemaModified.columns[i].type == TableColumnType::DATETIME )
        {
          Sqlite3Stmt stmtDatetime;
          stmtDatetime.prepare( diffContext.db, "SELECT STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', ?1) IS NOT STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', ?2)" );
          sqlite3_bind_value( stmtDatetime.get(), 1, v1.value() );
          sqlite3_bind_value( stmtDatetime.get(), 2, v2.value() );
          int res = sqlite3_step( stmtDatetime.get() );
          if ( SQLITE_ROW == res )
          {
            updated = sqlite3_column_int( stmtDatetime.get(), 0 );
          }
          else if ( SQLITE_DONE != res )
          {
            logSqliteError( context, diffContext.db, "Failed to write information about updated rows in table " + diffContext.schemaModified.name );
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
      if ( !diffContext.tableEntryWritten )
      {
        ChangesetTable chTable = schemaToChangesetTable( diffContext.schemaModified.name, diffContext.schemaModified );
        diffContext.writer.beginTable( chTable );
        diffContext.tableEntryWritten = true;
      }

      diffContext.writer.writeEntry( e );
    }
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context, diffContext.db, "Failed to write information about inserted rows in table " + diffContext.schemaModified.name );
  }
}

// To allow diff inversion to work, we first delete all rows when dropping a
// table, and NULL out all rows when dropping a column.
static void writeDataChangesForSchemaChange( std::shared_ptr<Sqlite3Db> db, const std::unordered_map<std::string, TableSchema> &currentSchemata, ChangesetWriter &writer, const ChangesetEntry &entry )
{
  if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    auto it = currentSchemata.find( dcEntry->tableName );
    if ( it == currentSchemata.end() )
      throw GeoDiffException( "Missing schema for table " + dcEntry->tableName );
    const TableSchema &table = it->second;

    std::string pkeyColStr;
    for ( const TableColumnInfo &c : table.columns )
    {
      if ( c.isPrimaryKey )
      {
        if ( !pkeyColStr.empty() )
          pkeyColStr += ", ";
        pkeyColStr += sqlitePrintf( "\"%w\"", c.name.c_str() );
      }
    }
    if ( pkeyColStr.empty() )
      throw GeoDiffException( "Table " + table.name + " has no primary key" );

    Sqlite3Stmt stmt;
    stmt.prepare( db, "SELECT %s, \"%w\" FROM \"main\".\"%w\" WHERE \"%w\" IS NOT NULL",
                  pkeyColStr.c_str(), dcEntry->column.name.c_str(), dcEntry->tableName.c_str(), dcEntry->column.name.c_str() );

    writer.beginTable( schemaToChangesetTable( table.name, table ) );
    int rc;
    while ( SQLITE_ROW == ( rc = sqlite3_step( stmt.get() ) ) )
    {
      ChangesetDataEntry e;
      e.op = ChangesetDataEntry::OpUpdate;

      size_t idxInResult = 0;
      for ( size_t i = 0; i < table.columns.size(); ++i )
      {
        bool isPkey = table.columns[i].isPrimaryKey;
        bool isDroppedCol = ( i == table.columns.size() - 1 );

        if ( isPkey || isDroppedCol )
        {
          Sqlite3Value v( sqlite3_column_value( stmt.get(), static_cast<int>( idxInResult ) ) );
          e.oldValues.push_back( changesetValue( v.value() ) );
          idxInResult++;
        }
        else
          e.oldValues.push_back( Value() );

        if ( isDroppedCol )
        {
          Value nullVal;
          nullVal.setNull();
          e.newValues.push_back( nullVal );
        }
        else
          e.newValues.push_back( Value() );
      }

      writer.writeEntry( e );
    }
  }
  else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
  {
    auto it = currentSchemata.find( dtEntry->tableName );
    if ( it == currentSchemata.end() )
      throw GeoDiffException( "Missing schema for table " + dtEntry->tableName );
    const TableSchema &table = it->second;

    Sqlite3Stmt stmt;
    stmt.prepare( db, "SELECT * FROM \"main\".\"%w\"", dtEntry->tableName.c_str() );

    writer.beginTable( schemaToChangesetTable( table.name, table ) );
    int rc;
    while ( SQLITE_ROW == ( rc = sqlite3_step( stmt.get() ) ) )
    {
      ChangesetDataEntry e;
      e.op = ChangesetDataEntry::OpDelete;

      size_t numColumns = table.columns.size();
      for ( size_t i = 0; i < numColumns; ++i )
      {
        Sqlite3Value v( sqlite3_column_value( stmt.get(), static_cast<int>( i ) ) );
        e.oldValues.push_back( changesetValue( v.value() ) );
      }

      writer.writeEntry( e );
    }
  }
}

void SqliteDriver::createChangeset( ChangesetWriter &writer )
{
  DatabaseSchema schemaBase = getSchema( false );
  DatabaseSchema schemaModified = getSchema( true );

  // We keep table schemata that have exactly the written out schema-change
  // entries applied. They're necessary to know the intermediate database state
  // for any data changes (e.g. row deletions before table drop).
  std::unordered_map<std::string, TableSchema> currentSchemata;
  for ( const TableSchema &tbl : schemaBase.tables )
    currentSchemata[tbl.name] = tbl;

  auto schemaDiffEntries = diffDatabaseSchema( schemaBase, schemaModified );
  for ( const ChangesetEntry &entry : schemaDiffEntries )
  {
    writeDataChangesForSchemaChange( mDb, currentSchemata, writer, entry );
    writer.writeEntry( entry );

    if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
      simulateColumnChange( currentSchemata[acEntry->tableName], entry );
    else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
      simulateColumnChange( currentSchemata[dcEntry->tableName], entry );
  }

  for ( const TableSchema &tblModified : schemaModified.tables )
  {
    if ( !tblModified.hasPrimaryKey() )
      continue;  // ignore tables without primary key - they can't be compared properly

    // Find corresponding table in base DB
    const TableSchema *tblBase = nullptr;
    for ( const TableSchema &tbl : schemaBase.tables )
    {
      if ( tbl.name == tblModified.name )
      {
        tblBase = &tbl;
        break;
      }
    }

    if ( !tblBase )
    {
      // Table was newly added, just dump data using INSERTs
      dumpTableData( writer, tblModified, true );
      continue;
    }

    TableDiffContext diffContext = { mDb, *tblBase, tblModified, {}, {}, writer };

    for ( const TableColumnInfo &baseColumn : tblBase->columns )
    {
      for ( const TableColumnInfo &modifiedColumn : tblModified.columns )
      {
        if ( baseColumn.name == modifiedColumn.name )
        {
          diffContext.commonColumns.push_back( modifiedColumn );
          break;
        }
      }
    }

    for ( const TableColumnInfo &modifiedColumn : tblModified.columns )
    {
      bool found = false;
      for ( const TableColumnInfo &baseColumn : tblBase->columns )
      {
        if ( baseColumn.name == modifiedColumn.name )
        {
          found = true;
          break;
        }
      }
      if ( !found )
        diffContext.newColumns.push_back( modifiedColumn );
    }

    handleInserted( context(), diffContext, false );  // INSERT
    handleInserted( context(), diffContext, true );   // DELETE
    handleUpdated( context(), diffContext );          // UPDATE
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
      sql += sqlitePrintf( " ( ?%d = 0 OR STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', \"%w\") IS STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', ?%d) ) ", i * 3 + 2, tbl.columns[i].name.c_str(), i * 3 + 1 );
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
      // compare date/time values using strftime() because otherwise
      // fractional seconds will be lost
      sql += sqlitePrintf( "STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', \"%w\") IS STRFTIME('%%Y-%%m-%%d %%H:%%M:%%f', ?)", tbl.columns[i].name.c_str() );
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
    rc = sqlite3_bind_blob( stmt, index, v.getString().c_str(), ( int ) v.getString().size(), SQLITE_TRANSIENT );
  else
    throw GeoDiffException( "unexpected bind type" );

  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "bind failed" );
  }
}


ChangeApplyResult SqliteDriver::applyDataChange( SqliteChangeApplyState &state, const ChangesetDataEntry &entry )
{
  std::string tableName = entry.table->name;

  if ( startsWith( tableName, "gpkg_" ) ) // skip any changes to GPKG meta tables
    return ChangeApplyResult::Skipped;

  if ( context()->isTableSkipped( tableName ) ) // skip table if necessary
    return ChangeApplyResult::Skipped;

  if ( state.tableState.count( entry.table.get() ) == 0 )
  {
    TableSchema schema = tableSchema( tableName );

    if ( schema.columns.size() == 0 )
      throw GeoDiffException( "No such table: " + tableName );

    if ( schema.columns.size() != entry.table->columnCount() )
      throw GeoDiffException( "Wrong number of columns for table: " + tableName );

    for ( size_t i = 0; i < entry.table->columnCount(); ++i )
    {
      if ( schema.columns[i].isPrimaryKey != entry.table->primaryKeys[i] )
        throw GeoDiffException( "Mismatch of primary keys in table: " + tableName );
    }

    SqliteChangeApplyState::TableState &tbl = state.tableState[entry.table.get()];
    tbl.schema = schema;

    tbl.stmtInsert.prepare( mDb, sqlForInsert( tableName, schema ) );
    tbl.stmtUpdate.prepare( mDb, sqlForUpdate( tableName, schema ) );
    tbl.stmtDelete.prepare( mDb, sqlForDelete( tableName, schema ) );
  }
  SqliteChangeApplyState::TableState &tbl = state.tableState[entry.table.get()];

  if ( entry.op == SQLITE_INSERT )
  {
    sqlite3_reset( tbl.stmtInsert.get() );
    for ( size_t i = 0; i < tbl.schema.columns.size(); ++i )
    {
      const Value &v = entry.newValues[i];
      bindValue( tbl.stmtInsert.get(), static_cast<int>( i ) + 1, v );
    }
    int res = sqlite3_step( tbl.stmtInsert.get() );
    if ( res == SQLITE_CONSTRAINT )
      return ChangeApplyResult::ConstraintConflict;
    else if ( res != SQLITE_DONE )
    {
      logApplyConflict( "insert_failed", entry, true );
      throw GeoDiffException( "SQLite error in INSERT" );
    }
    else if ( sqlite3_changes( mDb->get() ) != 1 )
      throw GeoDiffException( "Nothing inserted (this should never happen)" );
  }
  else if ( entry.op == SQLITE_UPDATE )
  {
    sqlite3_reset( tbl.stmtUpdate.get() );
    for ( size_t i = 0; i < tbl.schema.columns.size(); ++i )
    {
      const Value &vOld = entry.oldValues[i];
      const Value &vNew = entry.newValues[i];
      sqlite3_bind_int( tbl.stmtUpdate.get(), static_cast<int>( i ) * 3 + 2, vNew.type() != Value::TypeUndefined );
      if ( vOld.type() != Value::TypeUndefined )
        bindValue( tbl.stmtUpdate.get(), static_cast<int>( i ) * 3 + 1, vOld );
      if ( vNew.type() != Value::TypeUndefined )
        bindValue( tbl.stmtUpdate.get(), static_cast<int>( i ) * 3 + 3, vNew );
    }
    int res = sqlite3_step( tbl.stmtUpdate.get() );
    if ( res == SQLITE_CONSTRAINT )
      return ChangeApplyResult::ConstraintConflict;
    else if ( res != SQLITE_DONE )
    {
      logApplyConflict( "update_failed", entry, true );
      throw GeoDiffException( "SQLite error in UPDATE" );
    }
    else if ( sqlite3_changes( mDb->get() ) == 0 )
    {
      // either the row with such pkey does not exist or its data have been modified
      logApplyConflict( "update_nothing", entry );
      return ChangeApplyResult::NoChange;
    }
  }
  else if ( entry.op == SQLITE_DELETE )
  {
    sqlite3_reset( tbl.stmtDelete.get() );
    for ( size_t i = 0; i < tbl.schema.columns.size(); ++i )
    {
      const Value &v = entry.oldValues[i];
      bindValue( tbl.stmtDelete.get(), static_cast<int>( i ) + 1, v );
    }
    int res = sqlite3_step( tbl.stmtDelete.get() );
    if ( res == SQLITE_CONSTRAINT )
      return ChangeApplyResult::ConstraintConflict;
    else if ( res != SQLITE_DONE )
    {
      logApplyConflict( "delete_failed", entry, true );
      throw GeoDiffException( "SQLite error in DELETE" );
    }
    else if ( sqlite3_changes( mDb->get() ) == 0 )
    {
      // either the row with such pkey does not exist or its data have been modified
      logApplyConflict( "delete_nothing", entry );
      return ChangeApplyResult::NoChange;
    }
  }
  else
    throw GeoDiffException( "Unexpected operation" );

  return ChangeApplyResult::Applied;
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
  {
    throwSqliteError( db->get(), "Failed to access gpkg_spatial_ref_sys table" );
  }

  if ( sqlite3_column_int( stmtCheck.get(), 0 ) )
    return;  // already there

  if ( crs.wkt.size() == 0 )
    throw GeoDiffException( "Tried to add new CRS without WKT definition" );

  Sqlite3Stmt stmt;
  stmt.prepare( db, "INSERT INTO gpkg_spatial_ref_sys VALUES ('%q:%d', %d, '%q', %d, '%q', '')",
                crs.authName.c_str(), crs.authCode, crs.srsId, crs.authName.c_str(), crs.authCode,
                crs.wkt.c_str() );
  res = sqlite3_step( stmt.get() );
  if ( res != SQLITE_DONE )
  {
    throwSqliteError( db->get(), "Failed to insert CRS to gpkg_spatial_ref_sys table" );
  }
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
  {
    throwSqliteError( db->get(), "Failed to insert row to gpkg_contents table" );
  }

  // gpkg_geometry_columns
  //   table_name TEXT NOT NULL, column_name TEXT NOT NULL,
  //   geometry_type_name TEXT NOT NULL, srs_id INTEGER NOT NULL,
  //   z TINYINT NOT NULL,m TINYINT NOT NULL

  Sqlite3Stmt stmtGeomCol;
  stmtGeomCol.prepare( db, "INSERT INTO gpkg_geometry_columns VALUES ('%q', '%q', '%q', %d, %d, %d)",
                       tbl.name.c_str(), geomColumn.c_str(), geomType.c_str(), srsId, hasZ, hasM );
  res = sqlite3_step( stmtGeomCol.get() );
  if ( res != SQLITE_DONE )
  {
    throwSqliteError( db->get(), "Failed to insert row to gpkg_geometry_columns table" );
  }
}

static void createTable( std::shared_ptr<Sqlite3Db> db, const TableSchema &tbl )
{
  if ( tbl.geometryColumn() != SIZE_MAX )
  {
    addGpkgCrsDefinition( db, tbl.crs );
    addGpkgSpatialTable( db, tbl, Extent() );   // TODO: is it OK to set zeros?
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

  sql = sqlitePrintf( "CREATE TABLE \"%w\" (", tbl.name.c_str() );
  if ( !columns.empty() )
  {
    sql += columns;
  }
  if ( !pkeyCols.empty() )
  {
    sql += ", PRIMARY KEY (" + pkeyCols + ")";
  }
  sql += ");";

  Sqlite3Stmt stmt;
  stmt.prepare( db, sql );
  if ( sqlite3_step( stmt.get() ) != SQLITE_DONE )
  {
    throwSqliteError( db->get(), "Failure creating table: " + tbl.name );
  }
}

static void removeGpkgSpatialTable( std::shared_ptr<Sqlite3Db> db, const std::string &tableName )
{
  {
    Sqlite3Stmt stmt;
    stmt.prepare( db, "DELETE FROM gpkg_contents WHERE table_name = '%q'",
                  tableName.c_str() );
    int res = sqlite3_step( stmt.get() );
    if ( res != SQLITE_DONE )
      throwSqliteError( db->get(), "Failed to delete table from gpkg_contents table" );
  }

  {
    Sqlite3Stmt stmt;
    stmt.prepare( db, "DELETE FROM gpkg_geometry_columns WHERE table_name = '%q'",
                  tableName.c_str() );
    int res = sqlite3_step( stmt.get() );
    if ( res != SQLITE_DONE )
      throwSqliteError( db->get(), "Failed to delete table from gpkg_geometry_columns table" );
  }
}

void SqliteDriver::applySchemaChange( const ChangesetEntry &entry )
{
  if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
  {
    // TODO: Also save full CRS definition inside diff? It's pretty large and
    // we'd need it for all tables with geometry columns & geometry columns
    // themselves.
    CrsDefinition tableCrs;
    for ( const TableColumnInfo &col : ctEntry->columns )
    {
      if ( col.isGeometry )
        tableCrs.srsId = col.geomSrsId;
    }

    Sqlite3SavepointTransaction transaction( context(), mDb );
    try
    {
      createTable( mDb, { ctEntry->tableName, ctEntry->columns, tableCrs } );
    }
    catch ( const GeoDiffException & )
    {
      // TODO: Make sure this only catches sqlite errors on CREATE TABLE
      logApplyConflict( "create_table_failed", entry, true );
      throw;
    }
    transaction.commitChanges();
  }
  else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
  {
    // Check there's no data in table (zero rows)
    {
      Sqlite3Stmt stmt;
      stmt.prepare( mDb, "SELECT COUNT(*) FROM \"%w\"", dtEntry->tableName.c_str() );
      if ( sqlite3_step( stmt.get() ) != SQLITE_ROW )
        throwSqliteError( mDb->get(), "Getting row count in " + dtEntry->tableName );
      if ( sqlite3_column_int( stmt.get(), 0 ) != 0 )
      {
        logApplyConflict( "drop_table_not_empty", entry );
        throw GeoDiffException( "Tried to drop non-empty table " + dtEntry->tableName );
      }
    }

    Sqlite3Stmt stmt;
    stmt.prepare( mDb, "DROP TABLE \"%w\"", dtEntry->tableName.c_str() );
    if ( sqlite3_step( stmt.get() ) != SQLITE_DONE )
    {
      logApplyConflict( "drop_table_failed", entry, true );
      throwSqliteError( mDb->get(), "Failure deleting table: " + dtEntry->tableName );
    }
    removeGpkgSpatialTable( mDb, dtEntry->tableName );
  }
  else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
  {
    if ( acEntry->column.isGeometry )
      // Would need changing gpkg metadata
      throw GeoDiffException( "Adding geometry columns is not supported" );
    if ( acEntry->column.isPrimaryKey )
      throw GeoDiffException( "Adding column to primary key is not supported" );

    std::string sql = sqlitePrintf( "ALTER TABLE \"%w\" ADD COLUMN \"%w\" %s",
                                    acEntry->tableName.c_str(), acEntry->column.name.c_str(), acEntry->column.type.dbType.c_str() );

    if ( acEntry->column.isNotNull )
      sql += " NOT NULL";
    Sqlite3Stmt stmt;
    stmt.prepare( mDb, "%s", sql.c_str() );
    if ( sqlite3_step( stmt.get() ) != SQLITE_DONE )
    {
      logApplyConflict( "drop_column_failed", entry, true );
      throwSqliteError( mDb->get(), "Failure adding column: " + acEntry->tableName + "." + acEntry->column.name );
    }
  }
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    if ( dcEntry->column.isGeometry )
      throw GeoDiffException( "Dropping geometry columns is not supported" );
    if ( dcEntry->column.isPrimaryKey )
      throw GeoDiffException( "Dropping column from primary key is not supported" );

    // Check there's no data in the column (all NULLs)
    {
      Sqlite3Stmt stmt;
      stmt.prepare( mDb, "SELECT COUNT(*) FROM \"%w\" WHERE \"%w\" IS NOT NULL",
                    dcEntry->tableName.c_str(), dcEntry->column.name.c_str() );
      if ( sqlite3_step( stmt.get() ) != SQLITE_ROW )
        throwSqliteError( mDb->get(), "Getting row count in " + dcEntry->tableName + "." + dcEntry->column.name );
      if ( sqlite3_column_int( stmt.get(), 0 ) != 0 )
      {
        logApplyConflict( "drop_column_not_empty", entry );
        throw GeoDiffException( "Tried to drop non-empty column " + dcEntry->tableName + "." + dcEntry->column.name );
      }
    }

    Sqlite3Stmt stmt;
    stmt.prepare( mDb, "ALTER TABLE \"%w\" DROP COLUMN \"%w\"",
                  dcEntry->tableName.c_str(), dcEntry->column.name.c_str() );
    if ( sqlite3_step( stmt.get() ) != SQLITE_DONE )
    {
      logApplyConflict( "drop_column_failed", entry, true );
      throwSqliteError( mDb->get(), "Failure deleting column: " + dcEntry->tableName + "." + dcEntry->column.name );
    }
  }
  else
  {
    throw GeoDiffException( "Unhandled entry type (should have been schema change) "
                            + std::to_string( entry.index() ) );
  }
}

void SqliteDriver::applyChangeset( ChangesetReader &reader )
{
  TableSchema tbl;

  // this will acquire DB mutex and release it when the function ends (or when an exception is thrown)
  Sqlite3DbMutexLocker dbMutexLocker( mDb );

  // start transaction!
  Sqlite3SavepointTransaction savepointTransaction( context(), mDb );

  // Defer verifying foreign key constraints until end of transaction. This
  // only applies inside our transaction, so we don't need to reset it.
  Sqlite3Stmt statement;
  statement.prepare( mDb, "pragma defer_foreign_keys = 1" );
  int rc = sqlite3_step( statement.get() );
  if ( SQLITE_DONE != rc )
    logSqliteError( context(), mDb, "Failed to defer foreign key checks" );
  statement.close();

  // get all triggers sql commands
  // that we do not recognize (gpkg triggers are filtered)
  std::vector<std::string> triggerNames;
  std::vector<std::string> triggerCmds;
  sqliteTriggers( context(), mDb, triggerNames, triggerCmds );

  for ( const std::string &name : triggerNames )
  {
    statement.prepare( mDb, "drop trigger '%q'", name.c_str() );
    rc = sqlite3_step( statement.get() );
    if ( SQLITE_DONE != rc )
    {
      logSqliteError( context(), mDb, "Failed to drop trigger " + name );
    }
    statement.close();
  }

  // Applying some entries may fail due to constraints, since they require the
  // entries to be in some specific, unknown order. To work around this, we
  // retry applying the conflicting entries until either we apply them all or we
  // get stuck.
  //
  // We can only reorder data entries, not schema-changing DDL entries, so we
  // gather conflicting data entries in a list until either we run out of
  // entries or read a schema-change entry.

  int unrecoverableConflictCount = 0;
  std::vector<ChangesetDataEntry> conflictingEntries;
  ChangesetEntry entry;
  SqliteChangeApplyState state;
  while ( true )
  {
    bool haveEntry = reader.nextEntry( entry );
    if ( !haveEntry || !std::holds_alternative<ChangesetDataEntry>( entry ) )
    {
      // We can't reorder entries beyond this point (see above), retry applying
      // conflicting ones.
      std::vector<ChangesetDataEntry> newConflictingEntries;
      while ( conflictingEntries.size() > 0 )
      {
        for ( const ChangesetDataEntry &centry : conflictingEntries )
        {
          ChangeApplyResult res = applyDataChange( state, centry );
          switch ( res )
          {
            case ChangeApplyResult::Applied:
            case ChangeApplyResult::Skipped:
              break; // Applied correctly, don't put it in the new list.
            case ChangeApplyResult::ConstraintConflict:
              newConflictingEntries.push_back( centry ); // Still conflicting, keep in list.
              break;
            case ChangeApplyResult::NoChange:
              unrecoverableConflictCount++; // Other issue, will throw at the end.
              break;
          }
        }

        // If we haven't been able to apply any of the conflicting entries this
        // loop, then these conflicts can't be resolved by reordering entries.
        if ( newConflictingEntries.size() == conflictingEntries.size() )
        {
          for ( const ChangesetDataEntry &centry : conflictingEntries )
            logApplyConflict( "unresolvable_conflict", centry );
          throw GeoDiffConflictsException( "Could not resolve dependencies in constraint conflicts." );
        }
        conflictingEntries = newConflictingEntries;
        newConflictingEntries.clear();
      }
    }
    if ( !haveEntry )
      break;

    if ( const ChangesetDataEntry *dataEntry = std::get_if<ChangesetDataEntry>( &entry ) )
    {
      ChangeApplyResult res = applyDataChange( state, *dataEntry );
      switch ( res )
      {
        case ChangeApplyResult::Applied:
        case ChangeApplyResult::Skipped:
          break; // Applied correctly, continue onward.
        case ChangeApplyResult::ConstraintConflict:
          // Ordering conflict found, handle later.
          conflictingEntries.push_back( *dataEntry );
          break;
        case ChangeApplyResult::NoChange:
          unrecoverableConflictCount++; // Other issue, will throw at the end.
          break;
      }
    }
    else
    {
      applySchemaChange( entry );
    }
  }

  // recreate triggers
  for ( const std::string &cmd : triggerCmds )
  {
    statement.prepare( mDb, "%s", cmd.c_str() );
    if ( SQLITE_DONE != sqlite3_step( statement.get() ) )
    {
      logSqliteError( context(), mDb, "Failed to recreate trigger using SQL \"" + cmd + "\"" );
    }
    statement.close();
  }

  if ( !unrecoverableConflictCount )
  {
    savepointTransaction.commitChanges();
  }
  else
  {
    throw GeoDiffConflictsException( "Conflicts encountered while applying changes! Total " + std::to_string( unrecoverableConflictCount ) );
  }
}

void SqliteDriver::createTables( const std::vector<TableSchema> &tables )
{
  // currently we always create geopackage meta tables. Maybe in the future we can skip
  // that if there is a reason, and have that optional if none of the tables are spatial.

  Sqlite3Stmt stmt1;
  stmt1.prepare( mDb, "SELECT InitSpatialMetadata('main');" );
  int res = sqlite3_step( stmt1.get() );
  if ( res != SQLITE_ROW )
    throwSqliteError( mDb->get(), "Failure initializing spatial metadata" );

  for ( const TableSchema &tbl : tables )
  {
    if ( startsWith( tbl.name, "gpkg_" ) )
      continue;
    createTable( mDb, tbl );
  }
}

void SqliteDriver::dumpTableData( ChangesetWriter &writer, TableSchema tbl, bool useModified )
{
  std::string dbName = databaseName( useModified );
  if ( !tbl.hasPrimaryKey() )
    return;  // ignore tables without primary key - they can't be compared properly

  bool first = true;
  Sqlite3Stmt statementI;
  statementI.prepare( mDb, "SELECT * FROM \"%w\".\"%w\"", dbName.c_str(), tbl.name.c_str() );
  int rc;
  while ( SQLITE_ROW == ( rc = sqlite3_step( statementI.get() ) ) )
  {
    if ( first )
    {
      writer.beginTable( schemaToChangesetTable( tbl.name, tbl ) );
      first = false;
    }

    ChangesetDataEntry e;
    e.op = ChangesetDataEntry::OpInsert;
    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Sqlite3Value v( sqlite3_column_value( statementI.get(), static_cast<int>( i ) ) );
      e.newValues.push_back( changesetValue( v.value() ) );
    }
    writer.writeEntry( e );
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context(), mDb, "Failure dumping changeset" );
  }
}

void SqliteDriver::dumpData( ChangesetWriter &writer, bool useModified )
{
  std::vector<std::string> tables = listTables();
  for ( const std::string &tableName : tables )
  {
    TableSchema tbl = tableSchema( tableName, useModified );
    dumpTableData( writer, tbl, useModified );
  }
}

std::vector<std::vector<std::string>> SqliteDriver::executeSql( std::string sql )
{
  Sqlite3Stmt stmt;
  stmt.prepare( mDb, "%s", sql.c_str() );
  std::vector<std::vector<std::string>> rows;
  int rc;
  while ( ( rc = sqlite3_step( stmt.get() ) ) == SQLITE_ROW )
  {
    std::vector<std::string> values;
    values.resize( sqlite3_column_count( stmt.get() ) );
    for ( size_t i = 0; i < values.size(); ++i )
    {
      const unsigned char *text = sqlite3_column_text( stmt.get(), static_cast<int>( i ) );
      if ( text )
        values[i] = reinterpret_cast<const char *>( text );
      else
        values[i] = "<NULL>";
    }
    rows.push_back( values );
  }
  if ( rc != SQLITE_DONE )
  {
    logSqliteError( context(), mDb, "Failure executing SQL: " + sql );
  }
  return rows;
}
