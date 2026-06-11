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

enum class TablesFilterMode
{
  Nothing,
  IncludedTables,
  SkippedTables
};

class Context
{
  public:
    Context();

    Logger &logger();
    const Logger &logger() const;

    void setTablesToSkip( const std::vector<std::string> &tablesToSkip );
    void setTablesToInclude( const std::vector<std::string> &tablesToInclude );
    bool isTableSkipped( const std::string &tableName ) const;
    void setLastError( const std::string &message );
    const std::string &lastError() const;
    TablesFilterMode tableFilterMode() const;

  private:
    Logger mLogger;
    std::vector<std::string> mTablesToSkip;
    std::vector<std::string> mTablesToInclude;
    std::string mLastError;
    TablesFilterMode mTablesFilterMode = TablesFilterMode::Nothing;
};


#endif // GEODIFFCONTEXT_H
