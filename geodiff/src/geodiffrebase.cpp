/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffrebase.hpp"
#include "changeset.h"
#include "geodiffutils.hpp"
#include "geodiff.h"
#include "geodifflogger.hpp"
#include "geodiffcontext.hpp"

#include "changesetreader.h"
#include "changesetwriter.h"
#include "changesetutils.h"
#include "tableschema.h"
#include "tableschemadiff.hpp"

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>

static void dump_set( const std::set<int> &data, std::ostringstream &ret )
{
  if ( data.empty() )
    ret << "--none --";
  else
  {
    for ( auto it : data )
    {
      ret << it << ",";
    }
  }
  ret << std::endl;
}

/**
 * structure that keeps track of information needed for rebase extracted
 * from the original changeset (for a single table)
 */
struct TableRebaseInfo
{
  std::set<int> inserted;           //!< pkeys that were inserted
  std::set<int> deleted;            //!< pkeys that were deleted
  std::map<int, std::map<std::string, Value>> updated;  //!< new column values (by name) for each updated row (identified by pkey)

  void dump( std::ostringstream &ret )
  {
    ret << "  inserted ";
    dump_set( inserted, ret );
    ret << "  deleted  ";
    dump_set( deleted, ret );
    ret << "  updated  ";
    std::set<int> updatedPkeys;
    for ( auto pk : updated )
      updatedPkeys.insert( pk.first );
    dump_set( updatedPkeys, ret );
  }
};

/**
 * structure that keeps track of information needed for rebase extracted
 * from the original changeset (for the whole database)
 */
struct DatabaseRebaseInfo
{
  std::map<std::string, TableRebaseInfo> tables;   //!< mapping for each table (key = table name)
  DatabaseSchema theirSchema;

  void dump( const Context *context )
  {
    if ( context->logger().maxLogLevel() != GEODIFF_LoggerLevel::LevelDebug )
      return;

    std::ostringstream ret;
    ret << "rebase info (base2their / old)" << std::endl;
    for ( auto it : tables )
    {
      ret << "TABLE " << it.first << std::endl;
      it.second.dump( ret );
    }

    context->logger().debug( ret.str() );
  }
};


//! structure that keeps track of how we modify primary keys and column indices
//  of the rebased changeset.
struct RebaseMapping
{

  // table name -> old fid --> new fid
  std::map<std::string, std::map < int, int > > mapIds;

  // table name -> set of fids of inserts that have been untouched
  // (this is important because our mapping could cause FID conflicts
  // with FIDs that weren't previously in conflict, e.g. if 4,5,6
  // get mapped 4->6, 5->7 then the original 6 will need to be remapped
  // too: 6->8)
  std::map<std::string, std::set<int> > unmappedInsertIds;

  // special pkey value for deleted rows
  static const int INVALID_FID = -1;

  void addPkeyMapping( const std::string &table, int id, int id2 )
  {
    auto ids = mapIds.find( table );
    if ( ids == mapIds.end() )
    {
      std::map < int, int  > newSet;
      newSet.insert( std::pair<int, int>( id, id2 ) );
      mapIds[table] = newSet;
    }
    else
    {
      std::map < int, int  > &oldSet = ids->second;
      oldSet.insert( std::pair<int, int>( id, id2 ) );
    }
  }

  bool hasOldPkey( const std::string &table, int id ) const
  {
    auto mapId = mapIds.find( table );
    if ( mapId == mapIds.end() )
    {
      return false;
    }
    else
    {
      const std::map < int, int  > &ids = mapId->second;
      auto fid =  ids.find( id );
      return fid != ids.end();
    }
  }

  int getNewPkey( const std::string &table, int id ) const
  {
    auto ids = mapIds.find( table );
    if ( ids == mapIds.end() )
    {
      throw GeoDiffException( "internal error: _get_new MappingIds" );
    }
    else
    {
      const std::map < int, int  > &oldSet = ids->second;
      auto a = oldSet.find( id );
      if ( a == oldSet.end() )
        throw GeoDiffException( "internal error: _get_new MappingIds" );
      return a->second;
    }
  }

