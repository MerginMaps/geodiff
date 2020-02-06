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
    void setCallback( GEODIFF_LoggerCallback loggerCallback );
    void setMaxLogLevel( GEODIFF_LoggerLevel level ) { mMaxLogLevel = level; }
    GEODIFF_LoggerLevel maxLogLevel() const { return mMaxLogLevel; }
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
    GEODIFF_LoggerCallback mLoggerCallback = nullptr;
    GEODIFF_LoggerLevel mMaxLogLevel = GEODIFF_LoggerLevel::LevelError;
    void log( GEODIFF_LoggerLevel level, const std::string &msg );
};

#endif // GEODIFFLOGGER_H
