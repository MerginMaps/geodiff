/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffcontext.hpp"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include <iostream>
#include <sstream>

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

void Context::setTablesToSkip( std::string &tables )
{
  mTablesToSkip.clear();

  if ( tables.empty() )
  {
    return;
  }

  std::istringstream strm( tables );
  std::string s;
  while ( getline( strm, s, ';' ) )
  {
    mTablesToSkip.push_back( s );
  }
}
