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
  return val;
}

json valueToJSON( const Value &value )
{
  json j;
  switch ( value.type() )
  {
    case Value::TypeUndefined:
      break;  // actually this not get printed - undefined value should be omitted completely
    case Value::TypeInt:
      j= value.getInt();
      break;
    case Value::TypeDouble:
      j = value.getDouble();
      break;
    case Value::TypeText:
      j = value.getString();
      break;
    case Value::TypeBlob:
    {
      // this used to either show "blob N bytes" or would be converted to WKT
      // but this is better - it preserves content of any type + can be decoded back
      std::string base64 = base64_encode( ( const unsigned char * ) value.getString().data(), ( unsigned int ) value.getString().size() );
      j = escapeJSONString( base64 );
      break;
    }
    case Value::TypeNull:
      j = "null";
      break;
    default:
      j = "(unknown)";  // should never happen
  }
  return j;
}


json changesetEntryToJSON( const ChangesetEntry &entry )
{
  std::string status;
  if ( entry.op == ChangesetEntry::OpUpdate )
    status = "update";
  else if ( entry.op == ChangesetEntry::OpInsert )
    status = "insert";
  else if ( entry.op == ChangesetEntry::OpDelete )
    status = "delete";

  json res;
  res[ "table" ] = entry.table->name;
  res[ "type" ] = status;

  auto entries = json::array();

  Value valueOld, valueNew;
  for ( size_t i = 0; i < entry.table->columnCount(); ++i )
  {
    valueNew = ( entry.op == ChangesetEntry::OpUpdate || entry.op == ChangesetEntry::OpInsert ) ? entry.newValues[i] : Value();
    valueOld = ( entry.op == ChangesetEntry::OpUpdate || entry.op == ChangesetEntry::OpDelete ) ? entry.oldValues[i] : Value();

    json change;

    if ( valueNew.type() != Value::TypeUndefined || valueOld.type() != Value::TypeUndefined )
    {
      change[ "column" ] = i;

      json jsonValueOld = valueToJSON( valueOld );
      json jsonValueNew = valueToJSON( valueNew );
      if ( !jsonValueOld.empty() )
        change[ "old" ] = jsonValueOld;
      if ( !jsonValueNew.empty() )
        change[ "new" ] = jsonValueNew;

      entries.push_back( change );
    }
  }

  res[ "changes" ] = entries;
  return res;
}

json changesetToJSON( ChangesetReader &reader )
{
  auto entries = json::array();

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    json msg = changesetEntryToJSON( entry );
    if ( msg.empty() )
      continue;

    entries.push_back( msg );
  }

  json res;
  res[ "geodiff" ] = entries;
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

json changesetToJSONSummary( ChangesetReader &reader )
{
  std::map< std::string, TableSummary > summary;

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
  auto entries = json::array();
  for ( const auto &kv : summary )
  {
    json tableJson;
    tableJson[ "table" ] = kv.first;
    tableJson[ "insert" ] = kv.second.inserts;
    tableJson[ "update" ] = kv.second.updates;
    tableJson[ "delete" ] = kv.second.deletes;

    entries.push_back( tableJson );
  }
  json res;
  res[ "geodiff_summary" ] = entries;
  return res;
}

json conflictToJSON( const ConflictFeature &conflict )
{
  json res;
  res[ "table" ] = std::string( conflict.tableName() );
  res[ "type" ] = "conflict";
  res[ "fid" ] = conflict.pk();

  auto entries = json::array();

  const std::vector<ConflictItem> items = conflict.items();
  for ( const ConflictItem &item : items )
  {
    json change;
    change[ "column" ] = std::to_string( item.column() );

    json valueBase = valueToJSON( item.base() );
    json valueOld = valueToJSON( item.theirs() );
    json valueNew = valueToJSON( item.ours() );
    if ( !valueBase.empty() )
      change[ "base" ] = valueBase;
    if ( !valueOld.empty() )
      change[ "old" ] = valueOld;
    if ( !valueNew.empty() )
      change[ "new" ] = valueNew;

    entries.push_back( change );
  }
  res[ "changes" ] = entries;
  return res;
}

json conflictsToJSON( const std::vector<ConflictFeature> &conflicts )
{
  auto entries = json::array();
  for ( const ConflictFeature &item : conflicts )
  {
    json msg = conflictToJSON( item );
    if ( msg.empty() )
      continue;

    entries.push_back( msg );
  }

  json res;
  res[ "geodiff" ] = entries;
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
  return 0; // should never happen
}

inline char num2hex( int n )
{
  assert( n >= 0 && n < 16 );
  if ( n >= 0 && n < 10 )
    return char( '0' + n );
  else if ( n >= 10 && n < 16 )
    return char( 'A' + n - 10 );
  return '?';  // should never happen
}

std::string hex2bin( const std::string &str )
{
  assert( str.size() % 2 == 0 );
  std::string output( str.size() / 2, 0 );
  for ( size_t i = 0; i < str.size(); i += 2 )
  {
    int n1 = hex2num( str[i] ), n2 = hex2num( str[i + 1] );
    output[i / 2] = char( n1 * 16 + n2 );
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
