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

// table name -> ids (deleted or inserted)
typedef std::map<std::string, std::set < int > > MapIds;

bool _contains_id( const MapIds &mapIds, const std::string &table, int id )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    return false;
  }
  else
  {
    auto fid = ids->second.find( id );
    return fid != ids->second.end();
  }
}

void _insert_id( MapIds &mapIds, const std::string &table, int id )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    std::set <int> newSet;
    newSet.insert( id );
    mapIds[table] = newSet;
  }
  else
  {
    std::set < int > &oldSet = ids->second;
    oldSet.insert( id );
  }
}

int _maximum_id( const MapIds &mapIds, const std::string &table )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    return 0;
  }
  else
  {
    const std::set < int > &oldSet = ids->second;
    return *std::max_element( oldSet.begin(), oldSet.end() );
  }
}

void _print_idmap( MapIds &mapIds, const std::string &name )
{
  if ( Logger::instance().level() < Logger::LevelDebug )
    return;

  std::ostringstream ret;
  std::cout << name << std::endl;
  if ( mapIds.empty() )
    ret << "--none -- ";

  for ( auto it : mapIds )
  {
    ret << "  " << it.first << std::endl << "    ";
    if ( it.second.empty() )
      ret << "--none -- ";
    for ( auto it2 : it.second )
    {
      ret << it2 << ",";
    }
    ret << std::endl;
  }
  Logger::instance().debug( ret.str() );
}

// table name -> old fid --> new fid
const int INVALID_FID = -1;
typedef std::map<std::string, std::map < int, int  > > MappingIds;

bool _contains_old( const MappingIds &mapIds, const std::string &table, int id )
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

void _insert_ids( MappingIds &mapIds, const std::string &table, int id, int id2 )
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

int _get_new( const MappingIds &mapIds, const std::string &table, int id )
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

void _print_idmap( const MappingIds &mapIds, const std::string &name )
{
  if ( Logger::instance().level() < Logger::LevelDebug )
    return;

  std::ostringstream ret;

  ret << name << std::endl;
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

// table name, id -> old values, new values
typedef std::vector<std::shared_ptr<Sqlite3Value>> SqValues;
typedef std::map < int, std::pair<SqValues, SqValues> > SqValuesMap;
typedef std::map<std::string,  SqValuesMap> SqOperations;

bool _contains( const SqOperations &operations, const std::string &table, int id )
{
  auto mapId = operations.find( table );
  if ( mapId == operations.end() )
  {
    return false;
  }
  else
  {
    const SqValuesMap &ids = mapId->second;
    auto fid =  ids.find( id );
    return fid != ids.end();
  }
}

void _insert( SqOperations &mapIds, const std::string &table, int id, Sqlite3ChangesetIter &pp )
{
  if ( !pp.get() )
    throw GeoDiffException( "internal error in _insert" );

  int rc;
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  rc = sqlite3changeset_op(
         pp.get(),  /* Iterator object */
         &pzTab,             /* OUT: Pointer to table name */
         &pnCol,                     /* OUT: Number of columns in table */
         &pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
         &pbIndirect                 /* OUT: True for an 'indirect' change */
       );

  sqlite3_value *ppValue;

  SqValues oldValues( pnCol );
  SqValues newValues( pnCol );

  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_INSERT )
    {
      pp.newValue( i, &ppValue );
      newValues[i].reset( new Sqlite3Value( ppValue ) );
    }

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      pp.oldValue( i, &ppValue );
      oldValues[i].reset( new Sqlite3Value( ppValue ) );
    }
  }

  std::pair<SqValues, SqValues> vals( oldValues, newValues );
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    SqValuesMap newSet;
    newSet.insert( std::pair<int, std::pair<SqValues, SqValues>>( id, vals ) );
    mapIds[table] = newSet;
  }
  else
  {
    SqValuesMap &oldSet = ids->second;
    oldSet.insert( std::pair<int, std::pair<SqValues, SqValues>>( id, vals ) );
  }
}

SqValues _get_old( const SqOperations &mapIds, const std::string &table, int id )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    throw GeoDiffException( "internal error in _get_old SqOperations" );
  }
  else
  {
    const SqValuesMap &oldSet = ids->second;
    auto a = oldSet.find( id );
    if ( a == oldSet.end() )
      throw GeoDiffException( "internal error: _get_old SqOperations" );
    return a->second.first;
  }
}

SqValues _get_new( const SqOperations &mapIds, const std::string &table, int id )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    throw GeoDiffException( "internal error in _get_new SqOperations" );
  }
  else
  {
    const SqValuesMap &oldSet = ids->second;
    auto a = oldSet.find( id );
    if ( a == oldSet.end() )
      throw GeoDiffException( "internal error: _get_new SqOperations" );
    return a->second.second;
  }
}

