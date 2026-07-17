/*
 GEODIFF - MIT License
 Copyright (C) 2021 Martin Dobias
*/

#include "changeset.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

#include "geodifflogger.hpp"
#include "geodiffcontext.hpp"
#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"


struct ValueVectorHash
{
  size_t operator()( const std::vector<Value> &values ) const
  {
    size_t h = 0;
    for ( size_t i = 0; i < values.size(); ++i )
      h ^= std::hash<Value> {}( values[i] );
    return h;
  }
};

static std::vector<Value> entryPkey( const ChangesetDataEntry &entry )
{
  const std::vector<bool> &pkeys = entry.table->primaryKeys;
  const std::vector<Value> &values = entry.op == ChangesetDataEntry::OpInsert ? entry.newValues : entry.oldValues;
  std::vector<Value> pkeyValues;
  for ( size_t i = 0; i < values.size(); ++i )
  {
    if ( pkeys[i] )
      pkeyValues.push_back( values[i] );
  }
  return pkeyValues;
}


// primary key values -> data entry index in entries list
typedef std::unordered_map<std::vector<Value>, size_t, ValueVectorHash> TableEntriesMap;

//! Struct to keep information about table and its changes while concatenating
struct TableChanges
{
  // List of entries affecting this table. Wrapped in optional so we can do
  // in-place O(1) deletions.
  std::vector<std::optional<ChangesetEntry>> entries;
  // Entries output at the start. Used for column additions.
  std::vector<ChangesetEntry> prefixEntries;
  TableEntriesMap dataEntries;
};

// Output of concatenation is divided into phases, where entries can be freely
// merged.
// Indexed by table name.
typedef std::unordered_map<std::string, TableChanges> OutputPhase;


//! This is a helper function used by mergeUpdate().
static Value mergeValue( const Value &vOne, const Value &vTwo )
{
  return vTwo.type() != Value::TypeUndefined ? vTwo : vOne;
}


//! This function is used to merge two UPDATE changes on the same row.
//! Returns false if the two updates cancel each other and the resulting
//! changeset entry can be discarded.
static bool mergeUpdate(
  const ChangesetTable &t,
  const std::vector<Value> &valuesOld1, const std::vector<Value> &valuesOld2,
  const std::vector<Value> &valuesNew1, const std::vector<Value> &valuesNew2,
  std::vector<Value> &outputOld, std::vector<Value> &outputNew )
{
  bool bRequired = false;

  for ( size_t i = 0; i < t.columnCount(); ++i )
  {
    Value vOld = mergeValue( valuesOld1[i], valuesOld2.size() ? valuesOld2[i] : Value() );
    Value vNew = mergeValue( valuesNew1[i], valuesNew2.size() ? valuesNew2[i] : Value() );

    // if there would be no actual changes after the merge, we would discard the merged update...
    if ( vOld != vNew && !t.primaryKeys[i] )
      bRequired = true;

    // write OLD
    if ( t.primaryKeys[i] || vOld != vNew )
    {
      outputOld.push_back( vOld );
    }
    else
    {
      outputOld.push_back( Value() );
    }

    // write NEW
    if ( t.primaryKeys[i] || vOld == vNew )
    {
      outputNew.push_back( Value() );
    }
    else
    {
      outputNew.push_back( vNew );
    }
  }

  return bRequired;
}


//! Possible outcomes of merging two changeset entries
enum MergeEntriesResult
{
  EntryModified,   //!< the entry got updated within the merge (INSERT+UPDATE, UPDATE+UPDATE, UPDATE+DELETE, DELETE+INSERT)
  EntryRemoved,    //!< the entry should be removed after the merge (INSERT+DELETE)
  Unsupported,     //!< unexpected combination (INSERT+INSERT, UPDATE+INSERT, DELETE+UPDATE, DELETE+DELETE)
};

