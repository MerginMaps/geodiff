/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffrebase.hpp"
#include "geodiffutils.hpp"
#include "geodiff.h"

#include <boost/filesystem.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>

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
  std::cout << name << std::endl;
  if ( mapIds.empty() )
    std::cout << "--none -- " << std::endl;

  for ( auto it : mapIds )
  {
    std::cout << "  " << it.first << std::endl << "    ";
    if ( it.second.empty() )
      std::cout << "--none -- ";
    for ( auto it2 : it.second )
    {
      std::cout << it2 << ",";
    }
    std::cout << std::endl;
  }
}

// table name -> old fid --> new fid
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
    assert( false );
  }
  else
  {
    const std::map < int, int  > &oldSet = ids->second;
    auto a = oldSet.find( id );
    assert( a != oldSet.end() );
    return a->second;
  }
}

void _print_idmap( const MappingIds &mapIds, const std::string &name )
{
  std::cout << name << std::endl;
  if ( mapIds.empty() )
    std::cout << "--none -- " << std::endl;

  for ( auto it : mapIds )
  {
    std::cout << "  " << it.first << std::endl << "    ";
    if ( it.second.empty() )
      std::cout << "--none -- ";
    for ( auto it2 : it.second )
    {
      std::cout << it2.first << "->" << it2.second << ",";
    }
    std::cout << std::endl;
  }
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

void _insert( SqOperations &mapIds, const std::string &table, int id, sqlite3_changeset_iter *pp )
{


  int rc;
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  rc = sqlite3changeset_op(
         pp,  /* Iterator object */
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
      rc = sqlite3changeset_new( pp, i, &ppValue );
      assert( rc == SQLITE_OK );
      newValues[i].reset( new Sqlite3Value( ppValue ) );
    }

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      rc = sqlite3changeset_old( pp, i, &ppValue );
      assert( rc == SQLITE_OK );
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
    assert( false );
  }
  else
  {
    const SqValuesMap &oldSet = ids->second;
    auto a = oldSet.find( id );
    assert( a != oldSet.end() );
    return a->second.first;
  }
}

SqValues _get_new( const SqOperations &mapIds, const std::string &table, int id )
{
  auto ids = mapIds.find( table );
  if ( ids == mapIds.end() )
  {
    assert( false );
  }
  else
  {
    const SqValuesMap &oldSet = ids->second;
    auto a = oldSet.find( id );
    assert( a != oldSet.end() );
    return a->second.second;
  }
}

//////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////
///////////////////////////////////////

int _get_primary_key( sqlite3_changeset_iter *pp, int pOp )
{
  unsigned char *pabPK;          /* OUT: Array of boolean - true for PK cols */
  int pnCol;
  int rc = sqlite3changeset_pk(
             pp,  /* Iterator object */
             &pabPK,          /* OUT: Array of boolean - true for PK cols */
             &pnCol           /* OUT: Number of entries in output array */
           );
  if ( rc == SQLITE_MISUSE )
  {
    assert( false );
  }
  assert( !rc );

  // lets assume for now it has only one PK and it is int...
  int pk_column_number = -1;

  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pabPK[i] == 0x01 )
    {
      if ( pk_column_number >= 0 )
      {
        // ups primary key composite!
        assert( false );
      }
      pk_column_number = i;
    }
  }
  assert( pk_column_number >= 0 );

  // now get the value
  sqlite3_value *ppValue = 0;
  if ( pOp == SQLITE_INSERT )
  {
    rc = sqlite3changeset_new( pp, pk_column_number, &ppValue );
    assert( rc == SQLITE_OK );
  }
  else if ( pOp == SQLITE_DELETE || pOp == SQLITE_UPDATE )
  {
    rc = sqlite3changeset_old( pp, pk_column_number, &ppValue );
    assert( rc == SQLITE_OK );
  }

  assert( ppValue );

  int type = sqlite3_value_type( ppValue );
  if ( type == SQLITE_INTEGER )
  {
    int val = sqlite3_value_int( ppValue );
    return val;
  }
  else   if ( type == SQLITE_TEXT )
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
    assert( false );
  }
}

