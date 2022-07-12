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

const std::vector<std::string> &Context::tablesToSkip() const
{
  return mTablesToSkip;
}

void Context::setTablesToSkip( const std::vector<std::string> &tablesToSkip )
{
  mTablesToSkip = tablesToSkip;
}

bool Context::isTableSkipped( const std::string &tableName ) const
{
  if ( mTablesToSkip.empty() )
  {
    return false;
  }

  return std::any_of( mTablesToSkip.begin(), mTablesToSkip.end(), std::bind( std::equal_to< std::string >(), std::placeholders::_1, tableName ) );
}
