/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFUTILS_H
#define GEODIFFUTILS_H

#include <string>
#include <memory>
#include <exception>
#include <vector>
#include <map>

#include "geodiff.h"
#include "changeset.h"

class Buffer;
struct ChangesetEntry;

class GeoDiffException: public std::exception
{
  public:
    GeoDiffException( const std::string &msg );
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



std::string pOpToStr( int pOp );
std::string conflict2Str( int c );

//! copy file from to location. override if exists
void filecopy( const std::string &to, const std::string &from );

//! remove a file if exists
void fileremove( const std::string &path );

//! whether file exists
bool fileexists( const std::string &path );

//! whether string starts with substring
bool startsWith( const std::string &str, const std::string &substr );

std::string replace( const std::string &str, const std::string &substr, const std::string &replacestr );

//! writes std::string to file
void flushString( const std::string &filename, const std::string &str );

// SOME SQL

bool isLayerTable( const std::string &tableName );


int indexOf( const std::vector<std::string> &arr, const std::string &val );

std::string concatNames( const std::vector<std::string> &names );


void get_primary_key( const ChangesetEntry &entry, int &fid, int &nColumn );



//! Returns value of an environment variable - or returns default value if it is not set
std::string getEnvVar( std::string const &key, const std::string &defaultVal );

//! Returns temporary directory (including trailing slash)
std::string tmpdir();


class TmpFile
{
  public:
    TmpFile( const std::string &path );
    ~TmpFile();

    std::string path() const;

    const char *c_path() const;
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
    ConflictFeature( int pk,
                     const std::string &tableName );
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