  void dump( const Context *context ) const
  {
    if ( context->logger().maxLogLevel() != GEODIFF_LoggerLevel::LevelDebug )
      return;

    std::ostringstream ret;

    ret << "mapping" << std::endl;
    if ( mapIds.empty() )
      ret << "--none -- " << std::endl;

    for ( auto it : mapIds )
    {
      ret << "  " << it.first << std::endl << "    ";
      if ( it.second.empty() )
        ret << "--none -- ";
      for ( auto it2 : it.second )
      {
        ret << it2.first << "->" << it2.second << ",";
      }
      ret << std::endl;
    }

    context->logger().debug( ret.str() );
  }

};

///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////


int _get_primary_key( const ChangesetDataEntry &entry )
{
  int fid;
  int nFidColumn;
  get_primary_key( entry, fid, nFidColumn );
  return fid;
}


int _parse_old_changeset(
  const Context *context,
  const DatabaseSchema &baseSchema,
  ChangesetReader &reader_BASE_THEIRS,
  DatabaseRebaseInfo &dbInfo )
{
  dbInfo.theirSchema = baseSchema;

  ChangesetEntry entry;
  while ( reader_BASE_THEIRS.nextEntry( entry ) )
  {
    if ( std::holds_alternative<ChangesetDataEntry>( entry ) )
    {
      ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );

      std::string tableName = dataEntry.table->name;

      // skip table if necessary
      if ( context->isTableSkipped( tableName ) )
      {
        continue;
      }

      int pk = _get_primary_key( dataEntry );

      TableRebaseInfo &tableInfo = dbInfo.tables[tableName];

      if ( dataEntry.op == ChangesetDataEntry::OpInsert )
      {
        tableInfo.inserted.insert( pk );
      }
      if ( dataEntry.op == ChangesetDataEntry::OpDelete )
      {
        tableInfo.deleted.insert( pk );
      }
      if ( dataEntry.op == ChangesetDataEntry::OpUpdate )
      {
        const TableSchema *ts = dbInfo.theirSchema.tableByName( tableName );
        if ( !ts )
          throw GeoDiffException( "Update entry for table not in schema: " + tableName );
        std::map<std::string, Value> namedVals;
        for ( size_t i = 0; i < dataEntry.newValues.size() && i < ts->columns.size(); i++ )
        {
          if ( dataEntry.newValues[i].type() != Value::TypeUndefined )
            namedVals[ts->columns[i].name] = dataEntry.newValues[i];
        }
        tableInfo.updated[pk] = std::move( namedVals );
      }
    }
    else if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
    {
      if ( context->isTableSkipped( ctEntry->tableName ) )
        continue;
      simulateSchemaChange( dbInfo.theirSchema, entry );
    }
    else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
    {
      if ( context->isTableSkipped( dtEntry->tableName ) )
        continue;
      simulateSchemaChange( dbInfo.theirSchema, entry );
    }
    else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
    {
      if ( context->isTableSkipped( acEntry->tableName ) )
        continue;
      simulateSchemaChange( dbInfo.theirSchema, entry );
    }
    else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
    {
      if ( context->isTableSkipped( dcEntry->tableName ) )
        continue;
      simulateSchemaChange( dbInfo.theirSchema, entry );
    }
    else
    {
      throw GeoDiffException( "Unhandled entry type in rebase: " + std::to_string( entry.index() ) );
    }
  }

  dbInfo.dump( context );

  return GEODIFF_SUCCESS;
}

