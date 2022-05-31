/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#ifndef GEODIFFCONTEXT_H
#define GEODIFFCONTEXT_H

#include "geodiff.h"
#include "geodifflogger.hpp"

class Context
{
  public:
    Context();

    Logger &logger();
    const Logger &logger() const;
  private:
    Logger mLogger;
};


#endif // GEODIFFCONTEXT_H
