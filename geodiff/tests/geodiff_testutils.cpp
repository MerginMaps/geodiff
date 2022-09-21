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
#include <locale>
#include <codecvt>

#include "changesetreader.h"
#include "changesetwriter.h"
#include "geodiffutils.hpp"

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#endif

std::string getEnvVar( std::string const &key, const std::string &defaultVal );


std::string _replace( const std::string &str, const std::string &substr, const std::string &replacestr )
{
  std::string res( str );

  while ( res.find( substr ) != std::string::npos )
  {
    res.replace( res.find( substr ), substr.size(), replacestr );
  }
  return res;
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

class ContextSingleton
{
  public:
    ContextSingleton()
    {
      mContextH = GEODIFF_createContext();
      GEODIFF_CX_setLoggerCallback( mContextH, &logger );
      GEODIFF_CX_setMaximumLoggerLevel( mContextH,  GEODIFF_LoggerLevel::LevelDebug );
    }
    ~ContextSingleton()
    {
      if ( mContextH )
      {
        GEODIFF_CX_destroy( mContextH );
        mContextH = nullptr;
      }
    }
    GEODIFF_ContextH contextH() const;

  private:
    GEODIFF_ContextH mContextH = nullptr;
};

static std::unique_ptr<ContextSingleton> sContext = nullptr;

GEODIFF_ContextH testContext()
{
  if ( !sContext )
  {
    sContext = std::unique_ptr<ContextSingleton>( new ContextSingleton );
  }
  return sContext->contextH();
}

void init_test()
{
}

void finalize_test()
{
}

bool equals( const std::string &file1, const std::string &file2, bool ignore_timestamp_change )
{
  std::string changeset = file1 + "_changeset.bin";
  if ( GEODIFF_createChangeset( testContext(), file1.c_str(), file2.c_str(), changeset.c_str() ) != GEODIFF_SUCCESS )
    return false;

  int expected_changes = 0;
  if ( ignore_timestamp_change )
    expected_changes = 1;

  if ( expected_changes == 0 )
    return ( GEODIFF_hasChanges( testContext(), changeset.c_str() ) == 0 );
  else
    return ( GEODIFF_changesCount( testContext(), changeset.c_str() )  == expected_changes );
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
#ifdef WIN32
  std::ifstream f1( stringToWString( file1 ), std::ios::binary );
  std::ifstream f2( stringToWString( file2 ), std::ios::binary );
#else
  std::ifstream f1( file1, std::ios::binary );
  std::ifstream f2( file2, std::ios::binary );
#endif
  if ( !f1.is_open() )
    return false;
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
  CreateDirectory( stringToWString( dir ).c_str(), NULL );
#else
  mkdir( dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
#endif
}

void printFileToStdout( const std::string &caption, const std::string &filepath )
{
  std::cout << std::endl << caption << " (" << filepath << ")" << std::endl;
#ifdef WIN32
  std::ifstream f( stringToWString( filepath ) );
#else
  std::ifstream f( filepath );
#endif
  if ( f.is_open() )
    std::cout << f.rdbuf();
}

void printJSON( const std::string &changeset, const std::string &json, const std::string &json_summary )
{
  // printout JSON summary
  GEODIFF_listChangesSummary( testContext(), changeset.c_str(), json_summary.c_str() );
  printFileToStdout( "JSON Summary", json_summary );

  // printout JSON
  GEODIFF_listChanges( testContext(),  changeset.c_str(), json.c_str() );
  printFileToStdout( "JSON Full", json );
}

int fileContains( const std::string &filepath, const std::string key )
{
#ifdef WIN32
  std::ifstream f( stringToWString( filepath ) );
#else
  std::ifstream f( filepath );
#endif
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

bool isFileEmpty( const std::string &filepath )
{
#ifdef WIN32
  std::ifstream f( stringToWString( filepath ) );
#else
  std::ifstream f( filepath );
#endif
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


void writeChangeset( std::string filename, const std::unordered_map<std::string, ChangesetTable> &tables,
                     const std::unordered_map<std::string, std::vector<ChangesetEntry> > &entries )
{
  ChangesetWriter w;
  try
  {
    w.open( filename );
  }
  catch ( GeoDiffException & )
  {
    assert( false );
  }
  for ( auto it = entries.begin(); it != entries.end(); ++it )
  {
    std::string tableName = it->first;
    if ( !it->second.size() )
      continue;
    w.beginTable( tables.find( tableName )->second );
    for ( const ChangesetEntry &entry : it->second )
    {
      w.writeEntry( entry );
    }
  }
}


void writeSingleTableChangeset( std::string filename, const ChangesetTable &table, std::vector<ChangesetEntry> entries )
{
  writeChangeset( filename, { std::make_pair( table.name, table ) }, { std::make_pair( table.name, entries ) } );
}


static bool testAllEntriesInOtherVector( const std::vector<ChangesetEntry> &tableEntriesA, const std::vector<ChangesetEntry> &tableEntriesB )
{
  for ( size_t i = 0; i < tableEntriesA.size(); ++i )
  {
    const ChangesetEntry &entryI = tableEntriesA[i];
    bool found = false;
    for ( size_t j = 0; j < tableEntriesB.size(); ++j )
    {
      const ChangesetEntry &entryJ = tableEntriesB[j];
      if ( entryI.op == entryJ.op && entryI.oldValues == entryJ.oldValues && entryI.newValues == entryJ.newValues )
      {
        found = true;
        break;
      }
    }
    if ( !found )
      return false;
  }
  return true;
}

//! a single changeset can be stored in different ways (e.g. different order of entries)
//! so this function tests whether they are the same
bool compareDiffsByContent( std::string diffA, std::string diffB )
{
  ChangesetReader readerA, readerB;
  if ( !readerA.open( diffA ) )
    return false;
  if ( !readerB.open( diffB ) )
    return false;

  std::unordered_map<std::string, std::vector<bool> > tablesA, tablesB;
  std::unordered_map<std::string, std::vector<ChangesetEntry> > entriesA, entriesB;
  ChangesetEntry entryA, entryB;
  while ( readerA.nextEntry( entryA ) )
  {
    if ( tablesA.find( entryA.table->name ) == tablesA.end() )
      tablesA[entryA.table->name] = entryA.table->primaryKeys;
    entriesA[entryA.table->name].push_back( entryA );
  }

  while ( readerB.nextEntry( entryB ) )
  {
    if ( tablesB.find( entryB.table->name ) == tablesB.end() )
      tablesB[entryB.table->name] = entryB.table->primaryKeys;
    entriesB[entryB.table->name].push_back( entryB );
  }

  if ( tablesA != tablesB )
    return false;

  for ( auto tableIt = tablesA.begin(); tableIt != tablesA.end(); ++tableIt )
  {
    std::string tableName = tableIt->first;
    if ( entriesA[tableName].size() != entriesB[tableName].size() )
      return false;
    if ( !testAllEntriesInOtherVector( entriesA[tableName], entriesB[tableName] ) )
      return false;
    if ( !testAllEntriesInOtherVector( entriesB[tableName], entriesA[tableName] ) )
      return false;
  }
  return true;
}

#ifdef HAVE_POSTGRES
std::string pgTestConnInfo( bool secondInstance )
{
  if ( secondInstance )
  {
    return getEnvVar( "GEODIFF_PG_CONNINFO2", "" );
  }

  return getEnvVar( "GEODIFF_PG_CONNINFO", "" );
}
#endif

GEODIFF_ContextH ContextSingleton::contextH() const
{
  return mContextH;
}