int _find_mapping_for_new_changeset(
  const Context *context,
  ChangesetReader &reader,
  const DatabaseRebaseInfo &dbInfo,
  RebaseMapping &mapping )
{
  // figure out first free primary key value when rebasing for each table
  // TODO: should we consider all rows in the table instead of just the inserts? (maybe not needed - those were available in the other source too)
  std::map<std::string, int> freeIndices;
  for ( auto mapId : dbInfo.tables )
  {
    const std::set < int > &oldSet = mapId.second.inserted;
    if ( oldSet.empty() )
      continue;  // TODO: or set 0 to free indices??

    freeIndices[mapId.first] = *std::max_element( oldSet.begin(), oldSet.end() ) + 1;
  }

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( !std::holds_alternative<ChangesetDataEntry>( entry ) )
      continue;
    const ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );

    std::string tableName = dataEntry.table->name;

    // skip table if necessary
    if ( context->isTableSkipped( tableName ) )
    {
      continue;
    }

    auto tableIt = dbInfo.tables.find( tableName );
    if ( tableIt == dbInfo.tables.end() )
      continue;  // this table is not in our records at all - no rebasing needed

    const TableRebaseInfo &tableInfo = tableIt->second;

    if ( dataEntry.op == ChangesetDataEntry::OpInsert )
    {
      int pk = _get_primary_key( dataEntry );

      if ( tableInfo.inserted.find( pk ) != tableInfo.inserted.end() )
      {
        // conflict 2 concurrent inserts...
        auto it = freeIndices.find( tableName );
        if ( it == freeIndices.end() )
          throw GeoDiffException( "internal error: freeIndices" );

        mapping.addPkeyMapping( tableName, pk, it->second );

        // increase counter
        it->second ++;
      }
      else
      {
        // keep IDs of inserts later - we may need to remap them too
        mapping.unmappedInsertIds[tableName].insert( pk );
      }
    }
    else if ( dataEntry.op == ChangesetDataEntry::OpUpdate )
    {
      int pk = _get_primary_key( dataEntry );

      if ( tableInfo.deleted.find( pk ) != tableInfo.deleted.end() )
      {
        // update on deleted feature...
        mapping.addPkeyMapping( tableName, pk, RebaseMapping::INVALID_FID );
      }
    }
    else if ( dataEntry.op == ChangesetDataEntry::OpDelete )
    {
      int pk = _get_primary_key( dataEntry );

      if ( tableInfo.deleted.find( pk ) != tableInfo.deleted.end() )
      {
        // delete of deleted feature...
        mapping.addPkeyMapping( tableName, pk, RebaseMapping::INVALID_FID );
      }
    }
  }

  // finalize mapping of inserted features: e.g. if we have newly inserted IDs
  // 4,5,6 where we will get mapping 4->6, 5->7 that conflicts with unmapped IDs
  for ( auto pair : mapping.unmappedInsertIds )
  {
    std::string tableName = pair.first;

    // make a set of all new pkeys
    std::set<int> usedNewPkeys;
    for ( auto oldNewPair : mapping.mapIds[tableName] )
      usedNewPkeys.insert( oldNewPair.second );

    std::set<int> pkeys = pair.second;
    for ( int pk : pkeys )
    {
      if ( usedNewPkeys.find( pk ) != usedNewPkeys.end() )
      {
        // our mapping has previously introduced a new conflict in IDs -> remap this old pkey as well

        auto it = freeIndices.find( tableName );
        if ( it == freeIndices.end() )
          throw GeoDiffException( "internal error: freeIndices (2)" );

        mapping.addPkeyMapping( tableName, pk, it->second );
        usedNewPkeys.insert( it->second );

        // increase counter
        it->second ++;
      }
    }
  }

  mapping.dump( context );

  return GEODIFF_SUCCESS;
}


