/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef POSTGRESUTILS_H
#define POSTGRESUTILS_H

#include <assert.h>
#include <string>

extern "C"
{
#include <libpq-fe.h>
}


class PostgresResult
{
  public:
    PostgresResult( PGresult *result ) : mResult( result ) {}
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

PGresult *execSql( PGconn *c, const std::string &sql );

std::string quotedIdentifier( const std::string &ident );
std::string quotedString( const std::string &value );


#endif // POSTGRESUTILS_H
