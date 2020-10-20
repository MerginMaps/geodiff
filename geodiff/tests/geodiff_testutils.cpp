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
#include <locale>
#include <codecvt>

#ifdef WIN32
#define UNICODE
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
#ifdef WIN32
    std::wstring _from = stringToWString( from );
    std::wstring _to = stringToWString( to );
    CopyFile( _from.c_str(), _to.c_str(), false );
#else
  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );
  dst << src.rdbuf();
#endif
}

std::string testdir()
{
  return TEST_DATA_DIR;
}

std::string tmpdir()
{
#ifdef WIN32
  wchar_t arr[MAX_PATH];
  DWORD dwRetVal = GetTempPathW( MAX_PATH, arr );

  std::wstring tempDirPath( arr );

  if ( dwRetVal > MAX_PATH || ( dwRetVal == 0 ) )
  {
    return std::string( "C:/temp/" );
  }
  
  return wstringToString( tempDirPath );
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


std::wstring stringToWString( const std::string &str )
{
  // we need to convert UTF-8 string to UTF-16 in order to use WindowsAPI
  // https://stackoverflow.com/questions/2573834/c-convert-string-or-char-to-wstring-or-wchar-t
  try 
  {
    std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
    std::wstring wStr = converter.from_bytes( str );
  
    return wStr;
  }
  catch ( const std::range_error & )
  {
    return std::wstring();
  }
}

std::string wstringToString( const std::wstring& wStr )
{
    // we need to convert UTF-16 string to UTF-8 in order to use WindowsAPI
    // https://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
    try
    {
        std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
        std::string str = converter.to_bytes( wStr );

        return str;
    }
    catch ( const std::range_error& )
    {
        return std::string();
    }
}

void makedir( const std::string &dir )
{
#ifdef WIN32
  CreateDirectory( stringToWString( dir ).c_str(), NULL );
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

bool containsConflict( const std::string &conflictFile, const std::string key )
{
  return fileContains( conflictFile, key ) > 0;
}


int countConflicts( const std::string &conflictFile )
{
  return fileContains( conflictFile, "fid" );
}

