/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFUTILS_H
#define GEODIFFUTILS_H

#define GEODIFF_UNUSED(x) (void)(x)

#include <string>
#include <sstream>
#include <memory>
#include <exception>
#include <vector>
#include <map>

#include "geodiff.h"
#include "changeset.h"
#include "geodiffcontext.hpp"

class Buffer;
struct ChangesetEntry;

class GeoDiffException: public std::exception
{
  public:
    explicit GeoDiffException( const std::string &msg );
    virtual const char *what() const throw();
  private:
    std::string mMsg;
};


/**
 * Buffer for sqlite statements
 */
class Buffer
{
  public:
    Buffer();
    ~Buffer();

    bool isEmpty() const;

    /**
     * Populates buffer from BINARY file on disk (e.g changeset file)
     * Frees the existing buffer if exists
     */
    void read( const std::string &filename );

    /** Populates from stream. Takes ownership of stream */
    void read( int size, void *stream );

    /**
     * Adds formatted text to the end of a buffer
     */
    void printf( const char *zFormat, ... );

    /**
     * Writes buffer to disk
     */
    void write( const std::string &filename );

    void *v_buf() const;
    const char *c_buf() const;
    int size() const;

  private:
    void free();

    char *mZ = nullptr;  /* Stream (text or binary) */
    int mAlloc = 0;     /* Bytes allocated in mZ[] */
    int mUsed = 0;      /* Bytes actually used in mZ[] */
};


//! Ordinary std::to_string only has 6 digits of precision by default when converting doubles.
//! In some cases we need max. precision to make sure we do not loose data
std::string to_string_with_max_precision( double a_value );

/**
 * Copies file from to location. override if exists
 * Returns true on success
 * never throws
 */
bool filecopy( const std::string &to, const std::string &from );

/*
* Removes a file if exists.
*
* Returns true if file was removed or doesn't exist at all
* Never throws
*/
bool fileremove( const std::string &path );

/**
 *  whether file exists
 *  never throws
 */
bool fileexists( const std::string &path );

//! whether string starts with substring
bool startsWith( const std::string &str, const std::string &substr );

//! Returns the same string converted to lower-case
std::string lowercaseString( const std::string &str );

std::string replace( const std::string &str, const std::string &substr, const std::string &replacestr );

/**
 *  Writes std::string to file
 *  Returns true if successful
 *  never throws
 */
bool flushString( const std::string &filename, const std::string &str );

//! Joins a container of strings together using a separator
//! from https://codereview.stackexchange.com/questions/142902/simple-string-joiner-in-modern-c
template<typename InputIt> std::string join( InputIt begin, InputIt end, const std::string &separator )
{
  std::ostringstream ss;
  if ( begin != end )
    ss << *begin++;

  while ( begin != end )
  {
    ss << separator;
    ss << *begin++;
  }
  return ss.str();
}

// SOME SQL

bool isLayerTable( const std::string &tableName );

int indexOf( const std::vector<std::string> &arr, const std::string &val );

std::string concatNames( const std::vector<std::string> &names );

void get_primary_key( const ChangesetEntry &entry, int &fid, int &nColumn );


//! Returns value of an environment variable - or returns default value if it is not set
std::string getEnvVar( std::string const &key, const std::string &defaultVal );

//! Returns integer value of an environment variable - or returns default value if it is not set
int getEnvVarInt( std::string const &key, int defaultVal );

/*
* Returns temporary directory (including trailing slash)
* Returns empty string on error
* never throws
*/
std::string tmpdir();

//! Returns string given number of random alpha-numeric characters
std::string randomString( size_t length );

/*
* Returns a randomly generated filename in the temporary directory (e.g. "/tmp/geodiff_th3ix1")
* Returns empty string on error
* never throws
*/
std::string randomTmpFilename( );

/**
 *  Converts std::string to std::wstring
 *  throws GeoDiffException if conversion is not possible
 */
std::wstring stringToWString( const std::string &str );

/**
 * Converts std::wstring to std::string
 *  throws GeoDiffException if conversion is not possible
 */
std::string wstringToString( const std::wstring &wStr );

/**
 *  Opens file with specific function for windows
 *  throws GeoDiffException on error
 */
FILE *openFile( const std::string &path, const std::string &mode );

class TmpFile
{
  public:
    TmpFile( );
    explicit TmpFile( const std::string &path );
    ~TmpFile();

    std::string path() const;

    const char *c_path() const;

    void setPath( const std::string &path );
  private:
    std::string mPath;
};


class ConflictItem
{
  public:
    ConflictItem(
      int column,
      const Value &base,
      const Value &theirs,
      const Value &ours );

    Value base() const;
    Value theirs() const;
    Value ours() const;
    int column() const;

  private:
    int mColumn;
    Value mBase;
    Value mTheirs;
    Value mOurs;
};

class ConflictFeature
{
  public:
    ConflictFeature( int pk, const std::string &tableName );
    bool isValid() const;
    void addItem( const ConflictItem &item );
    std::string tableName() const;
    int pk() const;
    std::vector<ConflictItem> items() const;
  private:
    int mPk;
    std::string mTableName;
    std::vector<ConflictItem> mItems;
};


#endif // GEODIFFUTILS_H
