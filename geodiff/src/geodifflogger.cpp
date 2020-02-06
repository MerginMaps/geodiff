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

void StdoutLogger( GEODIFF_LoggerLevel level, const char *msg )
{
  switch ( level )
  {
    case LevelError:
      std::cerr << "Error: " << msg << std::endl;
      break;
    case LevelWarning:
      std::cout << "Warn: " << msg << std::endl;
      break;
    case LevelDebug:
      std::cout << "Debug: " << msg << std::endl;
      break;
    default: break;
  }
}

Logger::Logger()
{
  // Sort out which
  int envLevel = _envInt( "GEODIFF_LOGGER_LEVEL" );
  if ( envLevel >= 0 && envLevel <= GEODIFF_LoggerLevel::LevelDebug )
  {
    setMaxLogLevel( static_cast<GEODIFF_LoggerLevel>( envLevel ) );
  }

  setCallback( &StdoutLogger );
}

Logger &Logger::instance()
{
  static Logger instance;
  return instance;
}

void Logger::setCallback( GEODIFF_LoggerCallback loggerCallback )
{
  mLoggerCallback = loggerCallback;
}

void Logger::debug( const std::string &msg )
{
  log( GEODIFF_LoggerLevel::LevelDebug, msg );
}

void Logger::warn( const std::string &msg )
{
  log( GEODIFF_LoggerLevel::LevelWarning, msg );
}

void Logger::error( const std::string &msg )
{
  log( GEODIFF_LoggerLevel::LevelError, msg );
}

void Logger::error( const GeoDiffException &exp )
{
  log( GEODIFF_LoggerLevel::LevelError, exp.what() );
}

void Logger::info( const std::string &msg )
{
  log( GEODIFF_LoggerLevel::LevelInfo, msg );
}

void Logger::log( GEODIFF_LoggerLevel level, const std::string &msg )
{
  if ( mLoggerCallback )
  {
    // Check out if we want to print this message
    if ( static_cast<int>( level ) <= static_cast<int>( maxLogLevel() ) )
    {
      // Send to callback
      mLoggerCallback( level, msg.c_str() );
    }
  }
}
