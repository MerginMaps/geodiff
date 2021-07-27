/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "postgresdriver.h"

#include "geodifflogger.hpp"
#include "geodiffutils.hpp"
#include "changeset.h"
#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "postgresutils.h"
#include "sqliteutils.h"

#include <algorithm>
#include <iostream>
#include <memory.h>


/**
 * Wrapper around PostgreSQL transactions.
 *
 * Constructor start a trasaction, it needs to be confirmed by a call to commitChanges() when
 * changes are ready to be written. If commitChanges() is not called, changes since the constructor
 * will be rolled back (so that on exception everything gets cleaned up properly).
 */
class PostgresTransaction
{
  public:
    PostgresTransaction( PGconn *conn )
      : mConn( conn )
    {
      PostgresResult res( execSql( mConn, "BEGIN" ) );
      if ( res.status() != PGRES_COMMAND_OK )
        throw GeoDiffException( "Unable to start transaction" );
    }

    ~PostgresTransaction()
    {
      if ( mConn )
      {
        // we had some problems - roll back any pending changes
        PostgresResult res( execSql( mConn, "ROLLBACK" ) );
      }
    }

    void commitChanges()
    {
      assert( mConn );
      PostgresResult res( execSql( mConn, "COMMIT" ) );
      if ( res.status() != PGRES_COMMAND_OK )
        throw GeoDiffException( "Unable to commit transaction" );

      // reset handler to the database so that the destructor does nothing
      mConn = nullptr;
    }

  private:
    PGconn *mConn = nullptr;
};


static void logApplyConflict( const std::string &type, const ChangesetEntry &entry )
{
  Logger::instance().warn( "CONFLICT: " + type + ":\n" + changesetEntryToJSON( entry ) );
}

/////


PostgresDriver::~PostgresDriver()
{
  close();
}

void PostgresDriver::openPrivate( const DriverParametersMap &conn )
{
  DriverParametersMap::const_iterator connInfo = conn.find( "conninfo" );
  if ( connInfo == conn.end() )
    throw GeoDiffException( "Missing 'conninfo' parameter" );
  std::string connInfoStr = connInfo->second;

  DriverParametersMap::const_iterator baseSchema = conn.find( "base" );
  if ( baseSchema == conn.end() )
    throw GeoDiffException( "Missing 'base' parameter" );
  mBaseSchema = baseSchema->second;

  DriverParametersMap::const_iterator modifiedSchema = conn.find( "modified" );
  mModifiedSchema = ( modifiedSchema == conn.end() ) ? std::string() : modifiedSchema->second;

  if ( mConn )
    throw GeoDiffException( "Connection already opened" );

  PGconn *c = PQconnectdb( connInfoStr.c_str() );

  if ( PQstatus( c ) != CONNECTION_OK )
  {
    throw GeoDiffException( "Cannot connect to PostgreSQL database: " + std::string( PQerrorMessage( c ) ) );
  }

  mConn = c;

  // Make sure we are using enough digits for floating point numbers to make sure that we are
  // not loosing any digits when querying data.
  // https://www.postgresql.org/docs/12/runtime-config-client.html#GUC-EXTRA-FLOAT-DIGITS
  PostgresResult res( execSql( mConn, "SET extra_float_digits = 2;" ) );
  if ( res.status() != PGRES_COMMAND_OK )
    throw GeoDiffException( "Failed to set extra_float_digits" );
}

void PostgresDriver::close()
{
  mBaseSchema.clear();
  mModifiedSchema.clear();
  if ( mConn )
    PQfinish( mConn );
  mConn = nullptr;
}

void PostgresDriver::open( const DriverParametersMap &conn )
{
  openPrivate( conn );

  {
    PostgresResult resBase( execSql( mConn, "SELECT 1 FROM pg_namespace WHERE nspname = " + quotedString( mBaseSchema ) ) );
    if ( resBase.rowCount() == 0 )
    {
      std::string baseSchema = mBaseSchema;  // close() will erase mBaseSchema...
      close();
      throw GeoDiffException( "The base schema does not exist: " + baseSchema );
    }
  }

  if ( !mModifiedSchema.empty() )
  {
    PostgresResult resBase( execSql( mConn, "SELECT 1 FROM pg_namespace WHERE nspname = " + quotedString( mModifiedSchema ) ) );
    if ( resBase.rowCount() == 0 )
    {
      std::string modifiedSchema = mModifiedSchema;  // close() will erase mModifiedSchema...
      close();
      throw GeoDiffException( "The base schema does not exist: " + modifiedSchema );
    }
  }
}

