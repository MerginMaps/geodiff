/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffrebase.hpp"
#include "geodiffutils.hpp"
#include "geodiff.h"
#include "geodifflogger.hpp"

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>

// table name, id -> old values, new values
typedef std::vector<std::shared_ptr<Sqlite3Value>> SqValues;

/**
 * structure that keeps track of information needed for rebase extracted
 * from the original changeset (for a single table)
 */
struct TableRebaseInfo
{
  std::set<int> inserted;           //!< pkeys that were inserted
  std::set<int> deleted;            //!< pkeys that were deleted
  std::map<int, SqValues> updated;  //!< new column values for each recorded row (identified by pkey)

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

int _get_primary_key( Sqlite3ChangesetIter &pp, int pOp )
{
  int fid;
  int nFidColumn;
  get_primary_key( pp, pOp, fid, nFidColumn );
  return fid;
}


int _parse_old_changeset( const Buffer &buf_BASE_THEIRS, DatabaseRebaseInfo &dbInfo )
{
  Sqlite3ChangesetIter pp;
  pp.start( buf_BASE_THEIRS );

  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    int rc;
    const char *pzTab;
    int nCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp.get(),      // Iterator object
           &pzTab,        // OUT: Pointer to table name
           &nCol,         // OUT: Number of columns in table
           &pOp,          // OUT: SQLITE_INSERT, DELETE or UPDATE
           &pbIndirect    // OUT: True for an 'indirect' change
         );

    int pk = _get_primary_key( pp, pOp );

    TableRebaseInfo &tableInfo = dbInfo.tables[pzTab];

    if ( pOp == SQLITE_INSERT )
    {
      tableInfo.inserted.insert( pk );
    }
    if ( pOp == SQLITE_DELETE )
    {
      tableInfo.deleted.insert( pk );
    }
    if ( pOp == SQLITE_UPDATE )
    {
      sqlite3_value *ppValue;
      SqValues newValues( nCol );
      for ( int i = 0; i < nCol; ++i )
      {
        pp.newValue( i, &ppValue );
        newValues[i].reset( new Sqlite3Value( ppValue ) );
      }
      tableInfo.updated[pk] = newValues;
    }
  }

  dbInfo.dump();

  return GEODIFF_SUCCESS;
}

