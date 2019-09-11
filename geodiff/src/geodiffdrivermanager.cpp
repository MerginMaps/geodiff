/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffdrivermanager.hpp"

#include "drivers/geopackage/geodiffgeopackagedriver.hpp"
#include "drivers/sqlite/geodiffsqlitedriver.hpp"

size_t DriverManager::driversCount() const
{
  return mDrivers.size();
}

std::shared_ptr<Driver> DriverManager::driver( const std::string &name ) const
{
  for ( const auto &driver : mDrivers )
  {
    if ( driver->name() == name )
      return driver;
  }
  return std::shared_ptr<Driver>();
}

std::shared_ptr<Driver> DriverManager::driver( size_t index ) const
{
  if ( index < mDrivers.size() )
    return mDrivers[index];
  else
    return std::shared_ptr<Driver>();
}

DriverManager::DriverManager()
{
  mDrivers.push_back( std::make_shared<GeopackageDriver>() );
  mDrivers.push_back( std::make_shared<SqliteDriver>() );
}

