/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#define xstr(a) str(a)
#define str(a) #a

#include "geodiff_testutils.hpp"
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
  setenv( "GEODIFF_LOGGER_LEVEL", "4", 1 );
  GEODIFF_init();
}

void finalize_test()
{
}

bool equals(const std::string& file1, const std::string& file2)
{
  std::string changeset = file1 + "_changeset.bin";
  if (GEODIFF_createChangeset( file1.c_str(), file2.c_str(), changeset.c_str() ) != GEODIFF_SUCCESS)
    return false;

  return (GEODIFF_listChanges( changeset.c_str() )  == 0 );
}
