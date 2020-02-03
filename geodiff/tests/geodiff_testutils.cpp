/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#define xstr(a) str(a)
#define str(a) #a

#include "geodiff_testutils.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#endif

std::string _replace( const std::string &str, const std::string &substr, const std::string &replacestr )
{
  std::string res( str );

  while ( res.find( substr ) != std::string::npos )
  {
    res.replace( res.find( substr ), substr.size(), replacestr );
  }
  return res;
}

std::string _getEnvVar( std::string const &key, const std::string &defaultVal )
{
  char *val = getenv( key.c_str() );
  return val == nullptr ? defaultVal : std::string( val );
}

std::string pathjoin( const std::string &dir, const std::string &filename )
{
  std::string res = dir + "/" + filename;
  res = _replace( res, "//", "/" );
  res = _replace( res, "\\/", "/" );
  res = _replace( res, "\\\\", "/" );
  res = _replace( res, "\\", "/" );
  return res;
}

std::string pathjoin( const std::string &dir, const std::string &dir2, const std::string &filename )
{
  std::string res = pathjoin( dir, dir2 );
  res = pathjoin( res, filename );
  return res;
}

void filecopy( const std::string &to, const std::string &from )
{
  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );
  dst << src.rdbuf();
}

std::string testdir()
{
  return TEST_DATA_DIR;
}

std::string tmpdir()
{
#ifdef WIN32
  ;
  TCHAR lpTempPathBuffer[MAX_PATH];

  DWORD dwRetVal = GetTempPath( MAX_PATH, lpTempPathBuffer );
  if ( dwRetVal > MAX_PATH || ( dwRetVal == 0 ) )
  {
    return std::string( "C:/temp/" );
  }
  return std::string( lpTempPathBuffer );
#else
  return _getEnvVar( "TMPDIR", "/tmp/" );
#endif
}

std::string test_file( std::string basename )
{
  std::string path( testdir() );
  path += basename;
  return path;
}

std::string tmp_file( std::string basename )
{
  std::string path( tmpdir() );
  path += basename;
  return path;
}

static void logger( LoggerLevel level, const char *msg )
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

void init_test()
{
  GEODIFF_init( &logger, true );
}

void finalize_test()
{
}

bool equals( const std::string &file1, const std::string &file2, bool ignore_timestamp_change )
{
  std::string changeset = file1 + "_changeset.bin";
  if ( GEODIFF_createChangeset( file1.c_str(), file2.c_str(), changeset.c_str() ) != GEODIFF_SUCCESS )
    return false;

  int expected_changes = 0;
  if ( ignore_timestamp_change )
    expected_changes = 1;

  if ( expected_changes == 0 )
    return ( GEODIFF_hasChanges( changeset.c_str() ) == 0 );
  else
    return ( GEODIFF_changesCount( changeset.c_str() )  == expected_changes );
}

void makedir( const std::string &dir )
{
#ifdef WIN32
  CreateDirectory( dir.c_str(), NULL );
#else
  mkdir( dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
#endif
}

void printJSON( const std::string &changeset, const std::string &json, const std::string &json_summary )
{
  // printout JSON summary
  std::cout << "JSON Summary " << std::endl;
  GEODIFF_listChangesSummary( changeset.c_str(), json_summary.c_str() );
  std::ifstream f2( json_summary );
  if ( f2.is_open() )
    std::cout << f2.rdbuf();

  // printout JSON
  std::cout << std::endl << "JSON Full " << std::endl;
  GEODIFF_listChanges( changeset.c_str(), json.c_str() );
  std::ifstream f( json );
  if ( f.is_open() )
    std::cout << f.rdbuf();

}
