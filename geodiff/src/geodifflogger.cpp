/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik
*/

#include "geodiff.h"
#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include <iostream>

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
  int envLevel = getEnvVarInt( "GEODIFF_LOGGER_LEVEL", 0 );
  if ( envLevel >= 0 && envLevel <= GEODIFF_LoggerLevel::LevelDebug )
  {
    setMaxLogLevel( static_cast<GEODIFF_LoggerLevel>( envLevel ) );
  }

  setCallback( &StdoutLogger );
}

void Logger::setCallback( GEODIFF_LoggerCallback loggerCallback )
{
  mLoggerCallback = loggerCallback;
}

void Logger::debug( const std::string &msg ) const
{
  log( GEODIFF_LoggerLevel::LevelDebug, msg );
}

void Logger::warn( const std::string &msg ) const
{
  log( GEODIFF_LoggerLevel::LevelWarning, msg );
}

void Logger::error( const std::string &msg ) const
{
  log( GEODIFF_LoggerLevel::LevelError, msg );
}

void Logger::error( const GeoDiffException &exp ) const
{
  log( GEODIFF_LoggerLevel::LevelError, exp.what() );
}

void Logger::info( const std::string &msg ) const
{
  log( GEODIFF_LoggerLevel::LevelInfo, msg );
}

void Logger::log( GEODIFF_LoggerLevel level, const std::string &msg ) const
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
