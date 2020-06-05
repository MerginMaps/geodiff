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

// TODO: add docs!
class PostgresDriver : public Driver
{
  public:
    ~PostgresDriver();

    void open( const DriverParametersMap &conn ) override;
    std::vector<std::string> listTables( bool useModified = false ) override;
    TableSchema tableSchema( const std::string &tableName, bool useModified = false ) override;
    void createChangeset( ChangesetWriter &writer ) override;
    void applyChangeset( ChangesetReader &reader ) override;

  private:
    PGconn *mConn = nullptr;
    std::string mBaseSchema;
    std::string mModifiedSchema;
};

#endif // POSTGRESDRIVER_H
