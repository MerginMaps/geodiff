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

void _addValue( std::shared_ptr<Sqlite3Db> db, std::string &stream,
                sqlite3_value *ppValue, const std::string &type )
{
  std::string val;
  if ( ppValue )
  {
    if ( sqlite3_value_type( ppValue ) == SQLITE_BLOB )
    {
      val = convertGeometryToWKT( db, ppValue );
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

std::string GeoDiffExporter::toJSON( std::shared_ptr<Sqlite3Db> db, Sqlite3ChangesetIter &pp )
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

      _addValue( db, res, ppValueOld, "old" );
      res += ",\n";
      _addValue( db, res, ppValueNew, "new" );
      res += "\n";
      res += "          }";
    }

  }
  // close brackets
  res += "\n        ]\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string GeoDiffExporter::toJSON( std::shared_ptr<Sqlite3Db> db, Buffer &buf )
{
  std::string res = "{\n   \"geodiff\": [";

  Sqlite3ChangesetIter pp;
  pp.start( buf );
  bool first = true;
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
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

std::string GeoDiffExporter::toJSONSummary( Buffer &buf )
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
