/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef POSTGRESDRIVER_H
#define POSTGRESDRIVER_H

#include "driver.h"

extern "C"
{
#include <libpq-fe.h>
}

class PostgresDriver : public Driver
{
  public:
    ~PostgresDriver() override;

    void open( const DriverParametersMap &conn ) override;
    void create( const DriverParametersMap &conn, bool overwrite = false ) override;
    std::vector<std::string> listTables( bool useModified = false ) override;
    TableSchema tableSchema( const std::string &tableName, bool useModified = false ) override;
    void createChangeset( ChangesetWriter &writer ) override;
    void applyChangeset( ChangesetReader &reader ) override;
    void createTables( const std::vector<TableSchema> &tables ) override;
    void dumpData( ChangesetWriter &writer, bool useModified = false ) override;

  private:
    void openPrivate( const DriverParametersMap &conn );
    void close();

    PGconn *mConn = nullptr;
    std::string mBaseSchema;
    std::string mModifiedSchema;
};

#endif // POSTGRESDRIVER_H
