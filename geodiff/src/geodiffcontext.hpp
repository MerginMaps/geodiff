/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#ifndef GEODIFFCONTEXT_H
#define GEODIFFCONTEXT_H

#include <string>
#include <vector>

#include "geodiff.h"
#include "geodifflogger.hpp"

class Context
{
  public:
    Context();

    Logger &logger();
    const Logger &logger() const;

    void setTablesToSkip( const std::vector<std::string> &tablesToSkip );
    bool isTableSkipped( const std::string &tableName ) const;
    void setLastError( const std::string &message );
    const std::string &lastError() const;

  private:
    Logger mLogger;
    std::vector<std::string> mTablesToSkip;
    std::string mLastError;
};


#endif // GEODIFFCONTEXT_H