void PostgresDriver::create( const DriverParametersMap &conn, bool overwrite )
{
  openPrivate( conn );

  std::string sql;
  if ( overwrite )
    sql += "DROP SCHEMA IF EXISTS " + quotedIdentifier( mBaseSchema ) + " CASCADE; ";
  sql += "CREATE SCHEMA " + quotedIdentifier( mBaseSchema ) + ";";

  PostgresResult res( execSql( mConn, sql ) );
  if ( res.status() != PGRES_COMMAND_OK )
    throw GeoDiffException( "Failure creating table: " + res.statusErrorMessage() );
}


std::vector<std::string> PostgresDriver::listTables( bool useModified )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );
  if ( useModified && mModifiedSchema.empty() )
    throw GeoDiffException( "Should use modified schema, but it was not set" );

  std::string schemaName = useModified ? mModifiedSchema : mBaseSchema;
  std::string sql = "select tablename from pg_tables where schemaname=" + quotedString( schemaName );
  PostgresResult res( execSql( mConn, sql ) );

  std::vector<std::string> tables;
  for ( int i = 0; i < res.rowCount(); ++i )
  {
    if ( startsWith( res.value( i, 0 ), "gpkg_" ) )
      continue;

    tables.push_back( res.value( i, 0 ) );
  }

  // make sure tables are in alphabetical order, so that if we compare table names
  // from different schemas, they should be matching
  std::sort( tables.begin(), tables.end() );

  return tables;
}

struct GeometryTypeDetails
{
  const char *flatType;
  bool hasZ;
  bool hasM;
};

static void extractGeometryTypeDetails( const std::string &geomType, const std::string &coordinateDimension, std::string &flatGeomType, bool &hasZ, bool &hasM )
{
  std::map<std::string, GeometryTypeDetails> d =
  {
    { "POINT",   { "POINT", false, false } },
    { "POINTZ",  { "POINT", true,  false } },
    { "POINTM",  { "POINT", false, true  } },
    { "POINTZM", { "POINT", true,  true  } },
    { "LINESTRING",   { "LINESTRING", false, false } },
    { "LINESTRINGZ",  { "LINESTRING", true,  false } },
    { "LINESTRINGM",  { "LINESTRING", false, true  } },
    { "LINESTRINGZM", { "LINESTRING", true,  true  } },
    { "POLYGON",   { "POLYGON", false, false } },
    { "POLYGONZ",  { "POLYGON", true,  false } },
    { "POLYGONM",  { "POLYGON", false, true  } },
    { "POLYGONZM", { "POLYGON", true,  true  } },

    { "MULTIPOINT",   { "MULTIPOINT", false, false } },
    { "MULTIPOINTZ",  { "MULTIPOINT", true,  false } },
    { "MULTIPOINTM",  { "MULTIPOINT", false, true  } },
    { "MULTIPOINTZM", { "MULTIPOINT", true,  true  } },
    { "MULTILINESTRING",   { "MULTILINESTRING", false, false } },
    { "MULTILINESTRINGZ",  { "MULTILINESTRING", true,  false } },
    { "MULTILINESTRINGM",  { "MULTILINESTRING", false, true  } },
    { "MULTILINESTRINGZM", { "MULTILINESTRING", true,  true  } },
    { "MULTIPOLYGON",   { "MULTIPOLYGON", false, false } },
    { "MULTIPOLYGONZ",  { "MULTIPOLYGON", true,  false } },
    { "MULTIPOLYGONM",  { "MULTIPOLYGON", false, true  } },
    { "MULTIPOLYGONZM", { "MULTIPOLYGON", true,  true  } },

    { "GEOMETRYCOLLECTION",   { "GEOMETRYCOLLECTION", false, false } },
    { "GEOMETRYCOLLECTIONZ",  { "GEOMETRYCOLLECTION", true,  false } },
    { "GEOMETRYCOLLECTIONM",  { "GEOMETRYCOLLECTION", false, true  } },
    { "GEOMETRYCOLLECTIONZM", { "GEOMETRYCOLLECTION", true,  true  } },

    // TODO: curve geometries
  };

  /*
   *  Special PostGIS coding of xyZ, xyM and xyZM type https://postgis.net/docs/using_postgis_dbmanagement.html#geometry_columns
   *  coordinateDimension number bears information about third and fourth dimension.
   */
  std::string type = geomType;

  if ( coordinateDimension == "4" )
  {
    type += "ZM";
  }
  else if ( ( coordinateDimension == "3" ) && ( type.back() != 'M' ) )
  {
    type += "Z";
  }

  auto it = d.find( type );
  if ( it != d.end() )
  {
    flatGeomType = it->second.flatType;
    hasZ = it->second.hasZ;
    hasM = it->second.hasM;
  }
  else
    throw GeoDiffException( "Unknown geometry type: " + type );
}


