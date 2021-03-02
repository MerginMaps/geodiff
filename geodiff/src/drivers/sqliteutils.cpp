/*
 GEODIFF - MIT License
 Copyright (C) 2020 Peter Petrik, Martin Dobias
*/

#include "sqliteutils.h"

#include "geodiffutils.hpp"

#include <gpkg.h>

#include <algorithm>


Sqlite3Db::Sqlite3Db() = default;
Sqlite3Db::~Sqlite3Db()
{
  close();
}

void Sqlite3Db::open( const std::string &filename )
{
  close();
  int rc = sqlite3_open_v2( filename.c_str(), &mDb, SQLITE_OPEN_READWRITE, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "Unable to open " + filename + " as sqlite3 database" );
  }
}

void Sqlite3Db::create( const std::string &filename )
{
  close();

  if ( fileexists( filename ) )
  {
    throw GeoDiffException( "Unable to create sqlite3 database - already exists: " + filename );
  }

  int rc = sqlite3_open_v2( filename.c_str(), &mDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "Unable to create " + filename + " as sqlite3 database" );
  }
}

void Sqlite3Db::exec( const Buffer &buf )
{
  int rc = sqlite3_exec( get(), buf.c_buf(), NULL, 0, NULL );
  if ( rc )
  {
    throw GeoDiffException( "Unable to exec buffer on sqlite3 database" );
  }
}

sqlite3 *Sqlite3Db::get()
{
  return mDb;
}

void Sqlite3Db::close()
{
  if ( mDb )
  {
    sqlite3_close( mDb );
    mDb = nullptr;
  }
}


///


Sqlite3Stmt::Sqlite3Stmt() = default;

Sqlite3Stmt::~Sqlite3Stmt()
{
  close();
}

sqlite3_stmt *Sqlite3Stmt::db_vprepare( sqlite3 *db, const char *zFormat, va_list ap )
{
  char *zSql;
  int rc;
  sqlite3_stmt *pStmt;

  zSql = sqlite3_vmprintf( zFormat, ap );
  if ( zSql == nullptr )
  {
    throw GeoDiffException( "out of memory" );
  }

  rc = sqlite3_prepare_v2( db, zSql, -1, &pStmt, nullptr );
  sqlite3_free( zSql );
  if ( rc )
  {
    throw GeoDiffException( "SQL statement error: " + std::string( sqlite3_errmsg( db ) ) );
  }
  return pStmt;
}

void Sqlite3Stmt::prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... )
{
  if ( db && db->get() )
  {
    va_list ap;
    va_start( ap, zFormat );
    mStmt = db_vprepare( db->get(), zFormat, ap );
    va_end( ap );
  }
}

void Sqlite3Stmt::prepare( std::shared_ptr<Sqlite3Db> db, const std::string &sql )
{
  sqlite3_stmt *pStmt;
  int rc = sqlite3_prepare_v2( db->get(), sql.c_str(), -1, &pStmt, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "SQL statement error: " + std::string( sqlite3_errmsg( db->get() ) ) );
  }
  mStmt = pStmt;
}

sqlite3_stmt *Sqlite3Stmt::get()
{
  return mStmt;
}

void Sqlite3Stmt::close()
{
  if ( mStmt )
  {
    sqlite3_finalize( mStmt );
    mStmt = nullptr;
  }
}

std::string Sqlite3Stmt::expandedSql() const
{
  char *str = sqlite3_expanded_sql( mStmt );
  std::string sql( str );
  sqlite3_free( str );
  return sql;
}


///


Sqlite3Value::Sqlite3Value() = default;

Sqlite3Value::Sqlite3Value( const sqlite3_value *val )
{
  if ( val )
  {
    mVal = sqlite3_value_dup( val );
  }
}

Sqlite3Value::~Sqlite3Value()
{
  if ( mVal )
  {
    sqlite3_value_free( mVal );
  }
}

bool Sqlite3Value::isValid() const
{
  return mVal != nullptr;
}

