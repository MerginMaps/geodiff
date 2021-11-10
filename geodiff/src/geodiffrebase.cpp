/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffrebase.hpp"
#include "geodiffutils.hpp"
#include "geodiff.h"
#include "geodifflogger.hpp"

#include "changesetreader.h"
#include "changesetwriter.h"

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
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>


/**
 * structure that keeps track of information needed for rebase extracted
 * from the original changeset (for a single table)
 */
struct TableRebaseInfo
{
  std::set<int> inserted;           //!< pkeys that were inserted
  std::set<int> deleted;            //!< pkeys that were deleted
  std::map<int, std::vector<Value> > updated;  //!< new column values for each recorded row (identified by pkey)

  void dump_set( const std::set<int> &data, std::ostringstream &ret )
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

  void dump()
  {
    if ( Logger::instance().maxLogLevel() != GEODIFF_LoggerLevel::LevelDebug )
      return;

    std::ostringstream ret;
    ret << "rebase info (base2their / old)" << std::endl;
    for ( auto it : tables )
    {
      ret << "TABLE " << it.first << std::endl;
      it.second.dump( ret );
    }

    Logger::instance().debug( ret.str() );
  }
};


//! structure that keeps track of how we modify primary keys of the rebased changeset
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

  void dump() const
  {
    if ( Logger::instance().maxLogLevel() != GEODIFF_LoggerLevel::LevelDebug )
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

    Logger::instance().debug( ret.str() );
  }

};

///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////


int _get_primary_key( const ChangesetEntry &entry )
{
  int fid;
  int nFidColumn;
  get_primary_key( entry, fid, nFidColumn );
  return fid;
}


int _parse_old_changeset( ChangesetReader &reader_BASE_THEIRS, DatabaseRebaseInfo &dbInfo )
{
  ChangesetEntry entry;
  while ( reader_BASE_THEIRS.nextEntry( entry ) )
  {
    int pk = _get_primary_key( entry );

    TableRebaseInfo &tableInfo = dbInfo.tables[entry.table->name];

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      tableInfo.inserted.insert( pk );
    }
    if ( entry.op == ChangesetEntry::OpDelete )
    {
      tableInfo.deleted.insert( pk );
    }
    if ( entry.op == ChangesetEntry::OpUpdate )
    {
      tableInfo.updated[pk] = entry.newValues;
    }
  }

  dbInfo.dump();

  return GEODIFF_SUCCESS;
}

int _find_mapping_for_new_changeset( ChangesetReader &reader, const DatabaseRebaseInfo &dbInfo,
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
    std::string tableName = entry.table->name;
    auto tableIt = dbInfo.tables.find( tableName );
    if ( tableIt == dbInfo.tables.end() )
      continue;  // this table is not in our records at all - no rebasing needed

    const TableRebaseInfo &tableInfo = tableIt->second;

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      int pk = _get_primary_key( entry );

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
    else if ( entry.op == ChangesetEntry::OpUpdate )
    {
      int pk = _get_primary_key( entry );

      if ( tableInfo.deleted.find( pk ) != tableInfo.deleted.end() )
      {
        // update on deleted feature...
        mapping.addPkeyMapping( tableName, pk, RebaseMapping::INVALID_FID );
      }
    }
    else if ( entry.op == ChangesetEntry::OpDelete )
    {
      int pk = _get_primary_key( entry );

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

  mapping.dump();

  return GEODIFF_SUCCESS;
}


bool _handle_insert( const ChangesetEntry &entry, const RebaseMapping &mapping, ChangesetEntry &outEntry )
{
  size_t numColumns = entry.table->columnCount();

  outEntry.op = ChangesetEntry::OpInsert;
  outEntry.newValues.resize( numColumns );

  // resolve primary key and patched primary key
  int pk = _get_primary_key( entry );
  int newPk = pk;

  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = mapping.getNewPkey( entry.table->name, pk );
  }

  for ( size_t i = 0; i < numColumns; i++ )
  {
    if ( entry.table->primaryKeys[i] )
    {
      outEntry.newValues[i].setInt( newPk );
    }
    else
    {
      outEntry.newValues[i] = entry.newValues[i];
    }
  }
  return true;
}

