/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#define xstr(a) str(a)
#define str(a) #a

#include "geodiff_testutils.hpp"
#include <vector>
#include <math.h>
#include <assert.h>
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#endif

std::string _getEnvVar( std::string const &key, const std::string &defaultVal )
{
  char *val = getenv( key.c_str() );
  return val == nullptr ? defaultVal : std::string( val );
}

std::string pathjoin( const std::string &dir, const std::string &filename )
{
#ifdef WIN32
  std::string separator( "\\" );
#else
  std::string separator( "/" );
#endif
  return dir + separator + filename;
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

void init_test()
{
  GEODIFF_init();
}

void finalize_test()
{
}