TableSchema PostgresDriver::tableSchema( const std::string &tableName, bool useModified )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );
  if ( useModified && mModifiedSchema.empty() )
    throw GeoDiffException( "Should use modified schema, but it was not set" );

  std::string schemaName = useModified ? mModifiedSchema : mBaseSchema;

  // try to figure out details of the geometry columns (if any)
  std::string sqlGeomDetails = "SELECT f_geometry_column, type, srid, coord_dimension FROM geometry_columns WHERE f_table_schema = " +
                               quotedString( schemaName ) + " AND f_table_name = " + quotedString( tableName );
  std::map<std::string, std::pair<std::string, std::string>> geomTypes;
  std::map<std::string, int> geomSrids;
  PostgresResult resGeomDetails( execSql( mConn, sqlGeomDetails ) );
  for ( int i = 0; i < resGeomDetails.rowCount(); ++i )
  {
    std::string name = resGeomDetails.value( i, 0 );
    std::string type = resGeomDetails.value( i, 1 );
    std::string srid = resGeomDetails.value( i, 2 );
    std::string dimension = resGeomDetails.value( i, 3 );
    int sridInt = srid.empty() ? -1 : atoi( srid.c_str() );
    geomTypes[name] = { type, dimension };
    geomSrids[name] = sridInt;
  }

  std::string sqlColumns =
    "SELECT a.attname, pg_catalog.format_type(a.atttypid, a.atttypmod), i.indisprimary, a.attnotnull, "
    "    EXISTS ("
    "             SELECT FROM pg_attrdef ad"
    "             WHERE  ad.adrelid = a.attrelid"
    "             AND    ad.adnum   = a.attnum"
    "             AND    pg_get_expr(ad.adbin, ad.adrelid)"
    "                  = 'nextval('''"
    "                 || (pg_get_serial_sequence (a.attrelid::regclass::text, a.attname))::regclass"
    "                 || '''::regclass)'"
    "       ) AS has_sequence"
    " FROM pg_catalog.pg_attribute a"
    " LEFT JOIN pg_index i ON a.attrelid = i.indrelid AND a.attnum = ANY(i.indkey)"
    " WHERE"
    "   a.attnum > 0"
    "   AND NOT a.attisdropped"
    "   AND a.attrelid = ("
    "      SELECT c.oid"
    "        FROM pg_catalog.pg_class c"
    "        LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace"
    "        WHERE c.relname = " + quotedString( tableName ) +
    "          AND n.nspname = " + quotedString( schemaName ) +
    "   )"
    "  ORDER BY a.attnum";

  PostgresResult res( execSql( mConn, sqlColumns ) );

  int srsId = -1;
  TableSchema schema;
  schema.name = tableName;
  for ( int i = 0; i < res.rowCount(); ++i )
  {
    TableColumnInfo col;
    col.name = res.value( i, 0 );
    col.isPrimaryKey = ( res.value( i, 2 ) == "t" );
    std::string type( res.value( i, 1 ) );

    if ( startsWith( type, "geometry" ) )
    {
      std::string geomTypeName;
      bool hasM = false;
      bool hasZ = false;

      if ( geomTypes.find( col.name ) != geomTypes.end() )
      {
        extractGeometryTypeDetails( geomTypes[col.name].first, geomTypes[col.name].second, geomTypeName, hasZ, hasM );
        srsId = geomSrids[col.name];
      }
      col.setGeometry( geomTypeName, srsId, hasM, hasZ );
    }

    col.type = columnType( type, Driver::POSTGRESDRIVERNAME, col.isGeometry );
    col.isNotNull = ( res.value( i, 3 ) == "t" );
    col.isAutoIncrement = ( res.value( i, 4 ) == "t" );

    schema.columns.push_back( col );
  }

  //
  // get CRS details
  //

  if ( srsId != -1 )
  {
    PostgresResult resCrs( execSql( mConn,
                                    "SELECT auth_name, auth_srid, srtext "
                                    "FROM spatial_ref_sys WHERE srid = " + std::to_string( srsId ) ) );

    if ( resCrs.rowCount() == 0 )
      throw GeoDiffException( "Unknown CRS in table " + tableName );
    schema.crs.srsId = srsId;
    schema.crs.authName = resCrs.value( 0, 0 );
    schema.crs.authCode = atoi( resCrs.value( 0, 1 ).c_str() );
    schema.crs.wkt = resCrs.value( 0, 2 );
  }

  return schema;
}


