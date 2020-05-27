/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef SQLITEDRIVER_H
#define SQLITEDRIVER_H

#include "geodiffutils.hpp"
#include "geodiffdriver.h"

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
class GEODIFF_EXPORT SqliteDriver : public Driver
{
  public:

    void open( const std::map<std::string, std::string> &conn ) override;
    void listTables( std::vector<std::string> &tableNames, bool useModified = false ) override;
    TableSchema tableSchema( const std::string &tableName, bool useModified = false ) override;
    void createChangeset( GeoDiffChangesetWriter &writer ) override;
    void applyChangeset( GeoDiffChangesetReader &reader ) override;

  private:
    std::shared_ptr<Sqlite3Db> db;
    bool mHasModified = false;  // whether there is also a second file attached
};


#endif // SQLITEDRIVER_H