//////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////

int _get_primary_key( Sqlite3ChangesetIter &pp, int pOp )
{
  if ( !pp.get() )
    throw GeoDiffException( "internal error in _get_primary_key" );

  unsigned char *pabPK;
  int pnCol;
  int rc = sqlite3changeset_pk( pp.get(),  &pabPK, &pnCol );
  if ( rc )
  {
    throw GeoDiffException( "internal error in _get_primary_key" );
  }

  // lets assume for now it has only one PK and it is int...
  int pk_column_number = -1;
  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pabPK[i] == 0x01 )
    {
      if ( pk_column_number >= 0 )
      {
        // ups primary key composite!
        throw GeoDiffException( "internal error in _get_primary_key: support composite primary keys not implemented" );
      }
      pk_column_number = i;
    }
  }
  if ( pk_column_number == -1 )
  {
    throw GeoDiffException( "internal error in _get_primary_key: unable to find internal key" );
  }

  // now get the value
  sqlite3_value *ppValue = nullptr;
  if ( pOp == SQLITE_INSERT )
  {
    pp.newValue( pk_column_number, &ppValue );
  }
  else if ( pOp == SQLITE_DELETE || pOp == SQLITE_UPDATE )
  {
    pp.oldValue( pk_column_number, &ppValue );
  }
  if ( !ppValue )
    throw GeoDiffException( "internal error in _get_primary_key: unable to get value of primary key" );

  int type = sqlite3_value_type( ppValue );
  if ( type == SQLITE_INTEGER )
  {
    int val = sqlite3_value_int( ppValue );
    return val;
  }
  else if ( type == SQLITE_TEXT )
  {
    const unsigned char *valT = sqlite3_value_text( ppValue );
    int hash = 0;
    int len = strlen( ( const char * ) valT );
    for ( int i = 0; i < len; i++ )
    {
      hash = 33 * hash + ( unsigned char )valT[i];
    }
    return hash;
  }
  else
  {
    throw GeoDiffException( "internal error in _get_primary_key: unsuported type of primary key" );
  }
}

int _parse_old_changeset( const Buffer &buf_BASE_THEIRS, MapIds &inserted, MapIds &deleted, SqOperations &updated )
{
  Sqlite3ChangesetIter pp;
  pp.start( buf_BASE_THEIRS );

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

    int pk = _get_primary_key( pp, pOp );

    if ( pOp == SQLITE_INSERT )
    {
      _insert_id( inserted, pzTab, pk );
    }
    if ( pOp == SQLITE_DELETE )
    {
      _insert_id( deleted, pzTab, pk );
    }
    if ( pOp == SQLITE_UPDATE )
    {
      _insert( updated, pzTab, pk, pp );
    }
  }

  _print_idmap( inserted, "inserted" );
  _print_idmap( deleted, "deleted" );

  return GEODIFF_SUCCESS;
}

int _find_mapping_for_new_changeset( const Buffer &buf,
                                     const MapIds &inserted,
                                     const MapIds &deleted,
                                     MappingIds &mapping )
{
  std::map<std::string, int> freeIndices;
  for ( auto mapId : inserted )
  {
    freeIndices[mapId.first] = _maximum_id( inserted, mapId.first ) + 1;
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

    if ( pOp == SQLITE_INSERT )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( _contains_id( inserted, pzTab, pk ) )
      {
        // conflict 2 concurrent inserts...
        auto it = freeIndices.find( pzTab );
        if ( it == freeIndices.end() )
          throw GeoDiffException( "internal error: freeIndices" );

        _insert_ids( mapping, pzTab, pk, it->second );

        // increase counter
        it->second ++;
      }
    }
    else if ( pOp == SQLITE_UPDATE )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( _contains_id( deleted, pzTab, pk ) )
      {
        // update on deleted feature...
        _insert_ids( mapping, pzTab, pk, INVALID_FID );
      }
    }
  }

  _print_idmap( mapping, "mapping" );

  return GEODIFF_SUCCESS;
}