int _parse_old_changeset( const Buffer &buf_BASE_THEIRS, MapIds &inserted, MapIds &deleted, SqOperations &updated )
{
  sqlite3_changeset_iter *pp;

  int rc = sqlite3changeset_start(
             &pp,
             buf_BASE_THEIRS.size(),
             buf_BASE_THEIRS.v_buf()
           );
  if ( rc != SQLITE_OK )
  {
    printf( "sqlite3changeset_start error %d\n", rc );
    return GEODIFF_ERROR;
  }

  std::cout << " PARSE OLD CHANGESET " << std::endl;

  while ( SQLITE_ROW == sqlite3changeset_next( pp ) )
  {
    int rc;
    const char *pzTab;
    int pnCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp,  /* Iterator object */
           &pzTab,             /* OUT: Pointer to table name */
           &pnCol,                     /* OUT: Number of columns in table */
           &pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
           &pbIndirect                 /* OUT: True for an 'indirect' change */
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
  sqlite3changeset_finalize( pp );

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

  sqlite3_changeset_iter *pp;
  int rc = sqlite3changeset_start(
             &pp,
             buf.size(),
             buf.v_buf()
           );
  if ( rc != SQLITE_OK )
  {
    printf( "sqlite3changeset_start error %d\n", rc );
    return GEODIFF_ERROR;
  }

  std::cout << " FIND MAPPINGS " << std::endl;
  while ( SQLITE_ROW == sqlite3changeset_next( pp ) )
  {
    int rc;
    const char *pzTab;
    int pnCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp,  /* Iterator object */
           &pzTab,             /* OUT: Pointer to table name */
           &pnCol,                     /* OUT: Number of columns in table */
           &pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
           &pbIndirect                 /* OUT: True for an 'indirect' change */
         );

    if ( pOp == SQLITE_INSERT )
    {
      int pk = _get_primary_key( pp, pOp );

      if ( _contains_id( inserted, pzTab, pk ) )
      {
        // conflict 2 concurrent updates...
        auto it = freeIndices.find( pzTab );
        assert( it != freeIndices.end() );

        _insert_ids( mapping, pzTab, pk, it->second );

        // increase counter
        it->second ++;
      }
    }

    // TODO DELETE
    // TODO UPDATE

  }
  sqlite3changeset_finalize( pp );

  _print_idmap( mapping, "mapping" );

  return GEODIFF_SUCCESS;
}