int _find_mapping_for_new_changeset( const Buffer &buf,
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

  Sqlite3ChangesetIter pp;
  pp.start( buf );

  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    int rc;
    const char *pzTab;
    int pnCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp.get(),
           &pzTab,
           &pnCol,
           &pOp,
           &pbIndirect
         );

    auto tableIt = dbInfo.tables.find( pzTab );
    if ( tableIt == dbInfo.tables.end() )
      continue;  // this table is not in our records at all - no rebasing needed

    const TableRebaseInfo &tableInfo = tableIt->second;

    if ( pOp == SQLITE_INSERT )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( tableInfo.inserted.find( pk ) != tableInfo.inserted.end() )
      {
        // conflict 2 concurrent inserts...
        auto it = freeIndices.find( pzTab );
        if ( it == freeIndices.end() )
          throw GeoDiffException( "internal error: freeIndices" );

        mapping.addPkeyMapping( pzTab, pk, it->second );

        // increase counter
        it->second ++;
      }
      else
      {
        // keep IDs of inserts later - we may need to remap them too
        mapping.unmappedInsertIds[pzTab].insert( pk );
      }
    }
    else if ( pOp == SQLITE_UPDATE )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( tableInfo.deleted.find( pk ) != tableInfo.deleted.end() )
      {
        // update on deleted feature...
        mapping.addPkeyMapping( pzTab, pk, RebaseMapping::INVALID_FID );
      }
    }
    else if ( pOp == SQLITE_DELETE )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( tableInfo.deleted.find( pk ) != tableInfo.deleted.end() )
      {
        // delete of deleted feature...
        mapping.addPkeyMapping( pzTab, pk, RebaseMapping::INVALID_FID );
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


void _handle_insert(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const RebaseMapping &mapping,
  unsigned char *aiFlg,
  std::shared_ptr<BinaryStream> out
)
{
  // first write operation type (iType)
  out->put( SQLITE_INSERT );
  out->put( 0 );

  sqlite3_value *value;
  // resolve primary key and patched primary key
  int pk = _get_primary_key( pp, SQLITE_INSERT );
  int newPk = pk;

  if ( mapping.hasOldPkey( tableName, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = mapping.getNewPkey( tableName, pk );
  }

  for ( int i = 0; i < pnCol; i++ )
  {
    if ( aiFlg[i] )
    {
      out->putValue( newPk );
    }
    else
    {
      pp.newValue( i, &value );
      out->putValue( value );
    }
  }
}

void _handle_delete(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const RebaseMapping &mapping,
  const TableRebaseInfo &tableInfo,
  unsigned char *aiFlg,
  std::shared_ptr<BinaryStream> out
)
{
  // resolve primary key and patched primary key
  int pk = _get_primary_key( pp, SQLITE_DELETE );
  int newPk = pk;

  if ( mapping.hasOldPkey( tableName, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = mapping.getNewPkey( tableName, pk );

    // conflict 2 concurrent deletes...
    if ( newPk == RebaseMapping::INVALID_FID )
      return;
  }

  // find the previously new values (will be used as the old values in the rebased version)
  SqValues patchedVals;
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    patchedVals.resize( static_cast<size_t>( pnCol ) );
  else
    patchedVals = a->second;

  // first write operation type (iType)
  out->put( SQLITE_DELETE );
  out->put( 0 );
  sqlite3_value *value;

  for ( int i = 0; i < pnCol; i++ )
  {
    if ( aiFlg[i] )
    {
      out->putValue( newPk );
    }
    else
    {
      // if the value was patched in the previous commit, use that one as base
      std::shared_ptr<Sqlite3Value> patchedVal = patchedVals[i];
      if ( patchedVal && patchedVal->isValid() )
      {
        value = patchedVal->value();
      }
      else
      {
        // otherwise the value is same for both patched and this, so use base value
        pp.oldValue( i, &value );
      }
      out->putValue( value );
    }
  }
}

void _addConflictItem( ConflictFeature &conflictFeature,
                       std::shared_ptr<Sqlite3Value> theirs,
                       Sqlite3ChangesetIter &pp,
                       int i
                     )
{
  // 4th attribute in gpkg_contents is modified date
  // this is not a conflict since we can sort it out
  if ( ( conflictFeature.tableName() == "gpkg_contents" ) && ( i == 4 ) )
    return;

  // ok safe to add it
  sqlite3_value *tmp;
  pp.oldValue( i, &tmp );
  std::shared_ptr<Sqlite3Value> base = std::make_shared<Sqlite3Value>( tmp );
  pp.newValue( i, &tmp );
  std::shared_ptr<Sqlite3Value> ours = std::make_shared<Sqlite3Value>( tmp );
  ConflictItem item( i, base, theirs, ours );
  conflictFeature.addItem( item );
}

void _handle_update(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const RebaseMapping &mapping,
  const TableRebaseInfo &tableInfo,
  unsigned char *aiFlg,
  std::shared_ptr<BinaryStream> out,
  std::vector<ConflictFeature> &conflicts
)
{
  // get values from patched (new) master
  int pk = _get_primary_key( pp, SQLITE_UPDATE );
  if ( mapping.hasOldPkey( tableName, pk ) )
  {
    int newPk = mapping.getNewPkey( tableName, pk );
    if ( newPk == RebaseMapping::INVALID_FID )
      return;
  }

  // find the previously new values (will be used as the old values in the rebased version)
  SqValues patchedVals;
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    patchedVals.resize( static_cast<size_t>( pnCol ) );
  else
    patchedVals = a->second;


  ConflictFeature conflictFeature( pk, tableName );

  // first write operation type (iType)
  out->put( SQLITE_UPDATE );
  out->put( 0 );
  sqlite3_value *value;
  for ( int i = 0; i < pnCol; i++ )
  {
    // if the value was patched in the previous commit, use that one as base
    std::shared_ptr<Sqlite3Value> patchedVal = patchedVals[i];
    if ( patchedVal && patchedVal->isValid() )
    {
      value = patchedVal->value();
      // conflict for this value
      _addConflictItem( conflictFeature, patchedVal, pp, i );
    }
    else
    {
      // otherwise the value is same for both patched and this, so use base value
      pp.oldValue( i, &value );
    }
    out->putValue( value );
  }

  for ( int i = 0; i < pnCol; i++ )
  {
    pp.newValue( i, &value );
    out->putValue( value );
  }

  if ( conflictFeature.isValid() )
  {
    conflicts.push_back( conflictFeature );
  }
}

int _prepare_new_changeset( const Buffer &buf, const std::string &changesetNew,
                            const RebaseMapping &mapping, const DatabaseRebaseInfo &dbInfo,
                            std::vector<ConflictFeature> &conflicts )
{
  Sqlite3ChangesetIter pp;
  pp.start( buf );

  std::map<std::string, std::shared_ptr<BinaryStream> > buffers;
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    int rc;
    const char *pzTab;
    int pnCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp.get(),
           &pzTab,
           &pnCol,
           &pOp,
           &pbIndirect
         );

    unsigned char *aiFlg;
    int nCol;
    rc = sqlite3changeset_pk(
           pp.get(),  /* Iterator object */
           &aiFlg, /* OUT: Array of boolean - true for PK cols */
           &nCol /* OUT: Number of entries in output array */
         );
    if ( rc )
    {
      throw GeoDiffException( "internal error in _prepare_new_changeset: sqlite3changeset_pk" );
    }

    std::shared_ptr<BinaryStream> out;
    auto buffer = buffers.find( pzTab );
    if ( buffer == buffers.end() )
    {
      std::string temp = changesetNew + "_" + std::string( pzTab );
      out = std::make_shared<BinaryStream>( temp, true );
      out->open();
      if ( !out->isValid() )
      {
        std::cout << "unable to open file for writing " << changesetNew << std::endl;
        return GEODIFF_ERROR;
      }
      buffers[pzTab] = out;

      // write header for changeset for this table
      out->put( 'T' );
      out->putsVarint( ( sqlite3_uint64 )nCol );
      for ( int i = 0; i < nCol; i++ ) out->put( aiFlg[i] );
      out->write( pzTab, 1, strlen( pzTab ) );
      out->put( 0 );
    }
    else
    {
      out = buffer->second;
    }

    auto tablesIt = dbInfo.tables.find( pzTab );
    if ( tablesIt == dbInfo.tables.end() )
    {
      // we have change in different table that was modified in theirs modifications
      // just copy plain the change to the output buffer
      out->putChangesetIter( pp, pnCol, pOp );
      continue;
    }

    // commits to same table -> now save the change to changeset
    switch ( pOp )
    {
      case SQLITE_UPDATE:
      {
        _handle_update(
          pp,
          pzTab,
          pnCol,
          mapping,
          tablesIt->second,
          aiFlg,
          out,
          conflicts
        );
        break;
      }
      case SQLITE_INSERT:
      {
        _handle_insert(
          pp,
          pzTab,
          pnCol,
          mapping,
          aiFlg,
          out
        );
        break;
      }
      case SQLITE_DELETE:
      {
        _handle_delete(
          pp,
          pzTab,
          pnCol,
          mapping,
          tablesIt->second,
          aiFlg,
          out
        );
        break;
      }
    }
  }

  // join buffers to one file
  FILE *out = openFile( changesetNew, "wb" );
  if ( !out )
  {
    std::cout << "unable to open file for writing " << changesetNew << std::endl;
    return GEODIFF_ERROR;
  }

  for ( auto it : buffers )
  {
    std::shared_ptr<BinaryStream> buf = it.second;
    if ( buf->appendTo( out ) )
    {
      fclose( out );
      throw GeoDiffException( "unable to store changes for table " + it.first );
    }
  }

  fclose( out );

  return GEODIFF_SUCCESS;
}

int rebase( const std::string &changeset_BASE_THEIRS,
            const std::string &changeset_THEIRS_MODIFIED,
            const std::string &changeset_BASE_MODIFIED,
            std::vector<ConflictFeature> &conflicts )

{
  fileremove( changeset_THEIRS_MODIFIED );

  Buffer buf_BASE_THEIRS;
  buf_BASE_THEIRS.read( changeset_BASE_THEIRS );
  if ( buf_BASE_THEIRS.isEmpty() )
  {
    Logger::instance().info( " -- no rabase needed! --\n" );
    filecopy( changeset_BASE_MODIFIED, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  Buffer buf_BASE_MODIFIED;
  buf_BASE_MODIFIED.read( changeset_BASE_MODIFIED );
  if ( buf_BASE_MODIFIED.isEmpty() )
  {
    Logger::instance().info( " -- no rabase needed! --\n" );
    filecopy( changeset_BASE_THEIRS, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  // 1. go through the original changeset and extract data that will be needed in the second step
  DatabaseRebaseInfo dbInfo;
  int rc = _parse_old_changeset( buf_BASE_THEIRS, dbInfo );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  // 2. go through the changeset to be rebased and figure out changes we will need to do to it
  RebaseMapping mapping;
  rc = _find_mapping_for_new_changeset( buf_BASE_MODIFIED, dbInfo, mapping );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  // 3. go through the changeset to be rebased again and write it with changes determined in step 2
  rc = _prepare_new_changeset( buf_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, dbInfo, conflicts );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  return GEODIFF_SUCCESS;
}

bool concatChangesets( const std::string &A, const std::string &B, const std::string &C, const std::string &out )
{
  Buffer bufA;
  bufA.read( A );

  Buffer bufB;
  bufB.read( B );

  Buffer bufC;
  bufC.read( C );

  if ( bufA.isEmpty() && bufB.isEmpty() && bufC.isEmpty() )
  {
    return true;
  }

  sqlite3_changegroup *pGrp;
  int rc = sqlite3changegroup_new( &pGrp );
  if ( rc == SQLITE_OK ) rc = sqlite3changegroup_add( pGrp, bufA.size(), bufA.v_buf() );
  if ( rc == SQLITE_OK ) rc = sqlite3changegroup_add( pGrp, bufB.size(), bufB.v_buf() );
  if ( rc == SQLITE_OK ) rc = sqlite3changegroup_add( pGrp, bufC.size(), bufC.v_buf() );
  if ( rc == SQLITE_OK )
  {
    int pnOut = 0;
    void *ppOut = nullptr;
    rc = sqlite3changegroup_output( pGrp, &pnOut, &ppOut );
    if ( rc )
    {
      sqlite3changegroup_delete( pGrp );
      return true;
    }

    Buffer bufO;
    bufO.read( pnOut, ppOut );
    bufO.write( out );
  }

  if ( pGrp )
    sqlite3changegroup_delete( pGrp );

  return false;
}

