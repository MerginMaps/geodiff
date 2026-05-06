/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetutils.h"

#include "base64utils.h"
#include "changeset.h"
#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"
#include "tableschema.h"
#include <iostream>
#include <unordered_map>


ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl )
{
  ChangesetTable chTable;
  chTable.name = tableName;
  for ( const TableColumnInfo &c : tbl.columns )
    chTable.primaryKeys.push_back( c.isPrimaryKey );
  return chTable;
}

// Returns inverted changeset entries in reverse order
std::vector<ChangesetEntry> invertChangesetReverse( ChangesetReader &reader )
{
  std::string currentTableName;
  std::vector<ChangesetEntry> invertedEntries;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( ChangesetDataEntry *dataEntry = std::get_if<ChangesetDataEntry>( &entry ) )
    {
      if ( dataEntry->op == ChangesetDataEntry::OpInsert )
      {
        ChangesetDataEntry out;
        out.op = ChangesetDataEntry::OpDelete;
        out.table = dataEntry->table;
        out.oldValues = dataEntry->newValues;
        invertedEntries.push_back( out );
      }
      else if ( dataEntry->op == ChangesetDataEntry::OpDelete )
      {
        ChangesetDataEntry out;
        out.op = ChangesetDataEntry::OpInsert;
        out.table = dataEntry->table;
        out.newValues = dataEntry->oldValues;
        invertedEntries.push_back( out );
      }
      else if ( dataEntry->op == ChangesetDataEntry::OpUpdate )
      {
        ChangesetDataEntry out;
        out.op = ChangesetDataEntry::OpUpdate;
        out.table = dataEntry->table;
        out.newValues = dataEntry->oldValues;
        out.oldValues = dataEntry->newValues;
        // if a column is a part of pkey and has not been changed,
        // the original entry has "old" value the pkey value and "new"
        // value is undefined - let's reverse "old" and "new" in that case.
        for ( size_t i = 0; i < dataEntry->table->primaryKeys.size(); ++i )
        {
          if ( dataEntry->table->primaryKeys[i] && out.oldValues[i].type() == Value::TypeUndefined )
          {
            out.oldValues[i] = out.newValues[i];
            out.newValues[i].setUndefined();
          }
        }
        invertedEntries.push_back( out );
      }
      else
      {
        throw GeoDiffException( "Unknown entry operation!" );
      }
    }
    else if ( ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
    {
      ChangesetDropTableEntry out;
      out.tableName = ctEntry->tableName;
      out.columns = ctEntry->columns;
      invertedEntries.push_back( out );
    }
    else if ( ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
    {
      ChangesetDropColumnEntry out;
      out.tableName = acEntry->tableName;
      out.column = acEntry->column;
      invertedEntries.push_back( out );
    }
    else if ( ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
    {
      ChangesetCreateTableEntry out;
      out.tableName = dtEntry->tableName;
      out.columns = dtEntry->columns;
      invertedEntries.push_back( out );
    }
    else if ( ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
    {
      ChangesetAddColumnEntry out;
      out.tableName = dcEntry->tableName;
      out.column = dcEntry->column;
      invertedEntries.push_back( out );
    }
    else
    {
      throw GeoDiffException( "Cannot invert changeset entry variant " + std::to_string( entry.index() ) );
    }
  }
  return invertedEntries;
}

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer )
{
  std::vector<ChangesetEntry> invertedReverse = invertChangesetReverse( reader );
  ChangesetTable *currentTable = nullptr;
  for ( size_t i = 1; i <= invertedReverse.size(); i++ )
  {
    const auto &entry = invertedReverse[invertedReverse.size() - i];
    if ( const ChangesetDataEntry *dataEntry = std::get_if<ChangesetDataEntry>( &entry ) )
    {
      if ( dataEntry->table.get() != currentTable )
      {
        writer.beginTable( *dataEntry->table );
        currentTable = dataEntry->table.get();
      }
    }

    writer.writeEntry( entry );
  }
}

nlohmann::json valueToJSON( const Value &value )
{
  nlohmann::json j;
  switch ( value.type() )
  {
    case Value::TypeUndefined:
      break;  // actually this not get printed - undefined value should be omitted completely
    case Value::TypeInt:
      j = value.getInt();
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
      std::string base64 = base64_encode(
                             reinterpret_cast<const unsigned char *>( value.getString().data() ),
                             static_cast<unsigned int>( value.getString().size() ) );
      j = base64;
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


nlohmann::json changesetDataEntryToJSON( const ChangesetDataEntry &entry )
{
  std::string status;
  if ( entry.op == ChangesetDataEntry::OpUpdate )
    status = "update";
  else if ( entry.op == ChangesetDataEntry::OpInsert )
    status = "insert";
  else if ( entry.op == ChangesetDataEntry::OpDelete )
    status = "delete";

  // Check that the table column count matches the vector sizes to prevent
  // out-of-bounds errors.
  if ( ( ( entry.op == ChangesetDataEntry::OpUpdate || entry.op == ChangesetDataEntry::OpInsert )
         && entry.table->columnCount() != entry.newValues.size() )
       || ( ( entry.op == ChangesetDataEntry::OpUpdate || entry.op == ChangesetDataEntry::OpDelete )
            && entry.table->columnCount() != entry.oldValues.size() ) )
    throw GeoDiffException( "Table column count doesn't match value list size" );

  nlohmann::json res;
  res[ "table" ] = entry.table->name;
  res[ "type" ] = status;

  auto entries = nlohmann::json::array();

  Value valueOld, valueNew;
  for ( size_t i = 0; i < entry.table->columnCount(); ++i )
  {
    valueNew = ( entry.op == ChangesetDataEntry::OpUpdate || entry.op == ChangesetDataEntry::OpInsert ) ? entry.newValues[i] : Value();
    valueOld = ( entry.op == ChangesetDataEntry::OpUpdate || entry.op == ChangesetDataEntry::OpDelete ) ? entry.oldValues[i] : Value();

    nlohmann::json change;

    if ( valueNew.type() != Value::TypeUndefined || valueOld.type() != Value::TypeUndefined )
    {
      change[ "column" ] = i;

      nlohmann::json jsonValueOld = valueToJSON( valueOld );
      nlohmann::json jsonValueNew = valueToJSON( valueNew );

      if ( !jsonValueOld.empty() )
      {
        if ( jsonValueOld == "null" )
          change[ "old" ] = nullptr;
        else
          change[ "old" ] = jsonValueOld;
      }
      if ( !jsonValueNew.empty() )
      {
        if ( jsonValueNew == "null" )
          change[ "new" ] = nullptr;
        else
          change[ "new" ] = jsonValueNew;
      }

      entries.push_back( change );
    }
  }

  res[ "changes" ] = entries;
  return res;
}

static nlohmann::json columnInfoToJSON( const TableColumnInfo &column )
{
  nlohmann::json res;
  res["name"] = column.name;
  res["type"] = column.type.dbType;
  res["isPrimaryKey"] = column.isPrimaryKey;
  res["isNotNull"] = column.isNotNull;
  res["isAutoIncrement"] = column.isAutoIncrement;
  res["isGeometry"] = column.isGeometry;
  res["geomType"] = column.geomType;
  res["geomSrsId"] = column.geomSrsId;
  res["geomHasZ"] = column.geomHasZ;
  res["geomHasM"] = column.geomHasM;
  return res;
}

nlohmann::json changesetEntryToJSON( const ChangesetEntry &entry )
{
  if ( const ChangesetDataEntry *dataEntry = std::get_if<ChangesetDataEntry>( &entry ) )
  {
    return changesetDataEntryToJSON( *dataEntry );
  }
  else if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
  {
    nlohmann::json res;
    res["type"] = "create_table";
    res["tableName"] = ctEntry->tableName;
    res["columns"] = nlohmann::json::array();
    for ( const TableColumnInfo &column : ctEntry->columns )
    {
      res["columns"].push_back( columnInfoToJSON( column ) );
    }
    return res;
  }
  else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
  {
    nlohmann::json res;
    res["type"] = "drop_table";
    res["tableName"] = dtEntry->tableName;
    res["columns"] = nlohmann::json::array();
    for ( const TableColumnInfo &column : dtEntry->columns )
    {
      res["columns"].push_back( columnInfoToJSON( column ) );
    }
    return res;
  }
  else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
  {
    nlohmann::json res;
    res["type"] = "add_column";
    res["tableName"] = acEntry->tableName;
    res["column"] = columnInfoToJSON( acEntry->column );
    return res;
  }
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    nlohmann::json res;
    res["type"] = "drop_column";
    res["tableName"] = dcEntry->tableName;
    res["column"] = columnInfoToJSON( dcEntry->column );
    return res;
  }
  else
  {
    throw GeoDiffException( "Cannot convert entry variant " + std::to_string( entry.index() ) + " to JSON" );
  }
}

nlohmann::json changesetToJSON( ChangesetReader &reader )
{
  auto entries = nlohmann::json::array();

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    nlohmann::json msg = changesetEntryToJSON( entry );
    if ( msg.empty() )
      continue;

    entries.push_back( msg );
  }

  nlohmann::json res;
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

nlohmann::json changesetToJSONSummary( ChangesetReader &reader )
{
  std::map< std::string, TableSummary > summary;

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( !std::holds_alternative<ChangesetDataEntry>( entry ) )
      continue;
    ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );
    std::string tableName = dataEntry.table->name;
    TableSummary &tableSummary = summary[tableName];

    if ( dataEntry.op == ChangesetDataEntry::OpUpdate )
      ++tableSummary.updates;
    else if ( dataEntry.op == ChangesetDataEntry::OpInsert )
      ++tableSummary.inserts;
    else if ( dataEntry.op == ChangesetDataEntry::OpDelete )
      ++tableSummary.deletes;
  }

  // write JSON
  auto entries = nlohmann::json::array();
  for ( const auto &kv : summary )
  {
    nlohmann::json tableJson;
    tableJson[ "table" ] = kv.first;
    tableJson[ "insert" ] = kv.second.inserts;
    tableJson[ "update" ] = kv.second.updates;
    tableJson[ "delete" ] = kv.second.deletes;

    entries.push_back( tableJson );
  }
  nlohmann::json res;
  res[ "geodiff_summary" ] = entries;
  return res;
}

nlohmann::json conflictToJSON( const ConflictFeature &conflict )
{
  nlohmann::json res;
  res[ "table" ] = std::string( conflict.tableName() );
  res[ "type" ] = "conflict";
  res[ "fid" ] = std::to_string( conflict.pk() );

  auto entries = nlohmann::json::array();

  const std::vector<ConflictItem> items = conflict.items();
  for ( const ConflictItem &item : items )
  {
    nlohmann::json change;
    change[ "column" ] = item.column();

    nlohmann::json valueBase = valueToJSON( item.base() );
    nlohmann::json valueOld = valueToJSON( item.theirs() );
    nlohmann::json valueNew = valueToJSON( item.ours() );

    if ( !valueBase.empty() )
    {
      if ( valueBase == "null" )
        change[ "base" ] = nullptr;
      else
        change[ "base" ] = valueBase;
    }
    if ( !valueOld.empty() )
    {
      if ( valueOld == "null" )
        change[ "old" ] = nullptr;
      else
        change[ "old" ] = valueOld;
    }
    if ( !valueNew.empty() )
    {
      if ( valueNew == "null" )
        change[ "new" ] = nullptr;
      else
        change[ "new" ] = valueNew;
    }

    entries.push_back( change );
  }
  res[ "changes" ] = entries;
  return res;
}

nlohmann::json conflictsToJSON( const std::vector<ConflictFeature> &conflicts )
{
  auto entries = nlohmann::json::array();
  for ( const ConflictFeature &item : conflicts )
  {
    nlohmann::json msg = conflictToJSON( item );
    if ( msg.empty() )
      continue;

    entries.push_back( msg );
  }

  nlohmann::json res;
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
