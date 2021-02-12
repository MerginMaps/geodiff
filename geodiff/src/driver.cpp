/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "driver.h"

#include "sqlitedriver.h"

#include "geodiff_config.hpp"

#ifdef HAVE_POSTGRES
#include "postgresdriver.h"
#endif

// define driver names
const std::string Driver::SQLITEDRIVERNAME = "sqlite";
const std::string Driver::POSTGRESDRIVERNAME = "postgres";

Driver::Driver()
{
}

Driver::~Driver()
{
}

std::vector<std::string> Driver::drivers()
{
  std::vector<std::string> names;
  names.push_back( SQLITEDRIVERNAME );
#ifdef HAVE_POSTGRES
  names.push_back( POSTGRESDRIVERNAME );
#endif
  return names;
}

std::unique_ptr<Driver> Driver::createDriver( const std::string &driverName )
{
  if ( driverName == SQLITEDRIVERNAME )
  {
    return std::unique_ptr<Driver>( new SqliteDriver );
  }
#ifdef HAVE_POSTGRES
  if ( driverName == POSTGRESDRIVERNAME )
  {
    return std::unique_ptr<Driver>( new PostgresDriver );
  }
#endif
  return std::unique_ptr<Driver>();
}

DriverParametersMap Driver::sqliteParameters( const std::string &filenameBase, const std::string &filenameModified )
{
  DriverParametersMap conn;
  conn["base"] = filenameBase;
  conn["modified"] = filenameModified;
  return conn;
}

DriverParametersMap Driver::sqliteParametersSingleSource( const std::string &filename )
{
  DriverParametersMap conn;
  conn["base"] = filename;
  return conn;
}