static std::string allColumnNames( const TableSchema &tbl, const std::string &prefix = "" )
{
  std::string columns;
  for ( const TableColumnInfo &c : tbl.columns )
  {
    if ( !columns.empty() )
      columns += ", ";

    std::string name;
    if ( !prefix.empty() )
      name = prefix + ".";
    name += quotedIdentifier( c.name );

    if ( c.isGeometry )
      columns += "ST_AsBinary(" + name + ")";
    else if ( c.type == "timestamp without time zone" )
    {
      // by default postgresql would return date/time as a formatted string
      // e.g. "2020-07-13 16:17:54" but we want IS0-8601 format "2020-07-13T16:17:54Z"
      columns += "to_char(" + name + ",'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"')";
    }
    else
      columns += name;
  }
  return columns;
}


//! Constructs SQL query to get all rows that do not exist in the other table (used for insert and delete)
static std::string sqlFindInserted( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, bool reverse )
{
  std::string exprPk;
  for ( const TableColumnInfo &c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name ) + "=" +
                quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + "." + quotedIdentifier( c.name );
    }
  }

  std::string sql = "SELECT " + allColumnNames( tbl ) + " FROM " +
                    quotedIdentifier( reverse ? schemaNameBase : schemaNameModified ) + "." + quotedIdentifier( tableName ) +
                    " WHERE NOT EXISTS ( SELECT 1 FROM " +
                    quotedIdentifier( reverse ? schemaNameModified : schemaNameBase ) + "." + quotedIdentifier( tableName ) +
                    " WHERE " + exprPk + ")";
  return sql;
}


static std::string sqlFindModified( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl )
{
  std::string exprPk;
  std::string exprOther;
  for ( const TableColumnInfo &c : tbl.columns )
  {
    if ( c.isPrimaryKey )
    {
      if ( !exprPk.empty() )
        exprPk += " AND ";
      exprPk += "b." + quotedIdentifier( c.name ) + "=" +
                "a." + quotedIdentifier( c.name );
    }
    else // not a primary key column
    {
      if ( !exprOther.empty() )
        exprOther += " OR ";

      std::string colBase = "b." + quotedIdentifier( c.name );
      std::string colModified = "a." + quotedIdentifier( c.name );

      // pg IS DISTINCT FROM operator handles comparison between null and not null values. When comparing values with basic `=` operator,
      // comparison 7 = NULL returns null (no rows), not false as one would expect. IS DISTINCT FROM handles this and returns false in such situations.
      // When comparing non-null values with IS DISTINCT FROM operator, it works just as `=` does with non-null values.
      exprOther += "(" + colBase + " IS DISTINCT FROM " + colModified + ")";
    }
  }

  std::string sql = "SELECT " + allColumnNames( tbl, "a" ) + ", " + allColumnNames( tbl, "b" ) + " FROM " +
                    quotedIdentifier( schemaNameModified ) + "." + quotedIdentifier( tableName ) + " a, " +
                    quotedIdentifier( schemaNameBase ) + "." + quotedIdentifier( tableName ) + " b" +
                    " WHERE " + exprPk;

  if ( !exprOther.empty() )
    sql += " AND (" + exprOther + ")";

  return sql;
}


static bool isColumnInt( const TableColumnInfo &col )
{
  return col.type == "integer" || col.type == "smallint" || col.type == "bigint";
}

static bool isColumnDouble( const TableColumnInfo &col )
{
  return col.type == "real" || col.type == "double precision";
}

static bool isColumnText( const TableColumnInfo &col )
{
  return col.type == "text" || startsWith( col.type.dbType, "text(" ) ||
         col.type == "varchar" || startsWith( col.type.dbType, "varchar(" ) ||
         col.type == "character varying" || startsWith( col.type.dbType, "character varying(" ) ||
         col.type == "char" || col.type == "citext";
}

static bool isColumnGeometry( const TableColumnInfo &col )
{
  return col.isGeometry;
}

