/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef SQLITEDRIVER_H
#define SQLITEDRIVER_H

#include <unordered_map>

#include "driver.h"
#include "sqliteutils.h"
#include "changeset.h"
#include "tableschema.h"

/**
 * Holds state that is useful to keep between entries when applying changeset.
 */
class SqliteChangeApplyState
{
  public:
    struct TableState
    {
      TableSchema schema;
      Sqlite3Stmt stmtInsert;
      Sqlite3Stmt stmtUpdate;
      Sqlite3Stmt stmtDelete;
    };

    std::unordered_map<ChangesetTable *, TableState> tableState;
};


/**
 * Support for diffs between Sqlite-based files (including GeoPackage)
 *
 * Connection configuration:
 *
 * - for a single database use (not possible to call createChangeset())
 *   - "base" = path to the database
 *
 * - for use with two databases (possible to call createChangeset())
 *   - "base" = path to the 'base' database
 *   - "modified" = path to the 'modified' database
 */
class SqliteDriver : public Driver
{
  public:
    explicit SqliteDriver( const Context *context );

    void open( const DriverParametersMap &conn ) override;
    void create( const DriverParametersMap &conn, bool overwrite = false ) override;
    std::vector<std::string> listTables( bool useModified = false ) override;
    TableSchema tableSchema( const std::string &tableName, bool useModified = false ) override;
    DatabaseSchema getSchema( bool useModified = false );
    void createChangeset( ChangesetWriter &writer ) override;
    void applyChangeset( ChangesetReader &reader ) override;
    void createTables( const std::vector<TableSchema> &tables ) override;
    void dumpData( ChangesetWriter &writer, bool useModified = false ) override;
    std::vector<std::vector<std::string>> executeSql( std::string sql ) override;

  private:
    void logApplyConflict( const std::string &type, const ChangesetEntry &entry, bool isDbErr = false ) const;
    ChangeApplyResult applyDataChange( SqliteChangeApplyState &state, const ChangesetDataEntry &entry );
    void applySchemaChange( const ChangesetEntry &entry );
    std::string databaseName( bool useModified = false );
    void dumpTableData( ChangesetWriter &writer, TableSchema tbl, bool useModified );

    std::shared_ptr<Sqlite3Db> mDb;
    bool mHasModified = false;  // whether there is also a second file attached
};


#endif // SQLITEDRIVER_H