//! Takes two changeset entries e1 and e2 and merges their changes to e1 if possible.
//! It is also possible that merging results in no change at all, or the change is not allowed
static MergeEntriesResult mergeEntriesForRow( ChangesetDataEntry &e1, const ChangesetDataEntry &e2 )
{
  // all these changes make no sense really, if they happen most likely something got broken
  // (e.g. adding a row with the same pkey twice)
  if ( ( e1.op == ChangesetDataEntry::OpInsert && e2.op == ChangesetDataEntry::OpInsert ) ||
       ( e1.op == ChangesetDataEntry::OpUpdate && e2.op == ChangesetDataEntry::OpInsert ) ||
       ( e1.op == ChangesetDataEntry::OpDelete && e2.op == ChangesetDataEntry::OpUpdate ) ||
       ( e1.op == ChangesetDataEntry::OpDelete && e2.op == ChangesetDataEntry::OpDelete ) )
    return Unsupported;

  if ( e1.op == ChangesetDataEntry::OpInsert && e2.op == ChangesetDataEntry::OpDelete )
    return EntryRemoved;

  if ( e1.op == ChangesetDataEntry::OpInsert && e2.op == ChangesetDataEntry::OpUpdate )
  {
    // modify INSERT - update its values wherever the update has a newer value
    for ( size_t i = 0; i < e1.table->columnCount(); ++i )
    {
      if ( e2.newValues[i].type() != Value::TypeUndefined )
        e1.newValues[i] = e2.newValues[i];
    }
    return EntryModified;
  }

  if ( e1.op == ChangesetDataEntry::OpUpdate && e2.op == ChangesetDataEntry::OpUpdate )
  {
    // modify UPDATE
    std::vector<Value> oldVals, newVals;
    if ( !mergeUpdate( *e1.table, e2.oldValues, e1.oldValues, e1.newValues, e2.newValues, oldVals, newVals ) )
      return EntryRemoved;
    e1.oldValues = oldVals;
    e1.newValues = newVals;
    return EntryModified;
  }

  if ( e1.op == ChangesetDataEntry::OpUpdate && e2.op == ChangesetDataEntry::OpDelete )
  {
    // turn into DELETE, use old values from delete when update does not list them
    e1.op = ChangesetDataEntry::OpDelete;
    for ( size_t i = 0; i < e1.table->columnCount(); ++i )
    {
      if ( e1.oldValues[i].type() == Value::TypeUndefined )
        e1.oldValues[i] = e2.oldValues[i];
    }
    return EntryModified;
  }

  if ( e1.op == ChangesetDataEntry::OpDelete && e2.op == ChangesetDataEntry::OpInsert )
  {
    // turn into UPDATE
    std::vector<Value> oldVals, newVals;
    if ( !mergeUpdate( *e1.table, e1.oldValues, {}, e2.newValues, {}, oldVals, newVals ) )
      return EntryRemoved;
    e1.op = ChangesetDataEntry::OpUpdate;
    e1.oldValues = oldVals;
    e1.newValues = newVals;
    return EntryModified;
  }

  assert( false ); // all 9 possible cases are exhausted
  return Unsupported;
}


