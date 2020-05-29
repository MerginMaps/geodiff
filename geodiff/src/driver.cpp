/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "driver.h"

#include "sqlitedriver.h"


Driver::Driver() = default;

Driver::~Driver() = default;

std::vector<std::string> Driver::drivers()
{
  std::vector<std::string> names;
  names.push_back( "sqlite" );
  return names;
}

Driver *Driver::createDriver(const std::string &driverName)
{
  if ( driverName == "sqlite" )
  {
    return new SqliteDriver;
  }
  return nullptr;
}

DriverParametersMap Driver::sqliteParameters(const std::string &filenameBase, const std::string &filenameModified)
{
  DriverParametersMap conn;
  conn["base"] = filenameBase;
  conn["modified"] = filenameModified;
  return conn;
}

DriverParametersMap Driver::sqliteParametersSingleSource(const std::string &filename)
{
  DriverParametersMap conn;
  conn["base"] = filename;
  return conn;
}
