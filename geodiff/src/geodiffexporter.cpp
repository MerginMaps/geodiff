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
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

GeoDiffExporter::GeoDiffExporter()
{
  mDb = blankGeopackageDb();
}

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

void GeoDiffExporter::addValue( std::string &stream,
                                sqlite3_value *ppValue, const std::string &type ) const
{
  std::string val;
  if ( ppValue )
  {
    if ( sqlite3_value_type( ppValue ) == SQLITE_BLOB )
    {
      val = convertGeometryToWKT( mDb, ppValue );
      if ( val.empty() )
        val = Sqlite3Value::toString( ppValue );
    }
    else
      val = Sqlite3Value::toString( ppValue );
  }
  if ( val.empty() )
  {
    stream += "              \"" + type + "\": null";
  }
  else
  {
    val = replace( val, "\n", "\\n" );
    val = replace( val, "\r", "\\r" );
    val = replace( val, "\t", "\\t" );
    val = replace( val, "\"", "\\\"" );
    stream += "              \"" + type + "\": \"" + val + "\"";
  }
}

void GeoDiffExporter::addValue( std::string &stream,
                                std::shared_ptr<Sqlite3Value> value, const std::string &type ) const
{
  if ( value )
    return addValue( stream, value->value(), type );
  else
    return addValue( stream, nullptr, type );
}

std::string GeoDiffExporter::toJSON( Sqlite3ChangesetIter &pp ) const
{
  if ( !pp.get() )
    return std::string();


  const char *pzTab = nullptr;
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
  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "sqlite3changeset_op error" );
  }

  std::string status;
  if ( pOp == SQLITE_UPDATE )
    status = "update";
  else if ( pOp == SQLITE_INSERT )
    status = "insert";
  else if ( pOp == SQLITE_DELETE )
    status = "delete";

  std::string res = "      {\n";
  res += "        \"table\": \"" + std::string( pzTab ) + "\",\n";
  res += "        \"type\": \"" + status + "\",\n";

  sqlite3_value *ppValueOld = nullptr;
  sqlite3_value *ppValueNew = nullptr;
  res += "        \"changes\": [";
  bool first = true;

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

    if ( ppValueNew || ppValueOld )
    {
      if ( first )
      {
        first = false;
        res += "\n          {\n";
      }
      else
      {
        res += ",\n          {\n";
      }
      res += "              \"column\": " + std::to_string( i ) + ",\n";

      addValue( res, ppValueOld, "old" );
      res += ",\n";
      addValue( res, ppValueNew, "new" );
      res += "\n";
      res += "          }";
    }

  }
  // close brackets
  res += "\n        ]\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string GeoDiffExporter::toJSON( Buffer &buf ) const
{
  std::string res = "{\n   \"geodiff\": [";

  Sqlite3ChangesetIter pp;
  pp.start( buf );
  bool first = true;
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    std::string msg = GeoDiffExporter::toJSON( pp );
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

  res += "\n   ]\n";
  res += "}";
  return res;
}

//! auxiliary table used to create table changes summary
struct TableSummary
{
  TableSummary() : inserts( 0 ), updates( 0 ), deletes( 0 ) {}
  int inserts;
  int updates;
  int deletes;
};

std::string GeoDiffExporter::toJSONSummary( Buffer &buf ) const
{
  std::map< std::string, TableSummary > summary;

  Sqlite3ChangesetIter pp;
  pp.start( buf );
  bool first = true;
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    const char *pzTab = nullptr;
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
    if ( rc != SQLITE_OK )
    {
      throw GeoDiffException( "sqlite3changeset_op error" );
    }

    std::string tableName = pzTab;
    TableSummary &tableSummary = summary[tableName];

    if ( pOp == SQLITE_UPDATE )
      ++tableSummary.updates;
    else if ( pOp == SQLITE_INSERT )
      ++tableSummary.inserts;
    else if ( pOp == SQLITE_DELETE )
      ++tableSummary.deletes;
  }

  // write JSON
  std::string res = "{\n   \"geodiff_summary\": [";
  for ( const auto &kv : summary )
  {
    std::string tableJson;
    tableJson += "      {\n";
    tableJson += "         \"table\": \"" + kv.first + "\",\n";
    tableJson += "         \"insert\": " + std::to_string( kv.second.inserts ) + ",\n";
    tableJson += "         \"update\": " + std::to_string( kv.second.updates ) + ",\n";
    tableJson += "         \"delete\": " + std::to_string( kv.second.deletes ) + "\n";
    tableJson += "      }";

    if ( first )
    {
      res += "\n" + tableJson;
      first = false;
    }
    else
    {
      res += ",\n" + tableJson;
    }

  }
  res += "\n   ]\n";
  res += "}";
  return res;
}

std::string GeoDiffExporter::toJSON( const ConflictFeature &conflict ) const
{
  std::string status = "conflict";

  std::string res = "      {\n";
  res += "        \"table\": \"" + std::string( conflict.tableName() ) + "\",\n";
  res += "        \"type\": \"" + status + "\",\n";
  res += "        \"fid\": \"" + std::to_string( conflict.pk() ) + "\",\n";
  res += "        \"changes\": [";
  bool first = true;

  const std::vector<ConflictItem> items = conflict.items();
  for ( const ConflictItem &item : items )
  {
    if ( first )
    {
      first = false;
      res += "\n          {\n";
    }
    else
    {
      res += ",\n          {\n";
    }
    res += "              \"column\": " + std::to_string( item.column() ) + ",\n";
    addValue( res, item.base(), "base" );
    res += ",\n";
    addValue( res, item.theirs(), "old" );
    res += ",\n";
    addValue( res, item.ours(), "new" );
    res += "\n";
    res += "          }";

  }
  // close brackets
  res += "\n        ]\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string GeoDiffExporter::toJSON( const std::vector<ConflictFeature> &conflicts ) const
{
  std::string res = "{\n   \"geodiff\": [";

  bool first = true;
  for ( const ConflictFeature &item : conflicts )
  {
    std::string msg = GeoDiffExporter::toJSON( item );
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

  res += "\n   ]\n";
  res += "}";
  return res;
}