//! Concatenation of multiple changesets, based on the implementation from sqlite3session
//! (functions sqlite3changegroup_add() and sqlite3changegroup_output())
void concatChangesets(
  const Context *context,
  const std::vector<std::string> &filenames,
  const std::string &outputChangeset )
{
  std::vector<OutputPhase> outputPhases = {{}};

  for ( const std::string &inputFilename : filenames )
  {
    ChangesetReader reader;
    if ( !reader.open( inputFilename ) )
      throw GeoDiffException( "concatChangesets: unable to open input file: " + inputFilename );

    ChangesetEntry fullEntry;
    while ( reader.nextEntry( fullEntry ) )
    {
      OutputPhase &phase = outputPhases.back();

      if ( ChangesetDataEntry *dEntry = std::get_if<ChangesetDataEntry>( &fullEntry ) )
      {
        TableChanges &t = phase[dEntry->table->name];
        auto entriesIt = t.dataEntries.find( entryPkey( *dEntry ) );
        if ( entriesIt == t.dataEntries.end() )
        {
          // row with this pkey is not in our list yet
          t.entries.push_back( *dEntry );
          t.dataEntries[entryPkey( *dEntry )] = t.entries.size() - 1;
        }
        else
        {
          // we need to merge the recorded entry with the new one
          ChangesetDataEntry &entry0 = std::get<ChangesetDataEntry>( *t.entries[entriesIt->second] );
          MergeEntriesResult mergeRes = mergeEntriesForRow( entry0, *dEntry );
          switch ( mergeRes )
          {
            case EntryModified:
              break;   // nothing else to do - the original entry got updated in place
            case EntryRemoved:
              t.entries[ entriesIt->second ] = std::nullopt;
              t.dataEntries.erase( entriesIt );
              break;
            case Unsupported:
              // we are discarding the new entry (there's no sensible way to integrate it)
              context->logger().warn( "concatChangesets: unsupported sequence of entries for a single row - discarding newer entry" );
              t.entries[ entriesIt->second ] = std::nullopt;
              t.dataEntries.erase( entriesIt );
              break;
          }
        }
      }
      else if ( ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &fullEntry ) )
      {
        phase[dtEntry->tableName].entries.push_back( *dtEntry );
      }
      else if ( ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &fullEntry ) )
      {
        // This entry only contains the column's name, not its index, so we
        // can't apply its effects to the existing entries. The best we can do
        // is just forward this entry.
        phase[dcEntry->tableName].entries.push_back( *dcEntry );
        // We also need to start a new phase, since we can't merge entries
        // anymore.
        outputPhases.push_back( {{}} );
      }
      else if ( ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &fullEntry ) )
      {
        phase[ctEntry->tableName].entries = { *ctEntry };
      }
      else if ( ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &fullEntry ) )
      {
        phase[acEntry->tableName].prefixEntries.push_back( *acEntry );
        // Add the column to all existing entries, since we pushed the column
        // addition in front of them.
        size_t newColumnCount = SIZE_MAX;
        for ( auto &existingEntry : phase[acEntry->tableName].entries )
        {
          if ( !existingEntry ) continue;
          ChangesetDataEntry *existingDEntry = std::get_if<ChangesetDataEntry>( &*existingEntry );
          if ( !existingDEntry ) continue;
          if ( newColumnCount == SIZE_MAX )
            newColumnCount = existingDEntry->table->columnCount() + 1;
          if ( existingDEntry->table->columnCount() != newColumnCount )
            existingDEntry->table->primaryKeys.push_back( false );
          if ( existingDEntry->oldValues.size() != newColumnCount )
            existingDEntry->oldValues.push_back( Value::makeNull() );
          if ( existingDEntry->newValues.size() != newColumnCount )
            existingDEntry->newValues.push_back( Value::makeNull() );
        }
      }
      else
        throw GeoDiffException( "concatChanges: unhandled entry " + std::to_string( fullEntry.index() ) );
    }
  }

  ChangesetWriter writer;
  writer.open( outputChangeset );

  // output all we have captured
  for ( const OutputPhase &outPhase : outputPhases )
  {
    for ( auto it = outPhase.begin(); it != outPhase.end(); ++it )
    {
      const TableChanges &t = it->second;

      for ( const ChangesetEntry &e : t.prefixEntries )
      {
        writer.writeEntry( e );
      }

      std::shared_ptr<ChangesetTable> writtenSchema;
      for ( const std::optional<ChangesetEntry> &e : t.entries )
      {
        if ( e )
        {
          if ( const ChangesetDataEntry *dEntry = std::get_if<ChangesetDataEntry>( &*e ) )
          {
            if ( dEntry->table != writtenSchema )
            {
              writer.beginTable( *dEntry->table );
              writtenSchema = dEntry->table;
            }
          }
          writer.writeEntry( *e );
        }
      }
    }
  }
}
