/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffdrivermanager.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "geodiffsqlitedriver.hpp"

const char *GEODIFF_version()
{
  return "0.1.0";
}


static bool gInitialized = false;
void GEODIFF_init()
{
  if ( !gInitialized )
  {
    gInitialized = true;
    SqliteDriver::init();
  }
}

int GEODIFF_createChangeset( const char *base, const char *modified, const char *changeset )
{
  if ( !base || !modified || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( modified ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_createChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    DriverManager &dm = DriverManager::instance();
    for ( size_t i = 0; i < dm.driversCount(); ++i )
    {
      std::shared_ptr<Driver> driver = dm.driver( i );
      Logger::instance().info( "Trying createChangeset of driver: " + driver->name() );
      int res = driver->createChangeset( base, modified, changeset );
      if ( res != GEODIFF_NO_DRIVER_CAPABILITY )
        return res;
    }
    return GEODIFF_NO_DRIVER_CAPABILITY;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_applyChangeset( const char *base, const char *patched, const char *changeset )
{
  if ( !base || !patched || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_applyChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( changeset ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_applyChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    DriverManager &dm = DriverManager::instance();
    for ( size_t i = 0; i < dm.driversCount(); ++i )
    {
      std::shared_ptr<Driver> driver = dm.driver( i );
      Logger::instance().info( "Trying applyChangeset of driver: " + driver->name() );
      int res = driver->applyChangeset( base, patched, changeset );
      if ( res != GEODIFF_NO_DRIVER_CAPABILITY )
        return res;
    }
    return GEODIFF_NO_DRIVER_CAPABILITY;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_listChanges( const char *changeset )
{
  if ( !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_listChanges" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( changeset ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_listChanges" );
    return GEODIFF_ERROR;
  }

  try
  {
    DriverManager &dm = DriverManager::instance();
    for ( size_t i = 0; i < dm.driversCount(); ++i )
    {
      std::shared_ptr<Driver> driver = dm.driver( i );
      Logger::instance().info( "Trying listChanges of driver: " + driver->name() );
      int nchanges;
      int res = driver->listChanges( changeset, nchanges );
      if ( res != GEODIFF_NO_DRIVER_CAPABILITY )
        return nchanges;
    }
    return GEODIFF_NO_DRIVER_CAPABILITY;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return -1;
  }
}

int GEODIFF_createRebasedChangeset( const char *base, const char *modified, const char *changeset_their, const char *changeset )
{
  if ( !base || !modified || !changeset_their || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createRebasedChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( modified ) || !fileexists( changeset_their ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_createRebasedChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    DriverManager &dm = DriverManager::instance();
    for ( size_t i = 0; i < dm.driversCount(); ++i )
    {
      std::shared_ptr<Driver> driver = dm.driver( i );
      Logger::instance().info( "Trying createRebasedChangeset of driver: " + driver->name() );
      int res = driver->createRebasedChangeset( base, modified, changeset_their, changeset );
      if ( res != GEODIFF_NO_DRIVER_CAPABILITY )
        return res;
    }
    return GEODIFF_NO_DRIVER_CAPABILITY;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}
