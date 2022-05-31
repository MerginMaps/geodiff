/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffcontext.hpp"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include <iostream>

Context::Context() = default;

Logger &Context::logger()
{
  return mLogger;
}

const Logger &Context::logger() const
{
  return mLogger;
}
