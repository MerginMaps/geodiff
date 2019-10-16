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

#include "sqlite3.h"

class Buffer;

class GeoDiffException: public std::exception
{
  public:
    GeoDiffException( const std::string &msg );
    virtual const char *what() const throw();
  private:
    std::string mMsg;
};

/**
 * Logger
 *
 * the messages printed to stdout can be controlled by
 * environment variable GEODIFF_LOGGER_LEVEL
 * GEODIFF_LOGGER_LEVEL = 0 nothing is printed
 * GEODIFF_LOGGER_LEVEL = 1 errors are printed
 * GEODIFF_LOGGER_LEVEL = 2 errors and warnings are printed
 * GEODIFF_LOGGER_LEVEL = 3 errors, warnings and infos are printed
 * GEODIFF_LOGGER_LEVEL = 4 errors, warnings, infos, debug messages are printed
 */
class Logger
{
  public:
    enum LoggerLevel
    {
      LevelNothing = 0,
      LevelErrors = 1,
      LevelWarnings = 2,
      LevelInfos = 3,
      LevelDebug = 4
    };

    static Logger &instance();
    LoggerLevel level() const;
    Logger( Logger const & ) = delete;
    void operator=( Logger const & ) = delete;
    void debug( const std::string &msg );
    void warn( const std::string &msg );
    void error( const std::string &msg );
    void info( const std::string &msg );
    //! Prints error message
    void error( const GeoDiffException &exp );
  private:
    Logger();
    void log( LoggerLevel level, const std::string &msg );
    void levelFromEnv();

    LoggerLevel mLevel = LevelErrors; //by default record errors
};

class Sqlite3Db
{
  public:
    Sqlite3Db();
    ~Sqlite3Db();
    void open( const std::string &filename );
    void exec( const Buffer &buf );

    sqlite3 *get();
    void close();
  private:
    sqlite3 *mDb = nullptr;
};

class Sqlite3Session
{
  public:
    Sqlite3Session();
    ~Sqlite3Session();
    void create( std::shared_ptr<Sqlite3Db> db, const std::string &name );
    sqlite3_session *get() const;
    void close();
  private:
    sqlite3_session *mSession = nullptr;
};

class Sqlite3Stmt
{
  public:
    Sqlite3Stmt();
    ~Sqlite3Stmt();
    void prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... );
    sqlite3_stmt *get();
    void close();
  private:
    sqlite3_stmt *db_vprepare( sqlite3 *db, const char *zFormat, va_list ap );
    sqlite3_stmt *mStmt = nullptr;
};

class Sqlite3ChangesetIter
{
  public:
    Sqlite3ChangesetIter();
    ~Sqlite3ChangesetIter();
    void start( const Buffer &buf );
    sqlite3_changeset_iter *get();
    void close();
    //! do not delete, you are not owner
    void oldValue( int i, sqlite3_value **val );
    //! do not delete, you are not owner
    void newValue( int i, sqlite3_value **val );

    static std::string toString( sqlite3_changeset_iter *pp );

    std::string toJSON( std::shared_ptr<Sqlite3Db> db, Sqlite3ChangesetIter &pp );
  private:
    sqlite3_changeset_iter *mChangesetIter = nullptr;
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

    /**
     * Populates buffer from sqlite3 session
     * Frees the existing buffer if exists
     */
    void read( const Sqlite3Session &session );

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

/**
 * Smart pointer on sqlite3_value.
 * Can be inexpensively copied (data is shared)
 */
class Sqlite3Value
{
  public:
    /**
     * Creates copy of the value
     * and takes the ownership of the new instance
     */
    Sqlite3Value( const sqlite3_value *val );
    Sqlite3Value();
    ~Sqlite3Value();

    Sqlite3Value( const Sqlite3Value & ) = delete;
    Sqlite3Value &operator=( Sqlite3Value const & ) = delete;

    //! Returns if the stored value is valid pointer
    bool isValid() const;

    //! Returns raw pointer to sqlite3 value
    sqlite3_value *value() const;

    static std::string toString( sqlite3_value *val );

  private:
    sqlite3_value *mVal = nullptr;
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

// WKT geometry
std::string convertGeometryToWKT(
  std::shared_ptr<Sqlite3Db> db,
  sqlite3_value *wkb
);

void triggers( std::shared_ptr<Sqlite3Db> db,
               std::vector<std::string> &triggerNames,
               std::vector<std::string> &triggerCmds );

void tables( std::shared_ptr<Sqlite3Db> db,
             const std::string &dbName,
             std::vector<std::string> &tableNames );

std::vector<std::string> columnNames(
  std::shared_ptr<Sqlite3Db> db,
  const std::string &zDb,
  const std::string &tableName
);

bool has_same_table_schema( std::shared_ptr<Sqlite3Db> db,
                            const std::string &tableName,
                            std::string &errStr );

void get_primary_key( Sqlite3ChangesetIter &pp, int pOp, int &fid, int &nColumn );

bool register_gpkg_extensions( std::shared_ptr<Sqlite3Db> db );

bool isGeoPackage( std::shared_ptr<Sqlite3Db> db );

// WRITE CHANGESET API

class BinaryStream
{
  public:
    BinaryStream( const std::string &path, bool temporary );
    ~BinaryStream();
    void open();
    bool isValid();

    // returns true on error
    bool appendTo( FILE *stream );

    /*
    ** Write an SQLite value onto out.
    */
    void putValue( sqlite3_value *ppValue );
    void putValue( int ppValue );

    /*
    ** Write a 64-bit signed integer as a varint onto out
    ** Copied from sqldiff.c
    */
    void putsVarint( sqlite3_uint64 v );

    // see stdio.h::putc
    int put( int v );

    // see stdio.h::fwrite
    size_t write( const void *ptr, size_t size, size_t nitems );

  private:
    void close();
    void remove();
    std::string mPath;
    bool mIsTemporary;
    FILE *mBuffer;
};




#endif // GEODIFFUTILS_H