sqlite3_value *Sqlite3Value::value() const
{
  return mVal;
}

std::string Sqlite3Value::toString( sqlite3_value *ppValue )
{
  if ( !ppValue )
    return "nil";
  std::string val = "n/a";
  int type = sqlite3_value_type( ppValue );
  if ( type == SQLITE_INTEGER )
    val = std::to_string( sqlite3_value_int( ppValue ) );
  else if ( type == SQLITE_TEXT )
    val = std::string( reinterpret_cast<const char *>( sqlite3_value_text( ppValue ) ) );
  else if ( type == SQLITE_FLOAT )
    val = std::to_string( sqlite3_value_double( ppValue ) );
  else if ( type == SQLITE_BLOB )
    val = "blob " + std::to_string( sqlite3_value_bytes( ppValue ) ) + " bytes";
  return val;
}


///


bool register_gpkg_extensions( std::shared_ptr<Sqlite3Db> db )
{
  // register GPKG functions like ST_IsEmpty
  int rc = sqlite3_enable_load_extension( db->get(), 1 );
  if ( rc )
  {
    return false;
  }

  rc = sqlite3_gpkg_auto_init( db->get(), NULL, NULL );
  if ( rc )
  {
    return false;
  }

  return true;
}


bool isGeoPackage( std::shared_ptr<Sqlite3Db> db )
{
  std::vector<std::string> tableNames;
  sqliteTables( db,
                "main",
                tableNames );

  return std::find( tableNames.begin(), tableNames.end(), "gpkg_contents" ) != tableNames.end();
}


void sqliteTriggers( std::shared_ptr<Sqlite3Db> db, std::vector<std::string> &triggerNames, std::vector<std::string> &triggerCmds )
{
  triggerNames.clear();
  triggerCmds.clear();

  Sqlite3Stmt statament;
  statament.prepare( db, "%s", "select name, sql from sqlite_master where type = 'trigger'" );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const char *name = ( char * ) sqlite3_column_text( statament.get(), 0 );
    const char *sql = ( char * ) sqlite3_column_text( statament.get(), 1 );

    if ( !name || !sql )
      continue;

    /* typically geopackage from ogr would have these (table name is simple)
        - gpkg_tile_matrix_zoom_level_insert
        - gpkg_tile_matrix_zoom_level_update
        - gpkg_tile_matrix_matrix_width_insert
        - gpkg_tile_matrix_matrix_width_update
        - gpkg_tile_matrix_matrix_height_insert
        - gpkg_tile_matrix_matrix_height_update
        - gpkg_tile_matrix_pixel_x_size_insert
        - gpkg_tile_matrix_pixel_x_size_update
        - gpkg_tile_matrix_pixel_y_size_insert
        - gpkg_tile_matrix_pixel_y_size_update
        - gpkg_metadata_md_scope_insert
        - gpkg_metadata_md_scope_update
        - gpkg_metadata_reference_reference_scope_insert
        - gpkg_metadata_reference_reference_scope_update
        - gpkg_metadata_reference_column_name_insert
        - gpkg_metadata_reference_column_name_update
        - gpkg_metadata_reference_row_id_value_insert
        - gpkg_metadata_reference_row_id_value_update
        - gpkg_metadata_reference_timestamp_insert
        - gpkg_metadata_reference_timestamp_update
        - rtree_simple_geometry_insert
        - rtree_simple_geometry_update1
        - rtree_simple_geometry_update2
        - rtree_simple_geometry_update3
        - rtree_simple_geometry_update4
        - rtree_simple_geometry_delete
        - trigger_insert_feature_count_simple
        - trigger_delete_feature_count_simple
     */
    const std::string triggerName( name );
    if ( startsWith( triggerName, "gpkg_" ) )
      continue;
    if ( startsWith( triggerName, "rtree_" ) )
      continue;
    if ( startsWith( triggerName, "trigger_insert_feature_count_" ) )
      continue;
    if ( startsWith( triggerName, "trigger_delete_feature_count_" ) )
      continue;
    triggerNames.push_back( name );
    triggerCmds.push_back( sql );
  }
  statament.close();
}