bool _handle_delete( const ChangesetEntry &entry, const RebaseMapping &mapping,
                     const TableRebaseInfo &tableInfo, ChangesetEntry &outEntry )
{
  size_t numColumns = entry.table->columnCount();

  outEntry.op = ChangesetEntry::OpDelete;
  outEntry.oldValues.resize( numColumns );

  // resolve primary key and patched primary key
  int pk = _get_primary_key( entry );
  int newPk = pk;

  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = mapping.getNewPkey( entry.table->name, pk );

    // conflict 2 concurrent deletes...
    if ( newPk == RebaseMapping::INVALID_FID )
      return false;
  }

  // find the previously new values (will be used as the old values in the rebased version)
  std::vector<Value> patchedVals;
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    patchedVals.resize( static_cast<size_t>( numColumns ) );
  else
    patchedVals = a->second;

  for ( size_t i = 0; i < numColumns; i++ )
  {
    if ( entry.table->primaryKeys[i] )
    {
      outEntry.oldValues[i].setInt( newPk );
    }
    else
    {
      // if the value was patched in the previous commit, use that one as base
      Value value;
      const Value &patchedVal = patchedVals[i];
      if ( patchedVal.type() != Value::TypeUndefined )
      {
        value = patchedVal;
      }
      else
      {
        // otherwise the value is same for both patched and this, so use base value
        value = entry.oldValues[i];
      }
      outEntry.oldValues[i] = value;
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

bool _handle_update( const ChangesetEntry &entry, const RebaseMapping &mapping,
                     const TableRebaseInfo &tableInfo, ChangesetEntry &outEntry,
                     std::vector<ConflictFeature> &conflicts )
{
  size_t numColumns = entry.table->columnCount();

  outEntry.op = ChangesetEntry::OpUpdate;
  outEntry.oldValues.resize( numColumns );
  outEntry.newValues.resize( numColumns );

  // get values from patched (new) master
  int pk = _get_primary_key( entry );
  if ( mapping.hasOldPkey( entry.table->name, pk ) )
  {
    int newPk = mapping.getNewPkey( entry.table->name, pk );
    if ( newPk == RebaseMapping::INVALID_FID )
      return false;
  }

  // find the previously new values (will be used as the old values in the rebased version)
  std::vector<Value> patchedVals;
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    patchedVals.resize( static_cast<size_t>( numColumns ) );
  else
    patchedVals = a->second;

  ConflictFeature conflictFeature( pk, entry.table->name );

  bool entryHasChanges = false;
  for ( size_t i = 0; i < numColumns; i++ )
  {
    Value patchedVal = patchedVals[i];
    if ( patchedVal.type() != Value::TypeUndefined && entry.newValues[i].type() != Value::TypeUndefined )
    {
      if ( patchedVal == entry.newValues[i] )
      {
        // both "old" and "new" changeset modify the column's value to the same value - that
        // means that in our rebased changeset there's no further change and there's no conflict
        outEntry.oldValues[i].setUndefined();
        outEntry.newValues[i].setUndefined();
      }
      else
      {
        // we have edit conflict here: both "old" changeset and the "new" changeset modify the same
        // column of the same row. Rebased changeset will get the "old" value updated to the new (patched)
        // value of the older changeset
        outEntry.oldValues[i] = patchedVal;
        outEntry.newValues[i] = entry.newValues[i];
        entryHasChanges = true;
        _addConflictItem( conflictFeature, i, entry.oldValues[i], patchedVal, entry.newValues[i] );
      }
    }
    else
    {
      // the "new" changeset stays as is without modifications
      outEntry.oldValues[i] = entry.oldValues[i];
      outEntry.newValues[i] = entry.newValues[i];
      // if a column is pkey, it would have "new" value undefined in the entry and that's not an actual change
      if ( entry.newValues[i].type() != Value::TypeUndefined )
        entryHasChanges = true;
    }
  }

  if ( conflictFeature.isValid() )
  {
    conflicts.push_back( conflictFeature );
  }
  return entryHasChanges;
}

int _prepare_new_changeset( ChangesetReader &reader, const std::string &changesetNew,
                            const RebaseMapping &mapping, const DatabaseRebaseInfo &dbInfo,
                            std::vector<ConflictFeature> &conflicts )
{
  ChangesetEntry entry;
  std::map<std::string, ChangesetTable> tableDefinitions;
  std::map<std::string, std::vector<ChangesetEntry> > tableChanges;
  while ( reader.nextEntry( entry ) )
  {
    std::string tableName = entry.table->name;
    if ( tableDefinitions.find( tableName ) == tableDefinitions.end() )
    {
      tableDefinitions[tableName] = *entry.table;
    }

    auto tablesIt = dbInfo.tables.find( tableName );
    if ( tablesIt == dbInfo.tables.end() )
    {
      // we have change in different table that was modified in theirs modifications
      // just copy plain the change to the output buffer
      tableChanges[tableName].push_back( entry );
      continue;
    }

    bool writeEntry = false;
    ChangesetEntry outEntry;

    // commits to same table -> now save the change to changeset
    switch ( entry.op )
    {
      case ChangesetEntry::OpUpdate:
        writeEntry = _handle_update( entry, mapping, tablesIt->second, outEntry, conflicts );
        break;

      case ChangesetEntry::OpInsert:
        writeEntry = _handle_insert( entry, mapping, outEntry );
        break;

      case ChangesetEntry::OpDelete:
        writeEntry = _handle_delete( entry, mapping, tablesIt->second, outEntry );
        break;
    }

    if ( writeEntry )
      tableChanges[tableName].push_back( outEntry );
  }

  ChangesetWriter writer;
  if ( !writer.open( changesetNew ) )
  {
    std::cout << "unable to open file for writing " << changesetNew << std::endl;
    return GEODIFF_ERROR;
  }

  for ( auto it : tableDefinitions )
  {
    auto chit = tableChanges.find( it.first );
    if ( chit == tableChanges.end() )
      continue;

    const std::vector<ChangesetEntry> &changes = chit->second;
    if ( changes.empty() )
      continue;

    writer.beginTable( it.second );
    for ( const ChangesetEntry &entry : changes )
    {
      writer.writeEntry( entry );
    }
  }

  return GEODIFF_SUCCESS;
}

int rebase( const std::string &changeset_BASE_THEIRS,
            const std::string &changeset_THEIRS_MODIFIED,
            const std::string &changeset_BASE_MODIFIED,
            std::vector<ConflictFeature> &conflicts )

{
  fileremove( changeset_THEIRS_MODIFIED );

  ChangesetReader reader_BASE_THEIRS;
  if ( !reader_BASE_THEIRS.open( changeset_BASE_THEIRS ) )
  {
    Logger::instance().error( "Could not open changeset_BASE_THEIRS: " + changeset_BASE_THEIRS );
    return GEODIFF_ERROR;
  }
  if ( reader_BASE_THEIRS.isEmpty() )
  {
    Logger::instance().info( " -- no rebase needed! (empty base2theirs) --\n" );
    filecopy( changeset_BASE_MODIFIED, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  ChangesetReader reader_BASE_MODIFIED;
  if ( !reader_BASE_MODIFIED.open( changeset_BASE_MODIFIED ) )
  {
    Logger::instance().error( "Could not open changeset_BASE_MODIFIED: " + changeset_BASE_MODIFIED );
    return GEODIFF_ERROR;
  }
  if ( reader_BASE_MODIFIED.isEmpty() )
  {
    Logger::instance().info( " -- no rebase needed! (empty base2modified) --\n" );
    filecopy( changeset_BASE_THEIRS, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  // 1. go through the original changeset and extract data that will be needed in the second step
  DatabaseRebaseInfo dbInfo;
  int rc = _parse_old_changeset( reader_BASE_THEIRS, dbInfo );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  // 2. go through the changeset to be rebased and figure out changes we will need to do to it
  RebaseMapping mapping;
  rc = _find_mapping_for_new_changeset( reader_BASE_MODIFIED, dbInfo, mapping );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  reader_BASE_MODIFIED.rewind();

  // 3. go through the changeset to be rebased again and write it with changes determined in step 2
  rc = _prepare_new_changeset( reader_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, dbInfo, conflicts );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  return GEODIFF_SUCCESS;
}
