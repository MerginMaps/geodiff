/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#ifndef GEODIFFLOGGER_H
#define GEODIFFLOGGER_H

#include <string>
#include <memory>
#include <vector>
#include <stdio.h>

#include "geodiff.h"
#include "geodiffutils.hpp"

class Logger
{
  public:
    static Logger &instance();
    void setCallback( LoggerCallback loggerCallback );
    void setMaxLogLevel( LoggerLevel level ) {mMaxLogLevel = level;}
    LoggerLevel maxLogLevel() const { return mMaxLogLevel; }
    Logger( Logger const & ) = delete;
    void operator=( Logger const & ) = delete;
    void debug( const std::string &msg );
    void warn( const std::string &msg );
    void error( const std::string &msg );
    void info( const std::string &msg );
    //! Prints error message
    void error( const GeoDiffException &exp );
  private:
    Logger();
    LoggerCallback mLoggerCallback = nullptr;
    LoggerLevel mMaxLogLevel = LoggerLevel::LevelError;
    void log( LoggerLevel level, const std::string &msg );
};

#endif // GEODIFFLOGGER_H
