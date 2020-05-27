/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef GEODIFFDRIVER_H
#define GEODIFFDRIVER_H

#include <map>
#include <vector>
#include <string>

#include "geodiff.h"

/** Information about a single column of a database table */
struct TableColumnInfo
{
  //! Unique name of the column
  std::string name;
  //! Type of the column as reported by the database
  std::string type;
  //! Whether this column is a part of the table's primary key
  bool isPrimaryKey;

  bool operator==( const TableColumnInfo &other ) const
  {
    return name == other.name && type == other.type && isPrimaryKey == other.isPrimaryKey;
  }
  bool operator!=( const TableColumnInfo &other ) const
  {
    return !( *this == other );
  }
};

/** Information about table schema of a database table */
struct TableSchema
{
  std::vector<TableColumnInfo> columns;

  bool operator==( const TableSchema &other ) const
  {
    return columns == other.columns;
  }
  bool operator!=( const TableSchema &other ) const
  {
    return !( *this == other );
  }
};

class GeoDiffChangesetReader;
class GeoDiffChangesetWriter;


/**
 * Abstracts all backend-specific work
 */
class GEODIFF_EXPORT Driver
{
  public:

    virtual ~Driver() = default;

    // TODO: have some filter on what tables to consider

    /**
     * Opens a geodiff session using a set of key-value pairs with connection configuration.
     * The expected keys and values depend on the driver being used.
     *
     * On error the function throws GeoDiffException with the cause.
     */
    virtual void open( const std::map<std::string, std::string> &conn ) = 0;

    /**
     * Returns a list of tables in the current connection. The useModified argument
     * decides whether the list should be created for the base file/schema or for the locally
     * modified file/schema.
     */
    virtual void listTables( std::vector<std::string> &tableNames, bool useModified = false ) = 0;

    /**
     * Returns table schema information for a given table. This is used to check compatibility
     * between different tables.
     */
    virtual TableSchema tableSchema( const std::string &tableName, bool useModified = false ) = 0;

    /**
     * Writes changes between base and modified tables to the given writer
     * \note This method requires that both 'base' and 'modified' databases have been specified
     *       when opening the driver.
     */
    virtual void createChangeset( GeoDiffChangesetWriter &writer ) = 0;

    /**
     * Reads changes from the given reader and tries to apply them to the tables.
     * \note This method only uses 'base' database ('modified' does not need to be specified when opening)
     */
    virtual void applyChangeset( GeoDiffChangesetReader &reader ) = 0;
};


#if 0
/** Handles geodiff operations on tables within a single PostgreSQL database */
class PostgresDriver : public GeoDiffDriver
{
    // TODO: initialize driver with connection string: host, port, db, schema_base, schema_modified

};
#endif

#endif // GEODIFFDRIVER_H
