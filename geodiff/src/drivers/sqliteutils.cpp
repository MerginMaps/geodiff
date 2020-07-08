/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "sqliteutils.h"

#include "geodiffutils.hpp"


Sqlite3Db::Sqlite3Db() = default;
Sqlite3Db::~Sqlite3Db()
{
  close();
}

void Sqlite3Db::open( const std::string &filename )
{
  close();
  int rc = sqlite3_open_v2( filename.c_str(), &mDb, SQLITE_OPEN_READWRITE, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "Unable to open " + filename + " as sqlite3 database" );
  }
}

void Sqlite3Db::create( const std::string &filename )
{
  close();

  if ( fileexists( filename ) )
  {
    throw GeoDiffException( "Unable to create sqlite3 database - already exists: " + filename );
  }

  int rc = sqlite3_open_v2( filename.c_str(), &mDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "Unable to create " + filename + " as sqlite3 database" );
  }
}

void Sqlite3Db::exec( const Buffer &buf )
{
  int rc = sqlite3_exec( get(), buf.c_buf(), NULL, 0, NULL );
  if ( rc )
  {
    throw GeoDiffException( "Unable to exec buffer on sqlite3 database" );
  }
}

sqlite3 *Sqlite3Db::get()
{
  return mDb;
}

void Sqlite3Db::close()
{
  if ( mDb )
  {
    sqlite3_close( mDb );
    mDb = nullptr;
  }
}



Sqlite3Stmt::Sqlite3Stmt() = default;

Sqlite3Stmt::~Sqlite3Stmt()
{
  close();
}

sqlite3_stmt *Sqlite3Stmt::db_vprepare( sqlite3 *db, const char *zFormat, va_list ap )
{
  char *zSql;
  int rc;
  sqlite3_stmt *pStmt;

  zSql = sqlite3_vmprintf( zFormat, ap );
  if ( zSql == nullptr )
  {
    throw GeoDiffException( "out of memory" );
  }

  rc = sqlite3_prepare_v2( db, zSql, -1, &pStmt, nullptr );
  sqlite3_free( zSql );
  if ( rc )
  {
    throw GeoDiffException( "SQL statement error: " + std::string( sqlite3_errmsg( db ) ) );
  }
  return pStmt;
}

void Sqlite3Stmt::prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... )
{
  if ( db && db->get() )
  {
    va_list ap;
    va_start( ap, zFormat );
    mStmt = db_vprepare( db->get(), zFormat, ap );
    va_end( ap );
  }
}

void Sqlite3Stmt::prepare( std::shared_ptr<Sqlite3Db> db, const std::string &sql )
{
  sqlite3_stmt *pStmt;
  int rc = sqlite3_prepare_v2( db->get(), sql.c_str(), -1, &pStmt, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "SQL statement error: " + std::string( sqlite3_errmsg( db->get() ) ) );
  }
  mStmt = pStmt;
}

sqlite3_stmt *Sqlite3Stmt::get()
{
  return mStmt;
}

void Sqlite3Stmt::close()
{
  if ( mStmt )
  {
    sqlite3_finalize( mStmt );
    mStmt = nullptr;
  }
}

std::string Sqlite3Stmt::expandedSql() const
{
  char *str = sqlite3_expanded_sql( mStmt );
  std::string sql( str );
  sqlite3_free( str );
  return sql;
}
