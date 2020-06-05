/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

//#define xstr(a) str(a)
//#define str(a) #a

#include "geodiff_testutils.hpp"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <memory.h>
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

static void logger( GEODIFF_LoggerLevel level, const char *msg )
{
  std::string prefix;
  switch ( level )
  {
    case LevelError: prefix = "err: "; break;
    case LevelWarning: prefix = "wrn: "; break;
    case LevelDebug: prefix = "dbg: "; break;
    default: break;
  }
  std::cout << prefix << msg << std::endl ;
}

void init_test()
{
  GEODIFF_init();
  GEODIFF_setLoggerCallback( &logger );
  GEODIFF_setMaximumLoggerLevel( GEODIFF_LoggerLevel::LevelDebug );
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

static long file_size( std::ifstream &is )
{
  // get length of file:
  is.seekg( 0, is.end );
  long length = is.tellg();
  is.seekg( 0, is.beg );
  return length;
}

bool fileContentEquals( const std::string &file1, const std::string &file2 )
{
  std::ifstream f1( file1, std::ios::binary );
  if ( !f1.is_open() )
    return false;
  std::ifstream f2( file2, std::ios::binary );
  if ( !f2.is_open() )
    return false;

  long size1 = file_size( f1 );
  long size2 = file_size( f2 );
  if ( size1 != size2 )
    return false;

  std::string content1( ( std::istreambuf_iterator<char>( f1 ) ),
                        ( std::istreambuf_iterator<char>() ) );
  std::string content2( ( std::istreambuf_iterator<char>( f2 ) ),
                        ( std::istreambuf_iterator<char>() ) );
  return memcmp( content1.data(), content2.data(), size1 ) == 0;
}

void makedir( const std::string &dir )
{
#ifdef WIN32
  CreateDirectory( dir.c_str(), NULL );
#else
  mkdir( dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
#endif
}

void printFileToStdout( const std::string &caption, const std::string &filepath )
{
  std::cout << std::endl << caption << " (" << filepath << ")" << std::endl;
  std::ifstream f( filepath );
  if ( f.is_open() )
    std::cout << f.rdbuf();
}

void printJSON( const std::string &changeset, const std::string &json, const std::string &json_summary )
{
  // printout JSON summary
  GEODIFF_listChangesSummary( changeset.c_str(), json_summary.c_str() );
  printFileToStdout( "JSON Summary", json_summary );

  // printout JSON
  GEODIFF_listChanges( changeset.c_str(), json.c_str() );
  printFileToStdout( "JSON Full", json );
}

int fileContains( const std::string &filepath, const std::string key )
{
  std::ifstream f( filepath );
  if ( f.is_open() )
  {
    std::ostringstream datastream;
    datastream << f.rdbuf() << '\n';
    std::string strdata( datastream.str() );
    int occurences = 0;
    for ( size_t found( -1 ); ( found = strdata.find( key, found + 1 ) ) != std::string::npos; ++occurences );
    return occurences;
  }
  else
  {
    // file does not exist or is not readable
    return 0;
  }
}


bool fileExists( const std::string &filepath )
{
  struct stat buffer;
  return ( stat( filepath.c_str(), &buffer ) == 0 );
}

bool isFileEmpty( const std::string &filepath )
{
  std::ifstream f( filepath );
  if ( !f.is_open() )
    return false;
  return file_size( f ) == 0;
}

bool containsConflict( const std::string &conflictFile, const std::string key )
{
  return fileContains( conflictFile, key ) > 0;
}


int countConflicts( const std::string &conflictFile )
{
  return fileContains( conflictFile, "fid" );
}

str::string pgTestConnInfo()
{
  return _getEnvVar("GEODIFF_PG_CONNINFO", "");
}
