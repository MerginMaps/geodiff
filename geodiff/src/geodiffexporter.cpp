/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffexporter.hpp"
#include "sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

std::string GeoDiffExporter::toString( sqlite3_changeset_iter *pp )
{
  std::ostringstream ret;

  if ( !pp )
    return std::string();

  int rc;
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  rc = sqlite3changeset_op(
         pp,
         &pzTab,
         &pnCol,
         &pOp,
         &pbIndirect
       );
  std::string s = pOpToStr( pOp );
  ret << " " << pzTab << " " << s << "    columns " << pnCol << "    indirect " << pbIndirect << std::endl;

  sqlite3_value *ppValueOld;
  sqlite3_value *ppValueNew;
  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_INSERT )
    {
      rc = sqlite3changeset_new( pp, i, &ppValueNew );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_new" );
    }
    else
      ppValueNew = nullptr;

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      rc = sqlite3changeset_old( pp, i, &ppValueOld );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_old" );
    }
    else
      ppValueOld = nullptr;

    if ( ppValueNew || ppValueOld )
      ret << "  " << i << ": " << Sqlite3Value::toString( ppValueOld ) << "->" << Sqlite3Value::toString( ppValueNew ) << std::endl;
  }
  ret << std::endl;
  return ret.str();

}

std::string GeoDiffExporter::toJSON( std::shared_ptr<Sqlite3Db> db, Sqlite3ChangesetIter &pp )
{
  if ( !pp.get() )
    return std::string();

  std::string res = "      {\n";
  res += "        \"type\": \"Feature\",\n";

  // properties
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  int rc = sqlite3changeset_op(
             pp.get(),
             &pzTab,
             &pnCol,
             &pOp,
             &pbIndirect
           );

  std::vector<std::string> columns = columnNames( db, "main", pzTab );

  // find geometry column
  int geometryColumnIndex = -1;
  for ( int j = 0; j < columns.size(); ++j )
  {
    if ( ( columns[j] == std::string( "geometry" ) ) ||
         ( columns[j] == std::string( "geom" ) )
       )
      geometryColumnIndex = j;
  }

  if ( geometryColumnIndex < 0 )
    return std::string();

  // add geometry
  std::string wkt;
  sqlite3_value *ppValueOld = nullptr;
  sqlite3_value *ppValueNew = nullptr;

  if ( pOp == SQLITE_INSERT )
  {
    rc = sqlite3changeset_new( pp.get(), geometryColumnIndex, &ppValueNew );
    if ( rc != SQLITE_OK )
      throw GeoDiffException( "sqlite3changeset_new" );
    wkt = convertGeometrytoWKT( db, ppValueNew );
  }
  else
  {
    int fid, nFidColumn;
    get_primary_key( pp, pOp, fid, nFidColumn );
    wkt = getGeometry(
            db,
            fid,
            columns[geometryColumnIndex],
            columns[nFidColumn],
            "main." + std::string( pzTab ) );
  }

  // CRS and other global attributes
  std::string crs = getCRS( db, std::string( pzTab ) ) ;

  res += "        \"geometry\": {\n";
  res += "          \"WKT\": \"" + wkt + "\",\n";
  res += "          \"EPSG\": \"" + crs + "\"\n";
  res += "        },\n";

  // add properties
  res += "        \"properties\": {\n";

  std::string status;
  if ( pOp == SQLITE_UPDATE )
    status = "modified";
  else if ( pOp == SQLITE_INSERT )
    status = "added";
  else if ( pOp == SQLITE_DELETE )
    status = "deleted";
  else
    status = "unknown";

  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_INSERT )
    {
      rc = sqlite3changeset_new( pp.get(), i, &ppValueNew );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_new" );
    }
    else
      ppValueNew = nullptr;

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      rc = sqlite3changeset_old( pp.get(), i, &ppValueOld );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_old" );
    }
    else
      ppValueOld = nullptr;

    const std::string columnName = columns.at( size_t( i ) );

    if ( pOp == SQLITE_UPDATE && ppValueNew && ppValueOld && i == geometryColumnIndex )
    {
      status = "moved";
    };

    if ( i != geometryColumnIndex )
    {
      std::string oldVal = Sqlite3Value::toString( ppValueOld );
      std::string newVal = Sqlite3Value::toString( ppValueNew );
      if ( ppValueNew )
        if ( ppValueOld )
        {
          res += "          \"" + columnName + "\": \"" + oldVal + " -> " + newVal + "\",\n";
        }
        else
        {
          res += "          \"" + columnName + "\": \"" + newVal + "\",\n";
        }
      else
      {
        if ( ppValueOld )
        {
          res += "          \"" + columnName + "\": \"" + oldVal + "\",\n";
        }
      }
    }
  }

  // add status
  res += "          \"status\": \"" + status + "\"\n";

  // close brackets
  res += "        }\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string GeoDiffExporter::toJSON( std::shared_ptr<Sqlite3Db> db, Buffer &buf, int &nchanges )
{
  nchanges = 0;

  std::string res = "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [";

  Sqlite3ChangesetIter pp;
  pp.start( buf );
  bool first = true;
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    ++nchanges;
    std::string msg = GeoDiffExporter::toJSON( db, pp );
    if ( msg.empty() )
      continue;

    if ( first )
    {
      res += "\n" + msg;
      first = false;
    }
    else
    {
      res += ",\n" + msg;
    }
  }

  res += "\n  ]\n";
  res += "}";
  return res;
}