static Value resultToValue( const PostgresResult &res, int r, size_t i, const TableColumnInfo &col )
{
  Value v;
  if ( res.isNull( r, i ) )
  {
    v.setNull();
  }
  else
  {
    std::string valueStr = res.value( r, i );
    if ( col.type == "bool" || col.type == "boolean" )
    {
      v.setInt( valueStr == "t" );  // PostgreSQL uses 't' for true and 'f' for false
    }
    else if ( isColumnInt( col ) )
    {
      v.setInt( atol( valueStr.c_str() ) );
    }
    else if ( isColumnDouble( col ) )
    {
      v.setDouble( atof( valueStr.c_str() ) );
    }
    else if ( isColumnText( col ) )
    {
      v.setString( Value::TypeText, valueStr.c_str(), valueStr.size() );
    }
    else if ( col.type == "timestamp without time zone" || col.type == "date" )
    {
      v.setString( Value::TypeText, valueStr.c_str(), valueStr.size() );
    }
    else if ( isColumnGeometry( col ) )
    {
      // the value we get should have this format: "\x01234567890abcdef"
      if ( valueStr.size() < 2 )
        throw GeoDiffException( "Unexpected geometry value" );
      if ( valueStr[0] != '\\' || valueStr[1] != 'x' )
        throw GeoDiffException( "Unexpected prefix in geometry value" );

      // 1. convert from hex representation to proper binary stream
      std::string binString = hex2bin( valueStr.substr( 2 ) ); // chop \x prefix

      // 2. create binary header
      std::string binHead = createGpkgHeader( binString, col );

      // 3. copy header and body
      std::string gpb( binHead.size() + binString.size(), 0 );

      memcpy( &gpb[0], binHead.data(), binHead.size() );
      memcpy( &gpb[binHead.size()], binString.data(), binString.size() );

      v.setString( Value::TypeBlob, gpb.data(), gpb.size() );
    }
    else
    {
      // TODO: handling of other types (list, blob, ...)
      throw GeoDiffException( "unknown value type: " + col.type.dbType );
    }
  }
  return v;
}

static std::string valueToSql( const Value &v, const TableColumnInfo &col )
{
  if ( v.type() == Value::TypeUndefined )
  {
    throw GeoDiffException( "valueToSql: got 'undefined' value (malformed changeset?)" );
  }
  else if ( v.type() == Value::TypeNull )
  {
    return "NULL";
  }
  else if ( v.type() == Value::TypeInt )
  {
    if ( col.type == "boolean" )
      return v.getInt() ? "'t'" : "'f'";
    else
      return std::to_string( v.getInt() );
  }
  else if ( v.type() == Value::TypeDouble )
  {
    return to_string_with_max_precision( v.getDouble() );
  }
  else if ( v.type() == Value::TypeText || v.type() == Value::TypeBlob )
  {
    if ( col.isGeometry )
    {
      // handling of geometries - they are encoded with GPKG header
      std::string gpkgWkb = v.getString();
      int headerSize = parseGpkgbHeaderSize( gpkgWkb );
      std::string wkb( gpkgWkb.size() - headerSize, 0 );

      memcpy( &wkb[0], &gpkgWkb[headerSize], gpkgWkb.size() - headerSize );
      return "ST_GeomFromWKB('\\x" + bin2hex( wkb ) + "', " + std::to_string( col.geomSrsId ) + ")";
    }
    return quotedString( v.getString() );
  }
  else
  {
    throw GeoDiffException( "unexpected value" );
  }
}


static void handleInserted( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, bool reverse, PGconn *conn, ChangesetWriter &writer, bool &first )
{
  std::string sqlInserted = sqlFindInserted( schemaNameBase, schemaNameModified, tableName, tbl, reverse );
  PostgresResult res( execSql( conn, sqlInserted ) );

  int rows = res.rowCount();
  for ( int r = 0; r < rows; ++r )
  {
    if ( first )
    {
      ChangesetTable chTable = schemaToChangesetTable( tableName, tbl );
      writer.beginTable( chTable );
      first = false;
    }

    ChangesetEntry e;
    e.op = reverse ? ChangesetEntry::OpDelete : ChangesetEntry::OpInsert;

    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Value v( resultToValue( res, r, i, tbl.columns[i] ) );
      if ( reverse )
        e.oldValues.push_back( v );
      else
        e.newValues.push_back( v );
    }

    writer.writeEntry( e );
  }
}


