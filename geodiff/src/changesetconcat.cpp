/*
 GEODIFF - MIT License
 Copyright (C) 2021 Martin Dobias
*/

#include "sqlite3.h"

#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"


//! Hash value generator based on primary keys to have ChangesetEntry used in std::unordered_set
struct HashChangesetEntryPkey
{
  size_t operator()( const ChangesetEntry *pentry ) const
  {
    size_t h = 0;
    const ChangesetEntry &entry = *pentry;
    const std::vector<bool> &pkeys = entry.table->primaryKeys;
    const std::vector<Value> &values = entry.op == ChangesetEntry::OpInsert ? entry.newValues : entry.oldValues;
    for ( size_t i = 0; i < pkeys.size(); ++i )
    {
      if ( pkeys[i] )
        h ^= std::hash<Value> {}( values[i] );
    }
    return h;
  }
};


//! Exact equality check based on primary keys to have ChangesetEntry used in std::unordered_set
struct EqualToChangesetEntryPkey
{
  bool operator()( const ChangesetEntry *plhs, const ChangesetEntry *prhs ) const
  {
    const ChangesetEntry &lhs = *plhs;
    const ChangesetEntry &rhs = *prhs;
    const std::vector<bool> &pkeys = lhs.table->primaryKeys;
    const std::vector<Value> &lhsValues = lhs.op == ChangesetEntry::OpInsert ? lhs.newValues : lhs.oldValues;
    const std::vector<Value> &rhsValues = rhs.op == ChangesetEntry::OpInsert ? rhs.newValues : rhs.oldValues;
    for ( size_t i = 0; i < pkeys.size(); ++i )
    {
      if ( pkeys[i] && lhsValues[i] != rhsValues[i] )
        return false;
    }
    return true;
  }
};

typedef std::unordered_set<ChangesetEntry *, HashChangesetEntryPkey, EqualToChangesetEntryPkey> TableEntriesSet;

//! Struct to keep information about table and its changes while concatenating
struct TableChanges
{
  std::unique_ptr<ChangesetTable> table;
  TableEntriesSet entries;
};


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
  const std::vector<Value> valuesOld1, const std::vector<Value> valuesOld2,
  const std::vector<Value> valuesNew1, const std::vector<Value> valuesNew2,
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
static MergeEntriesResult mergeEntriesForRow( ChangesetEntry *e1, ChangesetEntry *e2 )
{
  // all these changes make no sense really, if they happen most likely something got broken
  // (e.g. adding a row with the same pkey twice)
  if ( ( e1->op == ChangesetEntry::OpInsert && e2->op == ChangesetEntry::OpInsert ) ||
       ( e1->op == ChangesetEntry::OpUpdate && e2->op == ChangesetEntry::OpInsert ) ||
       ( e1->op == ChangesetEntry::OpDelete && e2->op == ChangesetEntry::OpUpdate ) ||
       ( e1->op == ChangesetEntry::OpDelete && e2->op == ChangesetEntry::OpDelete ) )
    return Unsupported;

  if ( e1->op == ChangesetEntry::OpInsert && e2->op == ChangesetEntry::OpDelete )
    return EntryRemoved;

  if ( e1->op == ChangesetEntry::OpInsert && e2->op == ChangesetEntry::OpUpdate )
  {
    // modify INSERT - update its values wherever the update has a newer value
    for ( size_t i = 0; i < e1->table->columnCount(); ++i )
    {
      if ( e2->newValues[i].type() != Value::TypeUndefined )
        e1->newValues[i] = e2->newValues[i];
    }
    return EntryModified;
  }

  if ( e1->op == ChangesetEntry::OpUpdate && e2->op == ChangesetEntry::OpUpdate )
  {
    // modify UPDATE
    std::vector<Value> oldVals, newVals;
    if ( !mergeUpdate( *e1->table, e2->oldValues, e1->oldValues, e1->newValues, e2->newValues, oldVals, newVals ) )
      return EntryRemoved;
    e1->oldValues = oldVals;
    e1->newValues = newVals;
    return EntryModified;
  }

  if ( e1->op == ChangesetEntry::OpUpdate && e2->op == ChangesetEntry::OpDelete )
  {
    // turn into DELETE, use old values from delete when update does not list them
    e1->op = ChangesetEntry::OpDelete;
    for ( size_t i = 0; i < e1->table->columnCount(); ++i )
    {
      if ( e1->oldValues[i].type() == Value::TypeUndefined )
        e1->oldValues[i] = e2->oldValues[i];
    }
    return EntryModified;
  }

  if ( e1->op == ChangesetEntry::OpDelete && e2->op == ChangesetEntry::OpInsert )
  {
    // turn into UPDATE
    std::vector<Value> oldVals, newVals;
    if ( !mergeUpdate( *e1->table, e1->oldValues, {}, e2->newValues, {}, oldVals, newVals ) )
      return EntryRemoved;
    e1->op = ChangesetEntry::OpUpdate;
    e1->oldValues = oldVals;
    e1->newValues = newVals;
    return EntryModified;
  }

  assert( false ); // all 9 possible cases are exhausted
  return Unsupported;
}


