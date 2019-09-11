/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#endif

GeoDiffException::GeoDiffException( const std::string &msg )
  : std::exception()
  , mMsg( msg )
{
}

const char *GeoDiffException::what() const throw()
{
  return mMsg.c_str();
}

// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////

Logger::Logger()
{
  levelFromEnv();
}

Logger &Logger::instance()
{
  static Logger instance;
  return instance;
}

Logger::LoggerLevel Logger::level() const
{
  return mLevel;
}

void Logger::debug( const std::string &msg )
{
  log( LevelDebug, msg );
}

void Logger::warn( const std::string &msg )
{
  log( LevelWarnings, msg );
}

void Logger::error( const std::string &msg )
{
  log( LevelErrors, msg );
}

void Logger::error( const GeoDiffException &exp )
{
  log( LevelErrors, exp.what() );
}

void Logger::info( const std::string &msg )
{
  log( LevelInfos, msg );
}

void Logger::log( LoggerLevel level, const std::string &msg )
{
  if ( static_cast<int>( level ) > static_cast<int>( mLevel ) )
    return;

  std::string prefix;
  switch ( level )
  {
    case LevelErrors: prefix = "Error: "; break;
    case LevelWarnings: prefix = "Warn: "; break;
    case LevelDebug: prefix = "Debug: "; break;
    default: break;
  }
  std::cout << prefix << msg << std::endl ;
}

void Logger::levelFromEnv()
{
  char *val = getenv( "GEODIFF_LOGGER_LEVEL" );
  if ( val )
  {
    int level = atoi( val );
    if ( level >= LevelNothing && level <= LevelDebug )
    {
      mLevel = ( LoggerLevel )level;
    }
  }
}

void filecopy( const std::string &to, const std::string &from )
{
  fileremove( to );

  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );

  dst << src.rdbuf();
}

void fileremove( const std::string &path )
{
  if ( fileexists( path ) )
  {
    remove( path.c_str() );
  }
}

bool fileexists( const std::string &path )
{
#ifdef WIN32
  WIN32_FIND_DATA FindFileData;
  HANDLE handle = FindFirstFile( path.c_str(), &FindFileData ) ;
  int found = handle != INVALID_HANDLE_VALUE;
  if ( found )
  {
    //FindClose(&handle); this will crash
    FindClose( handle );
  }
  return found;
#else
  // https://stackoverflow.com/a/12774387/2838364
  struct stat buffer;
  return ( stat( path.c_str(), &buffer ) == 0 );
#endif
}