bool _handle_insert( const ChangesetDataEntry &entry, const RebaseMapping &mapping,
                     const std::map<int, int> &colMap,
                     ChangesetDataEntry &outEntry )
{
  outEntry.op = ChangesetDataEntry::OpInsert;
  outEntry.newValues.resize( outEntry.table->columnCount() );

  // resolve primary key and patched primary key
  int pk = _get_primary_key( entry );
  int newPk = pk;

  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    // conflict 2 concurrent inserts...
    newPk = mapping.getNewPkey( entry.table->name, pk );
  }

  for ( const auto &[inIdx, outIdx] : colMap )
  {
    if ( outEntry.table->primaryKeys[outIdx] )
      outEntry.newValues[outIdx].setInt( newPk );
    else
      outEntry.newValues[outIdx] = entry.newValues[inIdx];
  }
  return true;
}

bool _handle_delete( const ChangesetDataEntry &entry, const RebaseMapping &mapping,
                     const TableRebaseInfo &tableInfo,
                     const std::map<int, int> &colMap,
                     const TableSchema &inTableSchema,
                     ChangesetDataEntry &outEntry )
{
  outEntry.op = ChangesetDataEntry::OpDelete;
  outEntry.oldValues.resize( outEntry.table->columnCount() );

  // resolve primary key and patched primary key
  int pk = _get_primary_key( entry );
  int newPk = pk;

  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    // conflict 2 concurrent deletes...
    newPk = mapping.getNewPkey( entry.table->name, pk );
    if ( newPk == RebaseMapping::INVALID_FID )
      return false;
  }

  // find the previously new values (will be used as the old values in the rebased version)
  const std::map<std::string, Value> *patchedMap = nullptr;
  auto a = tableInfo.updated.find( pk );
  if ( a != tableInfo.updated.end() )
    patchedMap = &a->second;

  for ( const auto &[inIdx, outIdx] : colMap )
  {
    if ( outEntry.table->primaryKeys[outIdx] )
    {
      outEntry.oldValues[outIdx].setInt( newPk );
    }
    else
    {
      // if the value was patched in the previous commit, use that one as base
      Value patchedVal;
      if ( patchedMap )
      {
        auto it = patchedMap->find( inTableSchema.columns[inIdx].name );
        if ( it != patchedMap->end() )
          patchedVal = it->second;
      }
      if ( patchedVal.type() != Value::TypeUndefined )
        outEntry.oldValues[outIdx] = patchedVal;
      else
        // otherwise the value is same for both patched and this, so use base value
        outEntry.oldValues[outIdx] = entry.oldValues[inIdx];
    }
  }
  return true;
}

void _addConflictItem( ConflictFeature &conflictFeature, int i,
                       const Value &base, const Value &theirs, const Value &ours )
{
  // 4th attribute in gpkg_contents is modified date
  // this is not a conflict since we can sort it out
  if ( ( conflictFeature.tableName() == "gpkg_contents" ) && ( i == 4 ) )
    return;

  // ok safe to add it
  ConflictItem item( i, base, theirs, ours );
  conflictFeature.addItem( item );
}

