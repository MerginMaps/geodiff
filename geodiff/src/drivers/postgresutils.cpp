/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "postgresutils.h"

#include "geodiffutils.hpp"


PGresult *execSql( PGconn *c, const std::string &sql )
{
  PGresult *res = ::PQexec( c, sql.c_str() );

  if ( res && ::PQstatus( c ) == CONNECTION_OK )
  {
    int errorStatus = PQresultStatus( res );
    if ( errorStatus != PGRES_COMMAND_OK && errorStatus != PGRES_TUPLES_OK )
    {
      throw GeoDiffException( "postgres cmd error: " + std::string( PQresultErrorMessage( res ) ) );
    }

    return res;
  }
  if ( PQstatus( c ) != CONNECTION_OK )
  {
    throw GeoDiffException( "postgres conn error: " + std::string( PQerrorMessage( c ) ) );
  }
  else
  {
    throw GeoDiffException( "postgres error: out of memory" );
  }

  return nullptr;
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
