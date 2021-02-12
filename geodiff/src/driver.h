/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef DRIVER_H
#define DRIVER_H

#include <map>
#include <memory>
#include <vector>
#include <string>

#include "geodiff.h"
#include "tableschema.h"

class ChangesetReader;
class ChangesetWriter;

typedef std::map<std::string, std::string> DriverParametersMap;


/**
 * Abstracts all backend-specific work.
 *
 * A driver is normally opened with a reference to two data sources - the "base" ("old") source and
 * the "modified" ("new") data source. By comparing the two sources, it can create changesets
 * using createChangeset() method.
 *
 * When applying an existing changeset using applyChangeset() method, we only need one source which
 * will be modified. In this case, a driver may be opened with a single source only, but it will
 * not be possible to call createChangeset() because of missing second data source.
 *
 * Supported driver names:
 *
 * - "sqlite" - compares two sqlite database files. GeoPackages are supported as well.
 *    Use sqliteParameters() or sqliteParametersSingleSource() to get parameters to open the driver.
 * - "postgres" - TODO:add docs
 *
 * Use createDriver() to create instance of a driver.
 */
class Driver
{
  public:

    /**
     * Returns list of supported driver names
     */
    static std::vector<std::string> drivers();

    /**
     * Returns a new instance of a driver given its name. Returns nullptr if such driver does not exist.
     */
    static std::unique_ptr<Driver> createDriver( const std::string &driverName );

    /**
     * Returns driver parameters for Sqlite driver - it needs filenames of two sqlite databases.
     */
    static DriverParametersMap sqliteParameters( const std::string &filenameBase, const std::string &filenameModified );

    /**
     * Returns driver parameters for Sqlite driver, but only using a single database.
     */
    static DriverParametersMap sqliteParametersSingleSource( const std::string &filename );

    //

    Driver();
    virtual ~Driver();

    /**
     * Opens a geodiff session using a set of key-value pairs with connection configuration.
     * The expected keys and values depend on the driver being used.
     *
     * On error the function throws GeoDiffException with the cause.
     */
    virtual void open( const DriverParametersMap &conn ) = 0;

    /**
     * Opens a new geodiff session that creates data source. For example, for Sqlite this means creating
     * a new database file, for Postgres this is creation of the specified database schema (namespace).
     * \note This method only uses 'base' database ('modified' does not need to be specified)
     */
    virtual void create( const DriverParametersMap &conn, bool overwrite = false ) = 0;

    /**
     * Returns a list of tables in the current connection. The useModified argument
     * decides whether the list should be created for the base file/schema or for the locally
     * modified file/schema.
     */
    virtual std::vector<std::string> listTables( bool useModified = false ) = 0;

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
    virtual void createChangeset( ChangesetWriter &writer ) = 0;

    /**
     * Reads changes from the given reader and tries to apply them to the tables.
     * \note This method only uses 'base' database ('modified' does not need to be specified when opening)
     */
    virtual void applyChangeset( ChangesetReader &reader ) = 0;

    /**
     * Creates empty tables based on the definition given by 'tables' argument.
     * \note This method only uses 'base' database ('modified' does not need to be specified when opening)
     */
    virtual void createTables( const std::vector<TableSchema> &tables ) = 0;

    /**
     * Writes all rows of the specified table to a changeset (it will output only INSERT operations)
     */
    virtual void dumpData( ChangesetWriter &writer, bool useModified = false ) = 0;

    /**
     * Tests whether the table schemas are compatible with our rebase algorithm, i.e. no unsupported
     * database features are used. Currently, for example, geodiff rebase does not deal with foreign
     * keys or with user-defined triggers.
     *
     * If the check fails, GeoDiffException is thrown.
     */
    virtual void checkCompatibleForRebase( bool useModified = false ) = 0;

    static const std::string SQLITEDRIVERNAME;
    static const std::string POSTGRESDRIVERNAME;
};


#endif // DRIVER_H