bool _handle_update( const ChangesetDataEntry &entry, const RebaseMapping &mapping,
                     const TableRebaseInfo &tableInfo,
                     const std::map<int, int> &colMap,
                     const TableSchema &inTableSchema,
                     ChangesetDataEntry &outEntry,
                     std::vector<ConflictFeature> &conflicts )
{
  outEntry.op = ChangesetDataEntry::OpUpdate;
  outEntry.oldValues.resize( outEntry.table->columnCount() );
  outEntry.newValues.resize( outEntry.table->columnCount() );

  // get values from patched (new) master
  int pk = _get_primary_key( entry );
  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    int newPk = mapping.getNewPkey( entry.table->name, pk );
    if ( newPk == RebaseMapping::INVALID_FID )
    {
      // our UPDATE conflicts with their DELETE: record as conflict, delete wins
      ConflictFeature conflictFeature( pk, entry.table->name );
      for ( size_t i = 0; i < numColumns; i++ )
      {
        if ( entry.newValues[i].type() != Value::TypeUndefined )
        {
          _addConflictItem( conflictFeature, ( int ) i, entry.oldValues[i], Value(), entry.newValues[i] );
        }
      }
      if ( conflictFeature.isValid() )
        conflicts.push_back( conflictFeature );
      return false;
    }
  }

  // find the previously new values (will be used as the old values in the rebased version)
  const std::map<std::string, Value> *patchedMap = nullptr;
  auto a = tableInfo.updated.find( pk );
  if ( a != tableInfo.updated.end() )
    patchedMap = &a->second;

  ConflictFeature conflictFeature( pk, entry.table->name );

  bool entryHasChanges = false;
  for ( const auto &[inIdx, outIdx] : colMap )
  {
    Value patchedVal;
    if ( patchedMap )
    {
      auto it = patchedMap->find( inTableSchema.columns[inIdx].name );
      if ( it != patchedMap->end() )
        patchedVal = it->second;
    }

    if ( patchedVal.type() != Value::TypeUndefined && entry.newValues[inIdx].type() != Value::TypeUndefined )
    {
      if ( patchedVal == entry.newValues[inIdx] )
      {
        // both "old" and "new" changeset modify the column's value to the same value - that
        // means that in our rebased changeset there's no further change and there's no conflict
        outEntry.oldValues[outIdx].setUndefined();
        outEntry.newValues[outIdx].setUndefined();
      }
      else
      {
        // we have edit conflict here: both "old" changeset and the "new" changeset modify the same
        // column of the same row. Rebased changeset will get the "old" value updated to the new (patched)
        // value of the older changeset
        outEntry.oldValues[outIdx] = patchedVal;
        outEntry.newValues[outIdx] = entry.newValues[inIdx];
        entryHasChanges = true;
        _addConflictItem( conflictFeature, outIdx, entry.oldValues[inIdx], patchedVal, entry.newValues[inIdx] );
      }
    }
    else
    {
      // the "new" changeset stays as is without modifications
      outEntry.oldValues[outIdx] = entry.oldValues[inIdx];
      outEntry.newValues[outIdx] = entry.newValues[inIdx];
      // if a column is pkey, it would have "new" value undefined in the entry and that's not an actual change
      if ( entry.newValues[inIdx].type() != Value::TypeUndefined )
        entryHasChanges = true;
    }
  }

  if ( conflictFeature.isValid() )
    conflicts.push_back( conflictFeature );
  return entryHasChanges;
}

