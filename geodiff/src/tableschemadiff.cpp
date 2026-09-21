/*
 GEODIFF - MIT License
 Copyright (C) 2026 David Koňařík
*/

#include "tableschemadiff.hpp"
#include "changeset.h"
#include "geodiffutils.hpp"
#include "tableschema.h"
#include <algorithm>
#include <iterator>
#include <unordered_map>

template <typename T>
static std::vector<std::string> names( const std::vector<T> &items )
{
  std::vector<std::string> names;
  names.reserve( items.size() );
  for ( const auto &item : items )
  {
    names.push_back( item.name );
  }
  return names;
}

template std::vector<std::string> names( const std::vector<TableColumnInfo> &items );

template <typename T>
static std::unordered_map<std::string, const T *> byName( const std::vector<T> &items )
{
  std::unordered_map<std::string, const T *> map;
  for ( const T &item : items )
  {
    map[item.name] = &item;
  }
  return map;
}

void simulateColumnChange( TableSchema &schema, const ChangesetEntry &entry )
{
  if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
  {
    auto it = std::find_if( schema.columns.begin(), schema.columns.end(),
    [&]( const TableColumnInfo & c ) { return c.name == acEntry->column.name; } );
    if ( it != schema.columns.end() )
      throw GeoDiffException( "Tried simulating addition of already-existing column " + acEntry->column.name );
    // The new column may be appended, so the index may be one past the end
    if ( acEntry->columnIdx > schema.columns.size() )
      throw GeoDiffException( "Tried simulating addition of column beyond end of table: " + acEntry->column.name + ", idx=" + std::to_string( acEntry->columnIdx ) );
    schema.columns.insert( schema.columns.begin() + acEntry->columnIdx, acEntry->column );
  }
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    if ( dcEntry->columnIdx >= schema.columns.size() )
      throw GeoDiffException( "Tried simulating deletion of column beyond end of table: " + dcEntry->column.name + ", idx=" + std::to_string( dcEntry->columnIdx ) );
    const TableColumnInfo &column = schema.columns[dcEntry->columnIdx];
    if ( column.name != dcEntry->column.name )
      throw GeoDiffException( "Tried simulating deletion of column not matching name: is " + column.name + ", requested " + dcEntry->column.name );
    schema.columns.erase( schema.columns.begin() + dcEntry->columnIdx );
  }
}

void simulateSchemaChange( DatabaseSchema &schema, const ChangesetEntry &entry )
{
  if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
  {
    if ( schema.tableByName( ctEntry->tableName ) )
      throw GeoDiffException( "Tried simulating creation of already-existing table " + ctEntry->tableName );
    TableSchema ts;
    ts.name = ctEntry->tableName;
    ts.columns = ctEntry->columns;
    schema.tables.push_back( ts );
  }
  else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
  {
    auto it = std::find_if( schema.tables.begin(), schema.tables.end(),
    [&]( const TableSchema & t ) { return t.name == dtEntry->tableName; } );
    if ( it == schema.tables.end() )
      throw GeoDiffException( "Tried simulating deletion of non-existent table " + dtEntry->tableName );
    schema.tables.erase( it );
  }
  else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
  {
    TableSchema *table = schema.tableByName( acEntry->tableName );
    if ( !table )
      throw GeoDiffException( "Tried to add column " + acEntry->column.name + " to non-existent table " + acEntry->tableName );
    simulateColumnChange( *table, entry );
  }
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    TableSchema *table = schema.tableByName( dcEntry->tableName );
    if ( !table )
      throw GeoDiffException( "Tried to delete column " + dcEntry->column.name + " from non-existent table " + dcEntry->tableName );
    simulateColumnChange( *table, entry );
  }
}

