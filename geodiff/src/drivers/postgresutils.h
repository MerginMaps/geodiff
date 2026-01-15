/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef POSTGRESUTILS_H
#define POSTGRESUTILS_H

#include <assert.h>
#include <iostream>
#include <string>

extern "C"
{
#include <libpq-fe.h>
}

#include "geodiffutils.hpp"


class PostgresResult
{
  public:
    explicit PostgresResult( PGresult *result ) : mResult( result ) { }
    // Delete copy constructor, since then we'd PGclear(mResult) multiple times
    PostgresResult( PostgresResult & ) = delete;
    // and create move constructor
    PostgresResult( PostgresResult &&other ) : mResult( other.mResult )
    {
      other.mResult = nullptr;
    }
    ~PostgresResult()
    {
      if ( mResult )
        ::PQclear( mResult );
      mResult = nullptr;
    }

    ExecStatusType status() const
    {
      return mResult ? ::PQresultStatus( mResult ) : PGRES_FATAL_ERROR;
    }

    std::string statusErrorMessage() const
    {
      assert( mResult );
      return ::PQresultErrorMessage( mResult );
    }

    std::string sqlState() const
    {
      assert( mResult );
      return ::PQresultErrorField( mResult, PG_DIAG_SQLSTATE );
    }

    bool isIntegrityError() const
    {
      return sqlState().substr( 0, 2 ) == "23";
    }

    int rowCount() const
    {
      assert( mResult );
      return ::PQntuples( mResult );
    }

    std::string affectedRows() const
    {
      assert( mResult );
      return ::PQcmdTuples( mResult );
    }

    std::string value( int row, int col ) const
    {
      assert( mResult );
      return isNull( row, col )
             ? std::string()
             : std::string( ::PQgetvalue( mResult, row, col ) );
    }

    bool isNull( int row, int col ) const
    {
      assert( mResult );
      return ::PQgetisnull( mResult, row, col );
    }

  private:
    PGresult *mResult = nullptr;

};

class GeoDiffPostgresException: public GeoDiffException
{
  public:
    // Takes ownership of PGresult
    explicit GeoDiffPostgresException( PGresult *res, const std::string &sql );
    explicit GeoDiffPostgresException( PostgresResult res, const std::string &sql );
    const PostgresResult &result() const;
    int errorCode() const override;
  private:
    std::string mSql;
    // MSVC doesn't allow non-copyable exceptions, so we wrap non-copyable
    // PostgresResult in a refcounting smart pointer.
    std::shared_ptr<PostgresResult> mRes;
};

PostgresResult execSql( PGconn *c, const std::string &sql );

std::string quotedIdentifier( const std::string &ident );
std::string quotedString( const std::string &value );


#endif // POSTGRESUTILS_H
