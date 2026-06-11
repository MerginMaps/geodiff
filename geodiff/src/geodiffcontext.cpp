/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffcontext.hpp"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include <iostream>
#include <functional>
#include <algorithm>

Context::Context() = default;

Logger &Context::logger()
{
  return mLogger;
}

const Logger &Context::logger() const
{
  return mLogger;
}

void Context::setTablesToSkip( const std::vector<std::string> &tablesToSkip )
{
  if ( mTablesFilterMode == TablesFilterMode::IncludedTables )
    throw GeoDiffException( "Cannot set tables to skip when tables to include are already set" );

  mTablesFilterMode = TablesFilterMode::SkippedTables;
  mTablesToSkip = tablesToSkip;
}

void Context::setTablesToInclude( const std::vector<std::string> &tablesToInclude )
{
  if ( mTablesFilterMode == TablesFilterMode::SkippedTables )
    throw GeoDiffException( "Cannot set tables to include when tables to skip are already set" );

  mTablesFilterMode = TablesFilterMode::IncludedTables;
  mTablesToInclude = tablesToInclude;
}

bool Context::isTableSkipped( const std::string &tableName ) const
{
  if ( mTablesFilterMode == TablesFilterMode::IncludedTables )
  {
    return std::none_of( mTablesToInclude.begin(), mTablesToInclude.end(), std::bind( std::equal_to<std::string>(), std::placeholders::_1, tableName ) );
  }

  if ( mTablesFilterMode == TablesFilterMode::SkippedTables )
  {
    return std::any_of( mTablesToSkip.begin(), mTablesToSkip.end(), std::bind( std::equal_to<std::string>(), std::placeholders::_1, tableName ) );
  }

  return false;
}

void Context::setLastError( const std::string &message )
{
  mLastError = message;
}

const std::string &Context::lastError() const
{
  return mLastError;
}

TablesFilterMode Context::tableFilterMode() const
{
  return mTablesFilterMode;
}
