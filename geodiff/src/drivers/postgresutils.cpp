/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "postgresutils.h"
#include "geodiff.h"
#include "geodiffutils.hpp"

GeoDiffPostgresException::GeoDiffPostgresException( PGresult *res, const std::string &sql )
  : GeoDiffPostgresException( PostgresResult( res ), sql ) {}

GeoDiffPostgresException::GeoDiffPostgresException( PostgresResult res, const std::string &sql )
  : GeoDiffException( "postgres cmd error(" + res.sqlState() +
                      "): " + res.statusErrorMessage() +
                      ( sql.size() ? "\n\nSQL:\n" + sql : "" ) ),
    mSql( sql ),
#ifdef _MSC_VER
    mRes( &res )
#else
    mRes( std::move( res ) )
#endif
{
}

const PostgresResult &GeoDiffPostgresException::result() const
{
#ifdef _MSC_VER
  return *mRes;
#else
  return mRes;
#endif
}

int GeoDiffPostgresException::errorCode() const
{
  if ( result().isIntegrityError() )
    return GEODIFF_CONFLICTS;
  else
    return GEODIFF_ERROR;
}

PostgresResult execSql( PGconn *c, const std::string &sql )
{
  PGresult *res = ::PQexec( c, sql.c_str() );

  if ( res && ::PQstatus( c ) == CONNECTION_OK )
  {
    int errorStatus = PQresultStatus( res );
    if ( errorStatus != PGRES_COMMAND_OK && errorStatus != PGRES_TUPLES_OK )
    {
      throw GeoDiffPostgresException( res, sql );
    }

    return PostgresResult( res );
  }
  if ( PQstatus( c ) != CONNECTION_OK )
  {
    if ( res )
      PQclear( res );
    throw GeoDiffException( "postgres conn error: " + std::string( PQerrorMessage( c ) ) );
  }
  else
  {
    throw GeoDiffException( "postgres error: out of memory" );
  }

  return PostgresResult( nullptr );
}

std::string quotedIdentifier( const std::string &ident )
{
  std::string result = replace( ident, "\"", "\"\"" );
  return "\"" + result + "\"";
}

std::string quotedString( const std::string &value )
{
  std::string result = replace( value, "'", "''" );
  if ( result.find( '\\' ) != std::string::npos )
  {
    result = replace( result, "\\", "\\\\" );
    return "E'" + result + "'";
  }
  else
    return "'" +  result + "'";
}
