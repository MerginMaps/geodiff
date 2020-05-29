/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef SQLITEDRIVER_H
#define SQLITEDRIVER_H

#include "geodiffutils.hpp"
#include "driver.h"

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

    void open( const DriverParametersMap &conn ) override;
    std::vector<std::string> listTables( bool useModified = false ) override;
    TableSchema tableSchema( const std::string &tableName, bool useModified = false ) override;
    void createChangeset( ChangesetWriter &writer ) override;
    void applyChangeset( ChangesetReader &reader ) override;

  private:
    std::shared_ptr<Sqlite3Db> mDb;
    bool mHasModified = false;  // whether there is also a second file attached
};


#endif // SQLITEDRIVER_H