static void handleUpdated( const std::string &schemaNameBase, const std::string &schemaNameModified, const std::string &tableName, const TableSchema &tbl, PGconn *conn, ChangesetWriter &writer, bool &first )
{
  std::string sqlModified = sqlFindModified( schemaNameBase, schemaNameModified, tableName, tbl );
  PostgresResult res( execSql( conn, sqlModified ) );

  int rows = res.rowCount();
  for ( int r = 0; r < rows; ++r )
  {
    if ( first )
    {
      ChangesetTable chTable = schemaToChangesetTable( tableName, tbl );
      writer.beginTable( chTable );
      first = false;
    }

    /*
    ** Within the old.* record associated with an UPDATE change, all fields
    ** associated with table columns that are not PRIMARY KEY columns and are
    ** not modified by the UPDATE change are set to "undefined". Other fields
    ** are set to the values that made up the row before the UPDATE that the
    ** change records took place. Within the new.* record, fields associated
    ** with table columns modified by the UPDATE change contain the new
    ** values. Fields associated with table columns that are not modified
    ** are set to "undefined".
    */

    ChangesetEntry e;
    e.op = ChangesetEntry::OpUpdate;

    size_t numColumns = tbl.columns.size();
    for ( size_t i = 0; i < numColumns; ++i )
    {
      Value v1( resultToValue( res, r, i + numColumns, tbl.columns[i] ) );
      Value v2( resultToValue( res, r, i, tbl.columns[i] ) );
      bool pkey = tbl.columns[i].isPrimaryKey;
      bool updated = v1 != v2;
      e.oldValues.push_back( ( pkey || updated ) ? v1 : Value() );
      e.newValues.push_back( updated ? v2 : Value() );
    }

    writer.writeEntry( e );
  }
}


void PostgresDriver::createChangeset( ChangesetWriter &writer )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  std::vector<std::string> tablesBase = listTables( false );
  std::vector<std::string> tablesModified = listTables( true );

  if ( tablesBase != tablesModified )
  {
    throw GeoDiffException( "Table names are not matching between the input databases.\n"
                            "Base:     " + concatNames( tablesBase ) + "\n" +
                            "Modified: " + concatNames( tablesModified ) );
  }

  for ( const std::string &tableName : tablesBase )
  {
    TableSchema tbl = tableSchema( tableName );
    TableSchema tblNew = tableSchema( tableName, true );

    // test that table schema in the modified is the same
    if ( tbl != tblNew )
    {
      if ( !tbl.compareWithBaseTypes( tblNew ) )
        throw GeoDiffException( "PostgreSQL Table schemas are not the same for table: " + tableName );
    }

    if ( !tbl.hasPrimaryKey() )
      continue;  // ignore tables without primary key - they can't be compared properly

    bool first = true;

    handleInserted( mBaseSchema, mModifiedSchema, tableName, tbl, false, mConn, writer, first );  // INSERT
    handleInserted( mBaseSchema, mModifiedSchema, tableName, tbl, true, mConn, writer, first );   // DELETE
    handleUpdated( mBaseSchema, mModifiedSchema, tableName, tbl, mConn, writer, first );          // UPDATE
  }
}


static std::string sqlForInsert( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &values )
{
  /*
   * For a table defined like this: CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
   *
   * INSERT INTO x (a, b, c, d) VALUES (?, ?, ?, ?)
   */

  std::string sql;
  sql += "INSERT INTO " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " (";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += quotedIdentifier( tbl.columns[i].name );
  }
  sql += ") VALUES (";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += ", ";
    sql += valueToSql( values[i], tbl.columns[i] );
  }
  sql += ")";
  return sql;
}


static std::string sqlForUpdate( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &oldValues, const std::vector<Value> &newValues )
{
  std::string sql;
  sql += "UPDATE " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " SET ";

  bool first = true;
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( newValues[i].type() != Value::TypeUndefined )
    {
      if ( !first )
        sql += ", ";
      first = false;
      sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( newValues[i], tbl.columns[i] );
    }
  }
  first = true;
  sql += " WHERE ";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( oldValues[i].type() != Value::TypeUndefined )
    {
      if ( !first )
        sql += " AND ";
      first = false;
      sql += quotedIdentifier( tbl.columns[i].name );
      if ( oldValues[i].type() == Value::TypeNull )
        sql += " IS NULL";
      else
        sql += " = " + valueToSql( oldValues[i], tbl.columns[i] );
    }
  }

  return sql;
}


static std::string sqlForDelete( const std::string &schemaName, const std::string &tableName, const TableSchema &tbl, const std::vector<Value> &values )
{
  std::string sql;
  sql += "DELETE FROM " + quotedIdentifier( schemaName ) + "." + quotedIdentifier( tableName ) + " WHERE ";
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( i > 0 )
      sql += " AND ";
    if ( tbl.columns[i].isPrimaryKey )
      sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( values[i], tbl.columns[i] );
    else
    {
      if ( values[i].type() == Value::TypeNull )
        sql += quotedIdentifier( tbl.columns[i].name ) + " IS NULL";
      else
        sql += quotedIdentifier( tbl.columns[i].name ) + " = " + valueToSql( values[i], tbl.columns[i] ) ;
    }
  }
  return sql;
}