//! Concatenation of multiple changesets, based on the implementation from sqlite3session
//! (functions sqlite3changegroup_add() and sqlite3changegroup_output())
void concatChangesets( const std::vector<std::string> &filenames, const std::string &outputChangeset )
{
  // hashtable: table name -> ( fid -> changeset entry )
  std::unordered_map<std::string, TableChanges> result;

  for ( const std::string &inputFilename : filenames )
  {
    ChangesetReader reader;
    if ( !reader.open( inputFilename ) )
      throw GeoDiffException( "concatChangesets: unable to open input file: " + inputFilename );

    ChangesetEntry entry;
    while ( reader.nextEntry( entry ) )
    {
      auto tableIt = result.find( entry.table->name );
      if ( tableIt == result.end() )
      {
        TableChanges &t = result[ entry.table->name ];   // adds new entry
        t.table.reset( new ChangesetTable( *entry.table ) );
        ChangesetEntry *e = new ChangesetEntry( entry );
        e->table = t.table.get();
        t.entries.insert( e );
      }
      else
      {
        TableChanges &t = tableIt->second;
        auto entriesIt = t.entries.find( &entry );
        if ( entriesIt == t.entries.end() )
        {
          // row with this pkey is not in our list yet
          ChangesetEntry *e = new ChangesetEntry( entry );
          e->table = t.table.get();
          t.entries.insert( e );
        }
        else
        {
          // we need to merge the recorded entry with the new one
          ChangesetEntry *entry0 = *entriesIt;
          MergeEntriesResult mergeRes = mergeEntriesForRow( entry0, &entry );
          switch ( mergeRes )
          {
            case EntryModified:
              break;   // nothing else to do - the original entry got updated in place
            case EntryRemoved:
              t.entries.erase( entriesIt );
              delete entry0;
              break;
            case Unsupported:
              // we are discarding the new entry (there's no sensible way to integrate it)
              Logger::instance().warn( "concatChangesets: unsupported sequence of entries for a single row - discarding newer entry" );
              delete entry0;
              break;
          }
        }
      }
    }
  }

  ChangesetWriter writer;
  if ( !writer.open( outputChangeset ) )
    throw GeoDiffException( "concatChangesets: unable to open output file: " + outputChangeset );

  // output all we have captured
  for ( auto it = result.begin(); it != result.end(); ++it )
  {
    TableChanges &t = it->second;
    if ( t.entries.size() == 0 )
      continue;

    writer.beginTable( *t.table );
    for ( ChangesetEntry *e : t.entries )
    {
      writer.writeEntry( *e );
      delete e;
    }
  }
}
