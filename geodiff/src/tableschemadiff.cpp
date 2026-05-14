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
    schema.columns.push_back( acEntry->column );
  }
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
  {
    auto it = std::find_if( schema.columns.begin(), schema.columns.end(),
    [&]( const TableColumnInfo & c ) { return c.name == dcEntry->column.name; } );
    if ( it == schema.columns.end() )
      throw GeoDiffException( "Tried simulating deletion of non-existent column " + dcEntry->column.name );
    schema.columns.erase( it );
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

  std::vector<ChangesetEntry> entries;

  const std::unordered_map<std::string, const TableColumnInfo *> baseColumns = byName( base.columns );
  const std::unordered_map<std::string, const TableColumnInfo *> modifiedColumns = byName( modified.columns );
  const std::vector<std::string> baseColNames = names( base.columns );
  const std::vector<std::string> modifiedColNames = names( modified.columns );

  std::vector<std::string> deletedColNames;
  std::set_difference( baseColNames.begin(), baseColNames.end(),
                       modifiedColNames.begin(), modifiedColNames.end(),
                       std::back_inserter( deletedColNames ) );
  for ( const std::string &colName : deletedColNames )
  {
    entries.push_back( ChangesetDropColumnEntry{base.name, *baseColumns.at( colName )} );
  }

  std::vector<std::string> newColNames;
  std::set_difference( modifiedColNames.begin(), modifiedColNames.end(),
                       baseColNames.begin(), baseColNames.end(),
                       std::back_inserter( newColNames ) );
  for ( const std::string &colName : newColNames )
  {
    entries.push_back( ChangesetAddColumnEntry{base.name, *modifiedColumns.at( colName )} );
  }

  std::vector<std::string> oldColNames;
  std::set_intersection( modifiedColNames.begin(), modifiedColNames.end(),
                         baseColNames.begin(), baseColNames.end(),
                         std::back_inserter( oldColNames ) );
  for ( const std::string &colName : oldColNames )
  {
    // Compare column type by base type enum rather than the exact db-specific
    // string to avoid regression with DB pairs that use compatible types.
    if ( !baseColumns.at( colName )->compareWithBaseTypes( *modifiedColumns.at( colName ) ) )
      throw GeoDiffException( "Columns differ: " +
                              base.name + "." + colName + " and " + modified.name + "." + colName + ")" );
  }

  return entries;
}

std::vector<ChangesetEntry> diffDatabaseSchema( const DatabaseSchema &base, const DatabaseSchema &modified )
{
  std::vector<ChangesetEntry> entries;

  const std::unordered_map<std::string, const TableSchema *> baseTables = byName( base.tables );
  const std::unordered_map<std::string, const TableSchema *> modifiedTables = byName( modified.tables );
  const std::vector<std::string> baseTableNames = names( base.tables );
  const std::vector<std::string> modifiedTableNames = names( modified.tables );

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