//! throws GeoDiffException on error
void _prepare_new_changeset( const Context *context,
                             ChangesetReader &reader, const std::string &changesetNew,
                             RebaseMapping &mapping, const DatabaseRebaseInfo &dbInfo,
                             const DatabaseSchema &baseSchema,
                             std::vector<ConflictFeature> &conflicts )
{
  // The base DB schema with our changes from already processed entries applied
  // on top.
  DatabaseSchema currentSchema = baseSchema;
  // The base DB schema with our their changes and then ourchanges from already
  // processed entries applied on top.
  DatabaseSchema outputSchema = dbInfo.theirSchema;
  // table schema -> (old column index -> new column index)
  // Column being absent means its index didn't change.
  std::map<ChangesetTable *, std::map<int, int>> columnIndexMap;

  std::map<std::string, std::vector<ChangesetEntry> > tableChanges;

  // Cached output ChangesetTable for the current table.
  std::shared_ptr<ChangesetTable> outChangesetTable;

  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( std::holds_alternative<ChangesetDataEntry>( entry ) )
    {
      ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );
      std::string tableName = dataEntry.table->name;

      // skip table if necessary
      if ( context->isTableSkipped( tableName ) )
        continue;

      TableSchema *tableSchema = currentSchema.tableByName( tableName );

      if ( !tableSchema )
        throw GeoDiffException( "Tried rebasing data entry for table not in schema: " + tableName );

      // Get the output table schema (theirs + our schema changes so far).
      TableSchema *outTableSchema = outputSchema.tableByName( tableName );
      if ( !outTableSchema )
        // Table was dropped by theirs.
        continue;

      // Compute column mapping (input index -> output index) on first encounter.
      if ( columnIndexMap.find( dataEntry.table.get() ) == columnIndexMap.end() )
      {
        std::map<int, int> colMap;
        for ( size_t i = 0; i < tableSchema->columns.size(); i++ )
        {
          const std::string &colName = tableSchema->columns[i].name;
          for ( size_t j = 0; j < outTableSchema->columns.size(); j++ )
          {
            if ( outTableSchema->columns[j].name == colName )
            {
              colMap[static_cast<int>( i )] = static_cast<int>( j );
              break;
            }
          }
        }
        columnIndexMap[dataEntry.table.get()] = std::move( colMap );
      }

      const std::map<int, int> &colMap = columnIndexMap[dataEntry.table.get()];

      // Rebuild cached output ChangesetTable when the table name changes.
      if ( !outChangesetTable || outChangesetTable->name != tableName )
        outChangesetTable = std::make_shared<ChangesetTable>( schemaToChangesetTable( tableName, *outTableSchema ) );

      auto tablesIt = dbInfo.tables.find( tableName );
      if ( tablesIt == dbInfo.tables.end() )
      {
        // Table not touched by theirs data-wise - copy through as-is.
        tableChanges[tableName].push_back( entry );
        continue;
      }

      bool writeEntry = false;
      ChangesetDataEntry outEntry;
      outEntry.table = outChangesetTable;

      // commits to same table -> now save the change to changeset
      switch ( dataEntry.op )
      {
        case ChangesetDataEntry::OpUpdate:
          writeEntry = _handle_update( dataEntry, mapping, tablesIt->second, colMap, *tableSchema, outEntry, conflicts );
          break;

        case ChangesetDataEntry::OpInsert:
          writeEntry = _handle_insert( dataEntry, mapping, colMap, outEntry );
          break;

        case ChangesetDataEntry::OpDelete:
          writeEntry = _handle_delete( dataEntry, mapping, tablesIt->second, colMap, *tableSchema, outEntry );
          break;
      }

      if ( writeEntry )
        tableChanges[tableName].push_back( outEntry );
    }
    else
    {
      simulateSchemaChange( currentSchema, entry );
      outChangesetTable = nullptr; // Invalidate cached schema, columns may change

      // Check whether the same change is already contained in theirs. If not,
      // add it to the output.
      bool isDuplicate = false;
      std::string schemaEntryTableName;
      if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
      {
        schemaEntryTableName = ctEntry->tableName;
        const TableSchema *existing = outputSchema.tableByName( ctEntry->tableName );
        if ( existing )
        {
          if ( existing->columns != ctEntry->columns )
            throw GeoDiffException( "Conflict: table " + ctEntry->tableName +
                                    " was created by both changesets with different columns" );
          isDuplicate = true;
        }
      }
      else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
      {
        schemaEntryTableName = dtEntry->tableName;
        isDuplicate = outputSchema.tableByName( dtEntry->tableName ) == nullptr;
      }
      else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
      {
        schemaEntryTableName = acEntry->tableName;
        TableSchema *table = outputSchema.tableByName( acEntry->tableName );
        if ( table )
        {
          auto it = std::find_if( table->columns.begin(), table->columns.end(),
          [&]( const TableColumnInfo & c ) { return c.name == acEntry->column.name; } );
          if ( it != table->columns.end() )
          {
            if ( *it != acEntry->column )
              throw GeoDiffException( "During rebase, column " + acEntry->tableName + "." + acEntry->column.name +
                                      " was added by both changesets with different definitions" );
            isDuplicate = true;
          }
        }
        else
          throw GeoDiffException( " During rebase tried to add column "  + acEntry->tableName + "." + acEntry->column.name + " to non-existent table" );
      }
      else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
      {
        schemaEntryTableName = dcEntry->tableName;
        TableSchema *table = outputSchema.tableByName( dcEntry->tableName );
        if ( table )
        {
          auto it = std::find_if( table->columns.begin(), table->columns.end(),
          [&]( const TableColumnInfo & c ) { return c.name == dcEntry->column.name; } );
          isDuplicate = it == table->columns.end();
        }
        else
          throw GeoDiffException( " During rebase tried to drop column "  + dcEntry->tableName + "." + dcEntry->column.name + " from non-existent table" );
      }

      if ( !isDuplicate )
      {
        simulateSchemaChange( outputSchema, entry );
        tableChanges[schemaEntryTableName].push_back( entry );
      }
    }
  }

  ChangesetWriter writer;
  writer.open( changesetNew );

  for ( const auto &it : tableChanges )
  {
    const std::vector<ChangesetEntry> &changes = it.second;
    if ( changes.empty() )
      continue;

    const ChangesetTable *defWritten = nullptr;
    for ( const ChangesetEntry &writeEntry : changes )
    {
      if ( auto dataEntry = std::get_if<ChangesetDataEntry>( &writeEntry ) )
      {
        if ( defWritten != dataEntry->table.get() )
        {
          writer.beginTable( *dataEntry->table );
          defWritten = dataEntry->table.get();
        }
      }
      writer.writeEntry( writeEntry );
    }
  }
}