void PostgresDriver::applyChangeset( ChangesetReader &reader )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  std::string lastTableName;
  TableSchema tbl;

  int autoIncrementPkeyIndex = -1;
  std::map<std::string, int64_t> autoIncrementTablesToFix;  // key = table name, value = max. value in changeset
  std::map<std::string, std::string> tableNameToSequenceName;  // key = table name, value = name of its pkey's sequence object

  // start a transaction, so that all changes get committed at once (or nothing get committed)
  PostgresTransaction transaction( mConn );

  int conflictCount = 0;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    std::string tableName = entry.table->name;

    if ( startsWith( tableName, "gpkg_" ) )
      continue;   // skip any changes to GPKG meta tables

    if ( tableName != lastTableName )
    {
      lastTableName = tableName;
      tbl = tableSchema( tableName );

      if ( tbl.columns.size() == 0 )
        throw GeoDiffException( "No such table: " + tableName );

      if ( tbl.columns.size() != entry.table->columnCount() )
        throw GeoDiffException( "Wrong number of columns for table: " + tableName );

      for ( size_t i = 0; i < entry.table->columnCount(); ++i )
      {
        if ( tbl.columns[i].isPrimaryKey != entry.table->primaryKeys[i] )
          throw GeoDiffException( "Mismatch of primary keys in table: " + tableName );
      }

      // if a table has auto-incrementing pkey (using SEQUENCE object), we may need
      // to update the sequence value after doing some inserts (or subsequent INSERTs would fail)
      std::string seqName = getSequenceObjectName( tbl, autoIncrementPkeyIndex );
      if ( autoIncrementPkeyIndex != -1 )
        tableNameToSequenceName[tableName] = seqName;
    }

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      std::string sql = sqlForInsert( mBaseSchema, tableName, tbl, entry.newValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        logApplyConflict( "insert_failed", entry );
        ++conflictCount;
        Logger::instance().warn( "Failure doing INSERT: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        throw GeoDiffException( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() );
      }

      if ( autoIncrementPkeyIndex != -1 )
      {
        int64_t pkey = entry.newValues[autoIncrementPkeyIndex].getInt();
        if ( autoIncrementTablesToFix.find( tableName ) == autoIncrementTablesToFix.end() )
          autoIncrementTablesToFix[tableName] = pkey;
        else
          autoIncrementTablesToFix[tableName] = std::max( autoIncrementTablesToFix[tableName], pkey );
      }
    }
    else if ( entry.op == ChangesetEntry::OpUpdate )
    {
      std::string sql = sqlForUpdate( mBaseSchema, tableName, tbl, entry.oldValues, entry.newValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        logApplyConflict( "update_failed", entry );
        ++conflictCount;
        Logger::instance().warn( "Failure doing UPDATE: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        logApplyConflict( "update_nothing", entry );
        ++conflictCount;
        Logger::instance().warn( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() + "\nSQL: " + sql );
      }
    }
    else if ( entry.op == ChangesetEntry::OpDelete )
    {
      std::string sql = sqlForDelete( mBaseSchema, tableName, tbl, entry.oldValues );
      PostgresResult res( execSql( mConn, sql ) );
      if ( res.status() != PGRES_COMMAND_OK )
      {
        logApplyConflict( "delete_failed", entry );
        ++conflictCount;
        Logger::instance().warn( "Failure doing DELETE: " + res.statusErrorMessage() );
      }
      if ( res.affectedRows() != "1" )
      {
        logApplyConflict( "delete_nothing", entry );
        Logger::instance().warn( "Wrong number of affected rows! Expected 1, got: " + res.affectedRows() );
      }
    }
    else
      throw GeoDiffException( "Unexpected operation" );
  }

  // at the end, update any SEQUENCE objects if needed
  for ( const std::pair<std::string, int64_t> &it : autoIncrementTablesToFix )
    updateSequenceObject( tableNameToSequenceName[it.first], it.second );

  if ( !conflictCount )
  {
    transaction.commitChanges();
  }
  else
  {
    throw GeoDiffException( "Conflicts encountered while applying changes! Total " + std::to_string( conflictCount ) );
  }
}

