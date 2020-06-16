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

#include "sqlite3.h"
#include "geodiff.h"
#include "changeset.h"

class Buffer;

class GeoDiffException: public std::exception
{
  public:
    GeoDiffException( const std::string &msg );
    virtual const char *what() const throw();
  private:
    std::string mMsg;
};

class Sqlite3Db
{
  public:
    Sqlite3Db();
    ~Sqlite3Db();
    void open( const std::string &filename );
    //! Creates DB file (overwrites if one exists already)
    void create( const std::string &filename );
    void exec( const Buffer &buf );

    sqlite3 *get();
    void close();
  private:
    sqlite3 *mDb = nullptr;
};

//! Create black simple geopackage database
//! to be able to run ST_* functions
std::shared_ptr<Sqlite3Db> blankGeopackageDb();

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
    void prepare( std::shared_ptr<Sqlite3Db> db, const std::string &sql );
    void prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... );
    sqlite3_stmt *get();
    void close();
    //! Returns SQL statement with bound parameters expanded
    std::string expandedSql() const;
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

    /** Populates from stream. Takes ownership of stream */
    void read( int size, void *stream );

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

bool isLayerTable( const std::string &tableName );

typedef std::pair<std::string, int> TableColumn; //table name, column ID tree, 4(specie)
typedef std::map<TableColumn, TableColumn> ForeignKeys; // key is FK to value, e.g tree, 4(specie) -> species, 1(fid)
ForeignKeys foreignKeys( std::shared_ptr<Sqlite3Db> db, const std::string &dbName );

int indexOf( const std::vector<std::string> &arr, const std::string &val );

std::vector<std::string> columnNames(
  std::shared_ptr<Sqlite3Db> db,
  const std::string &zDb,
  const std::string &tableName
);

bool has_same_table_schema( std::shared_ptr<Sqlite3Db> db,
                            const std::string &tableName,
                            std::string &errStr );

struct ChangesetEntry;
void get_primary_key( const ChangesetEntry &entry, int &fid, int &nColumn );

bool register_gpkg_extensions( std::shared_ptr<Sqlite3Db> db );

bool isGeoPackage( std::shared_ptr<Sqlite3Db> db );

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
