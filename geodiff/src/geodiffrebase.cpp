/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffrebase.hpp"
#include "geodiffutils.hpp"
#include "geodiff.h"

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

  void dump_set( const std::set<int> &data, const std::string &name, std::ostringstream &ret )
  {
    ret << name << std::endl;
    if ( inserted.empty() )
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
    dump_set( inserted, "inserted", ret );
    dump_set( deleted, "deleted", ret );
    // TODO: dump updated too
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
    if ( Logger::instance().level() < Logger::LevelDebug )
      return;

    std::ostringstream ret;
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
    if ( Logger::instance().level() < Logger::LevelDebug )
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
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    throw GeoDiffException( "internal error: _get_new SqOperations" );
  SqValues patchedVals = a->second;

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

void _handle_update(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const RebaseMapping &mapping,
  const TableRebaseInfo &tableInfo,
  unsigned char *aiFlg,
  std::shared_ptr<BinaryStream> out
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
  auto a = tableInfo.updated.find( pk );
  if ( a == tableInfo.updated.end() )
    throw GeoDiffException( "internal error: _get_new SqOperations" );
  SqValues patchedVals = a->second;

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

}

int _prepare_new_changeset( const Buffer &buf, const std::string &changesetNew, const RebaseMapping &mapping, const DatabaseRebaseInfo &dbInfo )
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
    // now save the change to changeset
    switch ( pOp )
    {
      case SQLITE_UPDATE:
      {
        auto tablesIt = dbInfo.tables.find( pzTab );
        if ( tablesIt == dbInfo.tables.end() )
          throw GeoDiffException( "internal error in _get_new SqOperations" );

        _handle_update(
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
        auto tablesIt = dbInfo.tables.find( pzTab );
        if ( tablesIt == dbInfo.tables.end() )
          throw GeoDiffException( "internal error in _get_new SqOperations" );

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

  // join buffers to one file (ugly ugly)
  FILE *out = fopen( changesetNew.c_str(), "wb" );
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
            const std::string &changeset_BASE_MODIFIED )

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
  _parse_old_changeset( buf_BASE_THEIRS, dbInfo );

  // 2. go through the changeset to be rebased and figure out changes we will need to do to it
  RebaseMapping mapping;
  _find_mapping_for_new_changeset( buf_BASE_MODIFIED, dbInfo, mapping );

  // 3. go through the changeset to be rebased again and write it with changes determined in step 2
  _prepare_new_changeset( buf_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, dbInfo );

  return GEODIFF_SUCCESS;
}