int _prepare_new_changeset( const Buffer &buf, const std::string &changesetNew, const MappingIds &mapping, const SqOperations &updated )
{
  sqlite3_changeset_iter *pp;
  int rc = sqlite3changeset_start(
             &pp,
             buf.size(),
             buf.v_buf()
           );
  if ( rc != SQLITE_OK )
  {
    printf( "sqlite3changeset_start error %d\n", rc );
    return GEODIFF_ERROR;
  }

  std::map<std::string, FILE *> buffers;

  std::cout << " CREATE CHANGESET" << std::endl;
  while ( SQLITE_ROW == sqlite3changeset_next( pp ) )
  {
    int rc;
    const char *pzTab;
    int pnCol;
    int pOp;
    int pbIndirect;
    rc = sqlite3changeset_op(
           pp,  /* Iterator object */
           &pzTab,             /* OUT: Pointer to table name */
           &pnCol,                     /* OUT: Number of columns in table */
           &pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
           &pbIndirect                 /* OUT: True for an 'indirect' change */
         );

    // TODO merge somehow with _get_primary_key?
    unsigned char *aiFlg;
    int nCol;
    rc = sqlite3changeset_pk(
           pp,  /* Iterator object */
           &aiFlg, /* OUT: Array of boolean - true for PK cols */
           &nCol /* OUT: Number of entries in output array */
         );
    if ( rc == SQLITE_MISUSE )
    {
      assert( false );
    }
    assert( !rc );

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

    // first write operation type (iType)
    putc( pOp, out );
    putc( 0, out );

    // now save the change to changeset
    sqlite3_value *value;
    switch ( pOp )
    {
      case SQLITE_UPDATE:
      {
        // get values from patched (new) master
        int pk = _get_primary_key( pp, pOp );
        SqValues patchedVals = _get_new( updated, pzTab, pk );

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
            // otherwise the value is same for both patched nad this, so use base value
            rc = sqlite3changeset_old( pp, i, &value );
            assert( rc == SQLITE_OK );
          }
          putValue( out, value );
        }

        for ( int i = 0; i < pnCol; i++ )
        {
          rc = sqlite3changeset_new( pp, i, &value );
          assert( rc == SQLITE_OK );

          // gpkg_ogr_contents column 1 is total number of features
          if ( strcmp( pzTab, "gpkg_ogr_contents" ) == 0 && i == 1 )
          {
            int numberInPatched = 0;
            std::shared_ptr<Sqlite3Value> patchedVal = patchedVals[i];
            if ( patchedVal && patchedVal->isValid() )
            {
              numberInPatched = sqlite3_value_int64( patchedVal->value() );
            }
            sqlite3_value *oldValue;
            rc = sqlite3changeset_old( pp, i, &oldValue );
            assert( rc == SQLITE_OK );
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
        break;
      }
      case SQLITE_INSERT:
      {
        // resolve primary key and patched primary key
        int pk = _get_primary_key( pp, pOp );
        int newPk = pk;

        if ( _contains_old( mapping, pzTab, pk ) )
        {
          // conflict 2 concurrent updates...
          newPk = _get_new( mapping, pzTab, pk );
        }

        for ( int i = 0; i < pnCol; i++ )
        {
          rc = sqlite3changeset_new( pp, i, &value );
          assert( rc == SQLITE_OK );
          if ( aiFlg[i] )
          {
            putValue( out, newPk );
          }
          else
          {
            putValue( out, value );
          }
        }
        break;
      }
      case SQLITE_DELETE:
      {
        // TODO THIS IS PROBABLY NOT WORKING...
        // do we need old or new primary key?

        // resolve primary key and patched primary key
        int pk = _get_primary_key( pp, pOp );
        int newPk = pk;

        if ( _contains_old( mapping, pzTab, pk ) )
        {
          // conflict 2 concurrent updates...
          newPk = _get_new( mapping, pzTab, pk );
        }

        for ( int i = 0; i < pnCol; i++ )
        {
          rc = sqlite3changeset_old( pp, i, &value );
          assert( rc == SQLITE_OK );
          if ( aiFlg[i] )
          {
            putValue( out, newPk );
          }
          else
          {
            putValue( out, value );
          }


        }
        break;
      }
    }

  }

  sqlite3changeset_finalize( pp );

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
    assert( buf );

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
  if ( boost::filesystem::exists( changeset_THEIRS_MODIFIED ) )
  {
    boost::filesystem::remove( changeset_THEIRS_MODIFIED );
  }

  Buffer buf_BASE_THEIRS;
  buf_BASE_THEIRS.read( changeset_BASE_THEIRS );
  if ( buf_BASE_THEIRS.isEmpty() )
  {
    printf( " -- no rabase needed! --\n" );
    boost::filesystem::copy( changeset_BASE_MODIFIED, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  Buffer buf_BASE_MODIFIED;
  buf_BASE_MODIFIED.read( changeset_BASE_MODIFIED );
  if ( buf_BASE_MODIFIED.isEmpty() )
  {
    printf( " -- no rabase needed! --\n" );
    boost::filesystem::copy( changeset_BASE_THEIRS, changeset_THEIRS_MODIFIED );
    return GEODIFF_SUCCESS;
  }

  MapIds inserted;
  MapIds deleted;
  SqOperations updated;
  _parse_old_changeset( buf_BASE_THEIRS, inserted, deleted, updated );

  MappingIds mapping;
  _find_mapping_for_new_changeset( buf_BASE_MODIFIED, inserted, deleted, mapping );

  // finally
  _prepare_new_changeset( buf_BASE_MODIFIED, changeset_THEIRS_MODIFIED, mapping, updated );

  return GEODIFF_SUCCESS;
}