void rebase(
  const Context *context,
  const DatabaseSchema &baseSchema,
  const std::string &changeset_BASE_THEIRS,
  const std::string &changeset_THEIRS_MODIFIED,
  const std::string &changeset_BASE_MODIFIED,
  std::vector<ConflictFeature> &conflicts )

{
  fileremove( changeset_THEIRS_MODIFIED );

  ChangesetReader reader_BASE_THEIRS;
  if ( !reader_BASE_THEIRS.open( changeset_BASE_THEIRS ) )
  {
    throw GeoDiffException( "Could not open changeset_BASE_THEIRS: " + changeset_BASE_THEIRS );
  }
  if ( reader_BASE_THEIRS.isEmpty() )
  {
    context->logger().info( " -- no rebase needed! (empty base2theirs) --\n" );
    filecopy( changeset_BASE_MODIFIED, changeset_THEIRS_MODIFIED );
    return;
  }

  ChangesetReader reader_BASE_MODIFIED;
  if ( !reader_BASE_MODIFIED.open( changeset_BASE_MODIFIED ) )
  {
    throw GeoDiffException( "Could not open changeset_BASE_MODIFIED: " + changeset_BASE_MODIFIED );
  }
  if ( reader_BASE_MODIFIED.isEmpty() )
  {
    context->logger().info( " -- no rebase needed! (empty base2modified) --\n" );
    filecopy( changeset_BASE_THEIRS, changeset_THEIRS_MODIFIED );
    return;
  }

  // 1. go through the original changeset and extract data that will be needed in the second step
  DatabaseRebaseInfo dbInfo;
  int rc = _parse_old_changeset( context, baseSchema, reader_BASE_THEIRS, dbInfo );
  if ( rc != GEODIFF_SUCCESS )
    throw GeoDiffException( "Could not parse changeset_BASE_THEIRS: " + changeset_BASE_THEIRS );

  // 2. go through the changeset to be rebased and figure out changes we will need to do to it
  RebaseMapping mapping;
  rc = _find_mapping_for_new_changeset( context, reader_BASE_MODIFIED, dbInfo, mapping );
  if ( rc != GEODIFF_SUCCESS )
    throw GeoDiffException( "Could not figure out changes for rebase" );

  reader_BASE_MODIFIED.rewind();

  // 3. go through the changeset to be rebased again and write it with changes determined in step 2
  _prepare_new_changeset( context, reader_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, dbInfo, baseSchema, conflicts );
}
