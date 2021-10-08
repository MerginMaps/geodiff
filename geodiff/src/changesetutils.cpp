/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetutils.h"

#include "base64utils.h"
#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"
#include "tableschema.h"


ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl )
{
  ChangesetTable chTable;
  chTable.name = tableName;
  for ( const TableColumnInfo &c : tbl.columns )
    chTable.primaryKeys.push_back( c.isPrimaryKey );
  return chTable;
}

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer )
{
  std::string currentTableName;
  std::vector<bool> currentPkeys;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    assert( entry.table );
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
  //val = replace( val, "\\", "\\\\" );  // TODO: escaping of backslashes?
  val = replace( val, "\n", "\\n" );
  val = replace( val, "\r", "\\r" );
  val = replace( val, "\t", "\\t" );
  val = replace( val, "\"", "\\\"" );
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
      return to_string_with_max_precision( value.getDouble() );
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
    default:
      return "\"(unknown)\"";  // should never happen
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
  for ( size_t i = 0; i < entry.table->columnCount(); ++i )
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


std::string conflictToJSON( const ConflictFeature &conflict )
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
    res += "              \"column\": " + std::to_string( item.column() );

    std::string strValueBase = valueToJSON( item.base() );
    std::string strValueOld = valueToJSON( item.theirs() );
    std::string strValueNew = valueToJSON( item.ours() );
    if ( !strValueBase.empty() )
      res += ",\n              \"base\": " + strValueBase;
    if ( !strValueOld.empty() )
      res += ",\n              \"old\": " + strValueOld;
    if ( !strValueNew.empty() )
      res += ",\n              \"new\": " + strValueNew;

    res += "\n";
    res += "          }";
  }
  // close brackets
  res += "\n        ]\n"; // end properties
  res += "      }"; // end feature
  return res;
}

std::string conflictsToJSON( const std::vector<ConflictFeature> &conflicts )
{
  std::string res = "{\n   \"geodiff\": [";

  bool first = true;
  for ( const ConflictFeature &item : conflicts )
  {
    std::string msg = conflictToJSON( item );
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

inline int hex2num( unsigned char i )
{
  if ( i <= '9' && i >= '0' )
    return i - '0';
  if ( i >= 'A' && i <= 'F' )
    return 10 + i - 'A';
  if ( i >= 'a' && i <= 'f' )
    return 10 + i - 'a';
  assert( false );
}

inline char num2hex( int n )
{
  assert( n >= 0 && n < 16 );
  if ( n >= 0 && n < 10 )
    return '0' + n;
  else if ( n >= 10 && n < 16 )
    return 'A' + n - 10;
  return '?';  // should never happen
}

std::string hex2bin( const std::string &str )
{
  assert( str.size() % 2 == 0 );
  std::string output( str.size() / 2, 0 );
  for ( size_t i = 0; i < str.size(); i += 2 )
  {
    int n1 = hex2num( str[i] ), n2 = hex2num( str[i + 1] );
    output[i / 2] = n1 * 16 + n2;
  }
  return output;
}

std::string bin2hex( const std::string &str )
{
  std::string output( str.size() * 2, 0 );
  for ( size_t i = 0; i < str.size(); ++i )
  {
    unsigned char ch = str[i];
    output[i * 2] = num2hex( ch / 16 );
    output[i * 2 + 1] = num2hex( ch % 16 );
  }
  return output;
}
