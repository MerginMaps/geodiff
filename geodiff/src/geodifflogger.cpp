/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#include "geodiff.h"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include <iostream>

int _envInt( const char *key )
{
  char *val = getenv( key );
  if ( val )
  {
    return atoi( val );
  }
  return 0;
}

void StdoutLogger( LoggerLevel level, const char *msg )
{
  std::string prefix;
  switch ( level )
  {
    case LevelError: prefix = "Error: "; break;
    case LevelWarning: prefix = "Warn: "; break;
    case LevelDebug: prefix = "Debug: "; break;
    default: break;
  }
  std::cout << prefix << msg << std::endl ;
}

Logger::Logger()
{
  // Sort out which
  int envLevel = _envInt( "GEODIFF_LOGGER_LEVEL" );
  if ( envLevel > 0 && envLevel <= LoggerLevel::LevelDebug )
  {
    setMaxLogLevel( static_cast<LoggerLevel>( envLevel ) );
  }

  setCallback( &StdoutLogger );
}

Logger &Logger::instance()
{
  static Logger instance;
  return instance;
}

void Logger::setCallback( LoggerCallback loggerCallback )
{
  mLoggerCallback = loggerCallback;
}

void Logger::debug( const std::string &msg )
{
  log( LoggerLevel::LevelDebug, msg );
}

void Logger::warn( const std::string &msg )
{
  log( LoggerLevel::LevelWarning, msg );
}

void Logger::error( const std::string &msg )
{
  log( LoggerLevel::LevelError, msg );
}

void Logger::error( const GeoDiffException &exp )
{
  log( LoggerLevel::LevelError, exp.what() );
}

void Logger::info( const std::string &msg )
{
  log( LoggerLevel::LevelInfo, msg );
}

void Logger::log( LoggerLevel level, const std::string &msg )
{
  if ( mLoggerCallback )
  {
    // Check out if we want to print this message
    if ( static_cast<int>( level ) > static_cast<int>( maxLogLevel() ) )
      return;

    // Send to callback
    mLoggerCallback( level, msg.c_str() );
  }
}