ForeignKeys sqliteForeignKeys( std::shared_ptr<Sqlite3Db> db, const std::string &dbName )
{
  std::vector<std::string> fromTableNames;
  sqliteTables( db, dbName, fromTableNames );

  ForeignKeys ret;

  for ( const std::string &fromTableName : fromTableNames )
  {
    if ( isLayerTable( fromTableName ) )
    {
      Sqlite3Stmt pStmt;     /* SQL statement being run */
      pStmt.prepare( db, "SELECT * FROM %s.pragma_foreign_key_list(%Q)", dbName.c_str(), fromTableName.c_str() );
      while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
      {
        const char *fk_to_table = ( const char * )sqlite3_column_text( pStmt.get(), 2 );
        const char *fk_from = ( const char * )sqlite3_column_text( pStmt.get(), 3 );
        const char *fk_to = ( const char * )sqlite3_column_text( pStmt.get(), 4 );

        if ( fk_to_table && fk_from && fk_to )
        {
          // TODO: this part is not speed-optimized and could be slower for databases with a lot of
          // columns and/or foreign keys. For each entry we grab column names again
          // and we search for index of value in plain std::vector array...
          std::vector<std::string> fromColumnNames = sqliteColumnNames( db, dbName, fromTableName );
          int fk_from_id = indexOf( fromColumnNames, fk_from );
          if ( fk_from_id < 0 )
            continue;

          std::vector<std::string> toColumnNames = sqliteColumnNames( db, dbName, fk_to_table );
          int fk_to_id = indexOf( toColumnNames, fk_to );
          if ( fk_to_id < 0 )
            continue;

          TableColumn from( fromTableName, fk_from_id );
          TableColumn to( fk_to_table, fk_to_id );
          ret.insert( std::pair<TableColumn, TableColumn>( from, to ) );
        }
      }
      pStmt.close();
    }
  }

  return ret;
}


void sqliteTables( std::shared_ptr<Sqlite3Db> db,
                   const std::string &dbName,
                   std::vector<std::string> &tableNames )
{
  tableNames.clear();
  std::string all_tables_sql = "SELECT name FROM " + dbName + ".sqlite_master\n"
                               " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
                               " ORDER BY name";
  Sqlite3Stmt statament;
  statament.prepare( db, "%s", all_tables_sql.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const char *name = ( const char * )sqlite3_column_text( statament.get(), 0 );
    if ( !name )
      continue;

    std::string tableName( name );
    /* typically geopackage from ogr would have these (table name is simple)
    gpkg_contents
    gpkg_extensions
    gpkg_geometry_columns
    gpkg_ogr_contents
    gpkg_spatial_ref_sys
    gpkg_tile_matrix
    gpkg_tile_matrix_set
    rtree_simple_geometry_node
    rtree_simple_geometry_parent
    rtree_simple_geometry_rowid
    simple (or any other name(s) of layers)
    sqlite_sequence
    */

    // table handled by triggers trigger_*_feature_count_*
    if ( startsWith( tableName, "gpkg_ogr_contents" ) )
      continue;
    // table handled by triggers rtree_*_geometry_*
    if ( startsWith( tableName, "rtree_" ) )
      continue;
    // internal table for AUTOINCREMENT
    if ( tableName == "sqlite_sequence" )
      continue;

    tableNames.push_back( tableName );
  }

  // result is ordered by name
}


/*
 * inspired by sqldiff.c function: columnNames()
 */