void _handle_insert(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const MappingIds &mapping,
  unsigned char *aiFlg,
  FILE *out
)
{
  // first write operation type (iType)
  putc( SQLITE_INSERT, out );
  putc( 0, out );

  sqlite3_value *value;
  // resolve primary key and patched primary key
  int pk = _get_primary_key( pp, SQLITE_INSERT );
  int newPk = pk;

  if ( _contains_old( mapping, tableName, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = _get_new( mapping, tableName, pk );
  }

  for ( int i = 0; i < pnCol; i++ )
  {
    if ( aiFlg[i] )
    {
      putValue( out, newPk );
    }
    else
    {
      pp.newValue( i, &value );
      putValue( out, value );
    }
  }
}

void _handle_delete(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const MappingIds &mapping,
  unsigned char *aiFlg,
  FILE *out
)
{
  // first write operation type (iType)
  putc( SQLITE_DELETE, out );
  putc( 0, out );

  sqlite3_value *value;

  // resolve primary key and patched primary key
  int pk = _get_primary_key( pp, SQLITE_DELETE );
  int newPk = pk;

  if ( _contains_old( mapping, tableName, pk ) )
  {
    // conflict 2 concurrent updates...
    newPk = _get_new( mapping, tableName, pk );
  }

  for ( int i = 0; i < pnCol; i++ )
  {
    if ( aiFlg[i] )
    {
      putValue( out, newPk );
    }
    else
    {
      pp.oldValue( i, &value );
      putValue( out, value );
    }
  }
}

void _handle_update(
  Sqlite3ChangesetIter &pp,
  const std::string tableName,
  int pnCol,
  const MappingIds &mapping,
  const SqOperations &updated,
  unsigned char *aiFlg,
  FILE *out
)
{
  // get values from patched (new) master
  int pk = _get_primary_key( pp, SQLITE_UPDATE );
  if ( _contains_old( mapping, tableName, pk ) )
  {
    int newPk = _get_new( mapping, tableName, pk );
    if ( newPk == INVALID_FID )
      return;
  }

  SqValues patchedVals = _get_new( updated, tableName, pk );

  // first write operation type (iType)
  putc( SQLITE_UPDATE, out );
  putc( 0, out );
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
    putValue( out, value );
  }

  for ( int i = 0; i < pnCol; i++ )
  {
    pp.newValue( i, &value );
    // gpkg_ogr_contents column 1 is total number of features
    if ( strcmp( tableName.c_str(), "gpkg_ogr_contents" ) == 0 && i == 1 )
    {
      int numberInPatched = 0;
      std::shared_ptr<Sqlite3Value> patchedVal = patchedVals[i];
      if ( patchedVal && patchedVal->isValid() )
      {
        numberInPatched = sqlite3_value_int64( patchedVal->value() );
      }
      sqlite3_value *oldValue;
      pp.oldValue( i, &oldValue );
      int numberInBase =  sqlite3_value_int64( oldValue );
      int numberInThis = sqlite3_value_int64( value );
      int addedFeatures = numberInThis - numberInBase;
      int newVal = numberInPatched + addedFeatures;
      putValue( out, newVal );
    }
    else
    {
      putValue( out, value );
    }
  }

}

int _prepare_new_changeset( const Buffer &buf, const std::string &changesetNew, const MappingIds &mapping, const SqOperations &updated )
{
  Sqlite3ChangesetIter pp;
  pp.start( buf );

  std::map<std::string, FILE *> buffers;
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

    // create buffer.... this is BAD BAD ... replace FILE* with some normal binary streams. ...
    FILE *out = nullptr;
    auto buffer = buffers.find( pzTab );
    if ( buffer == buffers.end() )
    {
      std::string temp = changesetNew + "_" + std::string( pzTab );
      out = fopen( temp.c_str(), "wb" );
      if ( !out )
      {
        std::cout << "unable to open file for writing " << changesetNew << std::endl;
        return GEODIFF_ERROR;
      }
      buffers[pzTab] = out;

      // write header for changeset for this table
      putc( 'T', out );
      putsVarint( out, ( sqlite3_uint64 )nCol );
      for ( int i = 0; i < nCol; i++ ) putc( aiFlg[i], out );
      fwrite( pzTab, 1, strlen( pzTab ), out );
      putc( 0, out );
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
        _handle_update(
          pp,
          pzTab,
          pnCol,
          mapping,
          updated,
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
        _handle_delete(
          pp,
          pzTab,
          pnCol,
          mapping,
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
    FILE *buf = it.second;
    fclose( buf );
    std::string temp = changesetNew + "_" + std::string( it.first );
    buf = fopen( temp.c_str(), "rb" );
    if ( !buf )
      throw GeoDiffException( "unable to open " + temp );

    char buffer[1];; //do not be lazy, use bigger buffer
    while ( fread( buffer, 1, 1, buf ) > 0 )
      fwrite( buffer, 1, 1, out );

    fclose( buf );
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

  MapIds inserted;
  MapIds deleted;
  SqOperations updated;
  _parse_old_changeset( buf_BASE_THEIRS, inserted, deleted, updated );

  MappingIds mapping;
  _find_mapping_for_new_changeset( buf_BASE_MODIFIED, inserted, deleted, mapping );

  _prepare_new_changeset( buf_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, updated );

  return GEODIFF_SUCCESS;
}