std::vector<ChangesetEntry> diffTableSchema( const TableSchema &base, const TableSchema &modified )
{
  if ( base.crs != modified.crs )
    throw GeoDiffException( "Tried to compare tables with different CRSs (named" +
                            base.name + " and " + modified.name + ")" );

  auto modifiedColsByName = byName( modified.columns );

  std::vector<ChangesetEntry> entries;

  // This basic loop finds some list of column schema changes to transform
  // base's column list to modified's, indexes included. The result won't
  // necessarily be optimal WRT number of operations (we'd need some edit
  // distance algorithm).
  // Column indexes in the entries are relative to the intermediate schema, i.e.
  // the one produced by applying the entries emitted so far.
  size_t baseIdx = 0;
  size_t modifiedIdx = 0;
  while ( true )
  {
    if ( baseIdx == base.columns.size() && modifiedIdx == modified.columns.size() )
      break; // Processed all columns

    if ( baseIdx == base.columns.size() )
    {
      // Processed all base columns, every column now is a new one from modified.
      const TableColumnInfo &modifiedCol = modified.columns[modifiedIdx];
      entries.push_back( ChangesetAddColumnEntry{base.name, modifiedIdx, modifiedCol} );
      modifiedIdx++;
      continue;
    }

    if ( modifiedIdx == modified.columns.size() )
    {
      // Processed all modified columns, every column now was deleted in modified.
      const TableColumnInfo &baseCol = base.columns[baseIdx];
      entries.push_back( ChangesetDropColumnEntry{base.name, modifiedIdx, baseCol} );
      baseIdx++;
      continue;
    }

    const TableColumnInfo &baseCol = base.columns[baseIdx];
    const TableColumnInfo &modifiedCol = modified.columns[modifiedIdx];
    if ( baseCol.name == modifiedCol.name )
    {
      if ( baseCol.compareWithBaseTypes( modifiedCol ) )
      {
        // Columns match, skip over them
        baseIdx++;
        modifiedIdx++;
        continue;
      }
      else
        // Columns share a name, but differ in type. We can't do anything about that.
        throw GeoDiffException( "Columns differ: " +
                                base.name + "." + baseCol.name + " and " + modified.name + "." + modifiedCol.name );
    }
    else
    {
      if ( modifiedColsByName.count( baseCol.name ) )
      {
        // Modified has some new column, add it
        entries.push_back( ChangesetAddColumnEntry{base.name, modifiedIdx, modifiedCol} );
        modifiedIdx++;
        continue;
      }
      else
      {
        // Modified deleted this column, drop it
        entries.push_back( ChangesetDropColumnEntry{base.name, modifiedIdx, baseCol} );
        baseIdx++;
        continue;
      }
    }
  }

  return entries;
}

std::vector<ChangesetEntry> diffDatabaseSchema( const DatabaseSchema &base, const DatabaseSchema &modified )
{
  std::vector<ChangesetEntry> entries;

  const std::unordered_map<std::string, const TableSchema *> baseTables = byName( base.tables );
  const std::unordered_map<std::string, const TableSchema *> modifiedTables = byName( modified.tables );
  std::vector<std::string> baseTableNames = names( base.tables );
  std::vector<std::string> modifiedTableNames = names( modified.tables );
  std::sort( baseTableNames.begin(), baseTableNames.end() );
  std::sort( modifiedTableNames.begin(), modifiedTableNames.end() );

  std::vector<std::string> deletedTableNames;
  std::set_difference( baseTableNames.begin(), baseTableNames.end(),
                       modifiedTableNames.begin(), modifiedTableNames.end(),
                       std::back_inserter( deletedTableNames ) );
  for ( const std::string &name : deletedTableNames )
  {
    entries.push_back( ChangesetDropTableEntry{name, baseTables.at( name )->columns} );
  }

  std::vector<std::string> newTableNames;
  std::set_difference( modifiedTableNames.begin(), modifiedTableNames.end(),
                       baseTableNames.begin(), baseTableNames.end(),
                       std::back_inserter( newTableNames ) );
  for ( const std::string &name : newTableNames )
  {
    entries.push_back( ChangesetCreateTableEntry{name, modifiedTables.at( name )->columns} );
  }

  std::vector<std::string> oldTableNames;
  std::set_intersection( modifiedTableNames.begin(), modifiedTableNames.end(),
                         baseTableNames.begin(), baseTableNames.end(),
                         std::back_inserter( oldTableNames ) );
  for ( const std::string &name : oldTableNames )
  {
    std::vector<ChangesetEntry> tableEntries = diffTableSchema( *baseTables.at( name ), *modifiedTables.at( name ) );
    entries.insert( entries.end(), tableEntries.begin(), tableEntries.end() );
  }

  return entries;
}
