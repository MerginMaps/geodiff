/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFUTILS_H
#define GEODIFFUTILS_H

#include <string>
#include <exception>
#include "sqlite3.h"

class Sqlite3Db
{
  public:

};

class GeoDiffException: public std::exception
{
  public:
    GeoDiffException( const std::string &msg );

    virtual const char *what() const throw();
  private:
    std::string mMsg;
};

class Logger
{
    public:
        static Logger& instance();
        Logger(Logger const&) = delete;
        void operator=(Logger const&) = delete;
        void info( std::initializer_list<const std::string&> list );
        void info( const std::string& msg );
        void warn( const std::string& msg );
        void error( const std::string& msg );
        void error(const GeoDiffException& exp);
    private:
        Logger();
        void log( const std::string& type, const std::string& msg );
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
    void read( const std::string& filename );

    /**
     * Populates buffer from sqlite3 session
     * Frees the existing buffer if exists
     */
    void read( sqlite3_session *session );

    /**
     * Adds formatted text to the end of a buffer
     */
    void printf( const char *zFormat, ... );

    /**
     * Writes buffer to disk
     */
    void write( const std::string& filename );

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

  private:
    sqlite3_value *mVal = nullptr;
};


std::string pOpToStr( int pOp );
std::string conflict2Str( int c );
int changesetIter2Str( sqlite3_changeset_iter *pp );
void errorLogCallback( void *pArg, int iErrCode, const char *zMsg );

/*
** Prepare a new SQL statement. Print an error and abort if anything
** goes wrong.
*/
sqlite3_stmt *db_prepare( sqlite3 *db, const char *zFormat, ... );

/*
** Return the text of an SQL statement that itself returns the list of
** tables to process within the database.
*/
const char *all_tables_sql();

//! copy file from to location. override if exists
void filecopy( const std::string &to, const std::string &from );

//! remove a file if exists
void fileremove( const std::string &path );

//! whether file exists
bool fileexists( const std::string &path );

std::string sqlite_value_2str( sqlite3_value *ppValue );

// WRITE CHANGESET API

/*
** Write an SQLite value onto out.
*/
void putValue( FILE *out, sqlite3_value *ppValue );
void putValue( FILE *out, int ppValue );

/*
** Write a 64-bit signed integer as a varint onto out
** Copied from sqldiff.c
*/
void putsVarint( FILE *out, sqlite3_uint64 v );


#endif // GEODIFFUTILS_H
