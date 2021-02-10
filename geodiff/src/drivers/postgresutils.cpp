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
      std::string err( PQresultErrorMessage( res ) );
      PQclear( res );
      throw GeoDiffException( "postgres cmd error: " + err + "\n\nSQL:\n" + sql );
    }

    return res;
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

  return nullptr;
}

int parseGpkgbHeaderSize( const std::string &gpkgWkb )
{
  // see GPKG binary header definition http://www.geopackage.org/spec/#gpb_spec

  // parse envelope size
  char flagByte = gpkgWkb[ GPKG_FLAG_BYTE_POS ];

  char envelope_byte = ( flagByte & GPKG_ENVELOPE_SIZE_MASK ) >> 1;
  int envelope_size = 0;

  switch ( envelope_byte )
  {
    case 1:
      envelope_size = 32;
      break;

    case 2:
    // fall through
    case 3:
      envelope_size = 48;
      break;

    case 4:
      envelope_size = 64;
      break;

    default:
      envelope_size = 0;
      break;
  }

  return GPKG_NO_ENVELOPE_HEADER_SIZE + envelope_size;
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
