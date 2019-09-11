/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFSQLITEUTILS_H
#define GEODIFFSQLITEUTILS_H

#include <string>
#include <memory>
#include <exception>

#include "sqlite3.h"

class Buffer;

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


#endif // GEODIFFSQLITEUTILS_H
