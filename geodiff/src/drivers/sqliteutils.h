/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik, Martin Dobias
*/

#ifndef SQLITEUTILS_H
#define SQLITEUTILS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sqlite3.h"


class Buffer;


class Sqlite3Db
{
  public:
    Sqlite3Db();
    ~Sqlite3Db();
    void open( const std::string &filename );
    //! Creates DB file (overwrites if one exists already)
    void create( const std::string &filename );
    void exec( const Buffer &buf );

    sqlite3 *get();
    void close();
  private:
    sqlite3 *mDb = nullptr;
};


class Sqlite3Stmt
{
  public:
    Sqlite3Stmt();
    ~Sqlite3Stmt();
    void prepare( std::shared_ptr<Sqlite3Db> db, const std::string &sql );
    void prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... );
    sqlite3_stmt *get();
    void close();
    //! Returns SQL statement with bound parameters expanded
    std::string expandedSql() const;
  private:
    sqlite3_stmt *db_vprepare( sqlite3 *db, const char *zFormat, va_list ap );
    sqlite3_stmt *mStmt = nullptr;
};



/**
 * Smart pointer on sqlite3_value.
 * Can be inexpensively copied (data is shared)
 */
class Sqlite3Value
{
  public:
    /**
     * Creates copy of the value
     * and takes the ownership of the new instance
     */
    Sqlite3Value( const sqlite3_value *val );
    Sqlite3Value();
    ~Sqlite3Value();

    Sqlite3Value( const Sqlite3Value & ) = delete;
    Sqlite3Value &operator=( Sqlite3Value const & ) = delete;

    //! Returns if the stored value is valid pointer
    bool isValid() const;

    //! Returns raw pointer to sqlite3 value
    sqlite3_value *value() const;

    static std::string toString( sqlite3_value *val );

  private:
    sqlite3_value *mVal = nullptr;
};


bool register_gpkg_extensions( std::shared_ptr<Sqlite3Db> db );

bool isGeoPackage( std::shared_ptr<Sqlite3Db> db );

void sqliteTriggers( std::shared_ptr<Sqlite3Db> db,
                     std::vector<std::string> &triggerNames,
                     std::vector<std::string> &triggerCmds );

typedef std::pair<std::string, int> TableColumn; //table name, column ID tree, 4(specie)
typedef std::map<TableColumn, TableColumn> ForeignKeys; // key is FK to value, e.g tree, 4(specie) -> species, 1(fid)

ForeignKeys sqliteForeignKeys( std::shared_ptr<Sqlite3Db> db, const std::string &dbName );

// TODO: remove duplicate code (SqliteDriver::listTables)
void sqliteTables( std::shared_ptr<Sqlite3Db> db,
                   const std::string &dbName,
                   std::vector<std::string> &tableNames );

// TODO: remove potentially duplicate code (SqliteDriver::tableSchema)
std::vector<std::string> sqliteColumnNames(
  std::shared_ptr<Sqlite3Db> db,
  const std::string &zDb,
  const std::string &tableName
);


#endif // SQLITEUTILS_H