std::vector<std::string> sqliteColumnNames(
  std::shared_ptr<Sqlite3Db> db,
  const std::string &zDb,                /* Database ("main" or "aux") to query */
  const std::string &tableName   /* Name of table to return details of */
)
{
  std::vector<std::string> az;           /* List of column names to be returned */
  int naz = 0;             /* Number of entries in az[] */
  Sqlite3Stmt pStmt;     /* SQL statement being run */
  std::string zPkIdxName;    /* Name of the PRIMARY KEY index */
  int truePk = 0;          /* PRAGMA table_info indentifies the PK to use */
  int nPK = 0;             /* Number of PRIMARY KEY columns */
  int i, j;                /* Loop counters */

  /* Figure out what the true primary key is for the table.
  **   *  For WITHOUT ROWID tables, the true primary key is the same as
  **      the schema PRIMARY KEY, which is guaranteed to be present.
  **   *  For rowid tables with an INTEGER PRIMARY KEY, the true primary
  **      key is the INTEGER PRIMARY KEY.
  **   *  For all other rowid tables, the rowid is the true primary key.
  */
  const char *zTab = tableName.c_str();
  pStmt.prepare( db, "PRAGMA %s.index_list=%Q", zDb.c_str(), zTab );
  while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
  {
    if ( sqlite3_stricmp( ( const char * )sqlite3_column_text( pStmt.get(), 3 ), "pk" ) == 0 )
    {
      zPkIdxName = ( const char * ) sqlite3_column_text( pStmt.get(), 1 );
      break;
    }
  }
  pStmt.close();

  if ( !zPkIdxName.empty() )
  {
    int nKey = 0;
    int nCol = 0;
    truePk = 0;
    pStmt.prepare( db, "PRAGMA %s.index_xinfo=%Q", zDb.c_str(), zPkIdxName.c_str() );
    while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
    {
      nCol++;
      if ( sqlite3_column_int( pStmt.get(), 5 ) ) { nKey++; continue; }
      if ( sqlite3_column_int( pStmt.get(), 1 ) >= 0 ) truePk = 1;
    }
    if ( nCol == nKey ) truePk = 1;
    if ( truePk )
    {
      nPK = nKey;
    }
    else
    {
      nPK = 1;
    }
    pStmt.close();
  }
  else
  {
    truePk = 1;
    nPK = 1;
  }
  pStmt.prepare( db, "PRAGMA %s.table_info=%Q", zDb.c_str(), zTab );

  naz = nPK;
  az.resize( naz );
  while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
  {
    int iPKey;
    std::string name = ( char * )sqlite3_column_text( pStmt.get(), 1 );
    if ( truePk && ( iPKey = sqlite3_column_int( pStmt.get(), 5 ) ) > 0 )
    {
      az[iPKey - 1] = name;
    }
    else
    {
      az.push_back( name );
    }
  }
  pStmt.close();

  /* If this table has an implicit rowid for a PK, figure out how to refer
  ** to it. There are three options - "rowid", "_rowid_" and "oid". Any
  ** of these will work, unless the table has an explicit column of the
  ** same name.  */
  if ( az[0].empty() )
  {
    std::vector<std::string> azRowid = { "rowid", "_rowid_", "oid" };
    for ( i = 0; i < azRowid.size(); i++ )
    {
      for ( j = 1; j < naz; j++ )
      {
        if ( az[j] == azRowid[i] ) break;
      }
      if ( j >= naz )
      {
        az[0] = azRowid[i];
        break;
      }
    }
    if ( az[0].empty() )
    {
      az.clear();
    }
  }

  return az;
}

int parseGpkgbHeaderSize( const std::string &gpkgWkb )
{
  // see GPKG binary header definition http://www.geopackage.org/spec/#gpb_spec

  char flagByte = gpkgWkb[ GPKG_FLAG_BYTE_POS ];

  char envelope_byte = ( flagByte & GPKG_ENVELOPE_SIZE_MASK ) >> 1;
  int envelope_size = 0;

  switch ( envelope_byte )
  {
    case 1:
      envelope_size = 32;
      break;

    case 2:
    // fall through
    case 3:
      envelope_size = 48;
      break;

    case 4:
      envelope_size = 64;
      break;

    default:
      envelope_size = 0;
      break;
  }

  return GPKG_NO_ENVELOPE_HEADER_SIZE + envelope_size;
}