std::string PostgresDriver::getSequenceObjectName( const TableSchema &tbl, int &autoIncrementPkeyIndex )
{
  std::string colName;
  autoIncrementPkeyIndex = -1;
  for ( size_t i = 0; i < tbl.columns.size(); ++i )
  {
    if ( tbl.columns[i].isPrimaryKey && tbl.columns[i].isAutoIncrement )
    {
      autoIncrementPkeyIndex = i;
      colName = tbl.columns[i].name;
      break;
    }
  }

  if ( autoIncrementPkeyIndex == -1 )
    return "";

  std::string tableNameString = quotedIdentifier( mBaseSchema ) + "." + quotedIdentifier( tbl.name );
  std::string sql = "select pg_get_serial_sequence(" + quotedString( tableNameString ) + ", " + quotedString( colName ) + ")";
  PostgresResult resBase( execSql( mConn, sql ) );
  if ( resBase.rowCount() != 1 )
    throw GeoDiffException( "Unable to find sequence object for auto-incrementing pkey for table " + tbl.name );

  return resBase.value( 0, 0 );
}

void PostgresDriver::updateSequenceObject( const std::string &seqName, int64_t maxValue )
{
  PostgresResult resCurrVal( execSql( mConn, "SELECT last_value FROM " + seqName ) );
  std::string currValueStr = resCurrVal.value( 0, 0 );
  int currValue = std::stoi( currValueStr );

  if ( currValue < maxValue )
  {
    Logger::instance().info( "Updating sequence " + seqName + " from " + std::to_string( currValue ) + " to " + std::to_string( maxValue ) );

    std::string sql = "SELECT setval(" + quotedString( seqName ) + ", " + std::to_string( maxValue ) + ")";
    PostgresResult resSetVal( execSql( mConn, sql ) );
    // the SQL just returns the new value we set
  }
}

void PostgresDriver::createTables( const std::vector<TableSchema> &tables )
{
  for ( const TableSchema &tbl : tables )
  {
    if ( startsWith( tbl.name, "gpkg_" ) )
      continue;   // skip any changes to GPKG meta tables

    std::string sql, pkeyCols, columns;
    for ( const TableColumnInfo &c : tbl.columns )
    {
      if ( !columns.empty() )
        columns += ", ";

      std::string type = c.type.dbType;
      if ( c.isAutoIncrement )
        type = "SERIAL";   // there is also "smallserial", "bigserial" ...
      columns += quotedIdentifier( c.name ) + " " + type;

      if ( c.isNotNull )
        columns += " NOT NULL";

      if ( c.isPrimaryKey )
      {
        if ( !pkeyCols.empty() )
          pkeyCols += ", ";
        pkeyCols += quotedIdentifier( c.name );
      }
    }

    sql = "CREATE TABLE " + quotedIdentifier( mBaseSchema ) + "." + quotedIdentifier( tbl.name ) + " (";
    sql += columns;
    sql += ", PRIMARY KEY (" + pkeyCols + ")";
    sql += ");";

    PostgresResult res( execSql( mConn, sql ) );
    if ( res.status() != PGRES_COMMAND_OK )
      throw GeoDiffException( "Failure creating table: " + res.statusErrorMessage() );
  }
}


void PostgresDriver::dumpData( ChangesetWriter &writer, bool useModified )
{
  if ( !mConn )
    throw GeoDiffException( "Not connected to a database" );

  std::vector<std::string> tables = listTables();
  for ( const std::string &tableName : tables )
  {
    TableSchema tbl = tableSchema( tableName, useModified );
    if ( !tbl.hasPrimaryKey() )
      continue;  // ignore tables without primary key - they can't be compared properly

    std::string sql = "SELECT " + allColumnNames( tbl ) + " FROM " +
                      quotedIdentifier( useModified ? mModifiedSchema : mBaseSchema ) + "." + quotedIdentifier( tableName );

    PostgresResult res( execSql( mConn, sql ) );
    int rows = res.rowCount();
    for ( int r = 0; r < rows; ++r )
    {
      if ( r == 0 )
      {
        writer.beginTable( schemaToChangesetTable( tableName, tbl ) );
      }

      ChangesetEntry e;
      e.op = ChangesetEntry::OpInsert;
      size_t numColumns = tbl.columns.size();
      for ( size_t i = 0; i < numColumns; ++i )
      {
        e.newValues.push_back( Value( resultToValue( res, r, i, tbl.columns[i] ) ) );
      }
      writer.writeEntry( e );
    }
  }
}

void PostgresDriver::checkCompatibleForRebase( bool )
{
  throw GeoDiffException( "Rebase with postgres not supported yet" );
}
