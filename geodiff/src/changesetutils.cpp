/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetutils.h"

#include "base64utils.h"
#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"


void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer )
{
  std::string currentTableName;
  std::vector<bool> currentPkeys;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( entry.table->name != currentTableName )
    {
      writer.beginTable( *entry.table );
      currentTableName = entry.table->name;
      currentPkeys = entry.table->primaryKeys;
    }

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpDelete;
      out.oldValues = entry.newValues;
      writer.writeEntry( out );
    }
    else if ( entry.op == ChangesetEntry::OpDelete )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpInsert;
      out.newValues = entry.oldValues;
      writer.writeEntry( out );
    }
    else if ( entry.op == ChangesetEntry::OpUpdate )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpUpdate;
      out.newValues = entry.oldValues;
      out.oldValues = entry.newValues;
      // if a column is a part of pkey and has not been changed,
      // the original entry has "old" value the pkey value and "new"
      // value is undefined - let's reverse "old" and "new" in that case.
      for ( size_t i = 0; i < currentPkeys.size(); ++i )
      {
        if ( currentPkeys[i] && out.oldValues[i].type() == Value::TypeUndefined )
        {
          out.oldValues[i] = out.newValues[i];
          out.newValues[i].setUndefined();
        }
      }
      writer.writeEntry( out );
    }
    else
    {
      throw GeoDiffException( "Unknown entry operation!" );
    }
  }
}

std::string escapeJSONString( std::string val )
{
  val = replace( val, "\n", "\\n" );
  val = replace( val, "\r", "\\r" );
  val = replace( val, "\t", "\\t" );
  val = replace( val, "\"", "\\\"" );
  val = replace( val, "\\", "\\\\" );
  return "\"" + val + "\"";
}

std::string valueToJSON( const Value &value )
{
  switch ( value.type() )
  {
    case Value::TypeUndefined:
      return std::string();  // actually this not get printed - undefined value should be omitted completely
    case Value::TypeInt:
      return std::to_string( value.getInt() );
    case Value::TypeDouble:
      return std::to_string( value.getDouble() );
    case Value::TypeText:
      return escapeJSONString( value.getString() );
    case Value::TypeBlob:
    {
      // this used to either show "blob N bytes" or would be converted to WKT
      // but this is better - it preserves content of any type + can be decoded back
      std::string base64 = base64_encode( ( const unsigned char * ) value.getString().data(), value.getString().size() );
      return escapeJSONString( base64 );
    }
    case Value::TypeNull:
      return "null";
  }
}


std::string changesetEntryToJSON( const ChangesetEntry &entry )
{
  std::string status;
  if ( entry.op == ChangesetEntry::OpUpdate )
    status = "update";
  else if ( entry.op == ChangesetEntry::OpInsert )
    status = "insert";
  else if ( entry.op == ChangesetEntry::OpDelete )
    status = "delete";

  std::string res = "      {\n";
  res += "        \"table\": \"" + entry.table->name + "\",\n";
  res += "        \"type\": \"" + status + "\",\n";
  res += "        \"changes\": [";
  bool first = true;

  Value valueOld, valueNew;
  for ( size_t i = 0; i < entry.table->primaryKeys.size(); ++i )
  {
    valueNew = ( entry.op == ChangesetEntry::OpUpdate || entry.op == ChangesetEntry::OpInsert ) ? entry.newValues[i] : Value();
    valueOld = ( entry.op == ChangesetEntry::OpUpdate || entry.op == ChangesetEntry::OpDelete ) ? entry.oldValues[i] : Value();

    if ( valueNew.type() != Value::TypeUndefined || valueOld.type() != Value::TypeUndefined )
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
      res += "              \"column\": " + std::to_string( i );

      std::string strValueOld = valueToJSON( valueOld );
      std::string strValueNew = valueToJSON( valueNew );
      if ( !strValueOld.empty() )
        res += ",\n              \"old\": " + strValueOld;
      if ( !strValueNew.empty() )
        res += ",\n              \"new\": " + strValueNew;
      res += "\n          }";
    }
  }
  // close brackets
  res += "\n        ]\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string changesetToJSON( ChangesetReader &reader )
{
  std::string res = "{\n   \"geodiff\": [";

  ChangesetEntry entry;
  bool first = true;
  while ( reader.nextEntry( entry ) )
  {
    std::string msg = changesetEntryToJSON( entry );
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

std::string changesetToJSONSummary( ChangesetReader &reader )
{
  std::map< std::string, TableSummary > summary;

  bool first = true;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    std::string tableName = entry.table->name;
    TableSummary &tableSummary = summary[tableName];

    if ( entry.op == ChangesetEntry::OpUpdate )
      ++tableSummary.updates;
    else if ( entry.op == ChangesetEntry::OpInsert )
      ++tableSummary.inserts;
    else if ( entry.op == ChangesetEntry::OpDelete )
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
