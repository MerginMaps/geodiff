/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef TABLESCHEMA_H
#define TABLESCHEMA_H

#include <string>
#include <vector>
#include <algorithm>

#include "geodiffutils.hpp"

/* Information about column type, converted to base type */
struct TableColumnType
{
  enum BaseType
  {
    TEXT = 0,
    INTEGER,
    DOUBLE,
    BOOLEAN,
    BLOB,
    GEOMETRY,
    DATE,
    DATETIME
  };

  BaseType baseType = TEXT;
  std::string dbType;

  //! Returns string representation of the base type enum value
  static std::string baseTypeToString( BaseType t );

  //! Unified conversion of GeoPackage and PostgreSQL types to base types
  void convertToBaseType();

  bool operator==( const std::string &other ) const
  {
    return ( dbType == other );
  }

  bool operator!=( const std::string &other ) const
  {
    return !( *this == other );
  }

  bool operator==( const TableColumnType &other ) const
  {
    return ( dbType == other.dbType );
  }

  bool operator!=( const TableColumnType &other ) const
  {
    return !( *this == other );
  }

  bool operator==( const TableColumnType::BaseType &other ) const
  {
    return ( baseType == other );
  }

  bool operator!=( const TableColumnType::BaseType &other ) const
  {
    return !( *this == other );
  }
};

/** Information about a single column of a database table */
struct TableColumnInfo
{
  //! Unique name of the column
  std::string name;
  //! Type of the column as reported by the database
  TableColumnType type;
  //! Whether this column is a part of the table's primary key
  bool isPrimaryKey = false;

  //! Whether the column is defined as "NOT NULL" - i.e. null values are not allowed
  bool isNotNull = false;
  //! Whether the column has a default value assigned as an auto-incrementing
  bool isAutoIncrement = false;

  //! Whether the column encodes geometry data
  bool isGeometry = false;
  //! In case of geometry column - contains geometry type (e.g. POINT / LINESTRING / POLYGON / ...)
  //! Only "flat" types are allowed - without Z / M - these are stored separately in geomHasZ, geomHasM
  std::string geomType;
  //! In case of geometry column - contains ID of the spatial ref. system
  int geomSrsId = -1;
  //! Whether the geometry column includes Z coordinates
  bool geomHasZ = false;
  //! Whether the geometry column includes M coordinates
  bool geomHasM = false;

  bool compareWithBaseTypes( TableColumnInfo other ) const
  {
    return name == other.name && type == other.type.baseType && isPrimaryKey == other.isPrimaryKey &&
           isNotNull == other.isNotNull && isAutoIncrement == other.isAutoIncrement &&
           isGeometry == other.isGeometry && geomType == other.geomType && geomSrsId == other.geomSrsId &&
           geomHasZ == other.geomHasZ && geomHasM == other.geomHasM;
  }

  std::string dump() const
  {
    std::string output = name + " | " + type.dbType + " | ";
    if ( isPrimaryKey )
      output += "pkey ";
    if ( isNotNull )
      output += "notnull ";
    if ( isAutoIncrement )
      output += "autoincrement";
    if ( isGeometry )
    {
      output += "geometry:" + geomType + ":" + std::to_string( geomSrsId );
      if ( geomHasZ )
        output += "hasZ";
      if ( geomHasM )
        output += "hasM";
    }
    return output;
  }

  void setGeometry( const std::string &geomTypeName, int srsId, bool hasM, bool hasZ )
  {
    type.baseType = TableColumnType::GEOMETRY;
    isGeometry = true;
    geomType = geomTypeName;
    geomSrsId = srsId;
    geomHasM = hasM;
    geomHasZ = hasZ;
  }

  bool operator==( const TableColumnInfo &other ) const
  {
    return name == other.name && type == other.type && isPrimaryKey == other.isPrimaryKey &&
           isNotNull == other.isNotNull && isAutoIncrement == other.isAutoIncrement &&
           isGeometry == other.isGeometry && geomType == other.geomType && geomSrsId == other.geomSrsId &&
           geomHasZ == other.geomHasZ && geomHasM == other.geomHasM;
  }

  bool operator!=( const TableColumnInfo &other ) const
  {
    return !( *this == other );
  }

};


/** Definition of a coordinate reference system - may be needed when creating tables */
struct CrsDefinition
{
  int srsId = 0;            //!< Identifier of the CRS within the database
  std::string authName;     //!< Name of the authority (usually "EPSG")
  int authCode = 0;         //!< Code of the CRS within authority
  std::string wkt;          //!< Definition in form of WKT string

  bool operator==( const CrsDefinition &other ) const
  {
    // TODO: We should also compare crs in future, however, the format may vary even for the same object
    // and that makes it hard to compare just with string compare
    return srsId == other.srsId && authName == other.authName && authCode == other.authCode;
  }
  bool operator!=( const CrsDefinition &other ) const
  {
    return !( *this == other );
  }

};


/** Information about table's spatial extent */
struct Extent
{
  Extent( double _minX = 0, double _minY = 0, double _maxX = 0, double _maxY = 0 )
    : minX( _minX ), minY( _minY ), maxX( _maxX ), maxY( _maxY ) {}

  double minX = 0, minY = 0, maxX = 0, maxY = 0;
};


/** Information about table schema of a database table */
struct TableSchema
{
  std::string name;
  std::vector<TableColumnInfo> columns;
  CrsDefinition crs;

  //! Returns true if at least one column is a part of table's primary key
  bool hasPrimaryKey() const;

  //! Returns column index for the given column name (returns SIZE_MAX if column not is not found)
  size_t columnFromName( const std::string &name );

  //! Returns index of the first encountered geometry column (returns SIZE_MAX if no geometry column is found)
  size_t geometryColumn() const;

  std::string dump() const
  {
    std::string output = "TABLE " + name + "\n";
    for ( const TableColumnInfo &col : columns )
      output += "  " + col.dump() + "\n";
    return output;
  }

  bool compareWithBaseTypes( const TableSchema &other ) const
  {
    return name == other.name &&
           crs == other.crs && columns.size() == other.columns.size() &&
           std::equal( columns.begin(), columns.end(), other.columns.begin(),
    []( TableColumnInfo me, TableColumnInfo other ) { return me.compareWithBaseTypes( other ); } );
  }

  bool operator==( const TableSchema &other ) const
  {
    return name == other.name && columns == other.columns && crs == other.crs;
  }
  bool operator!=( const TableSchema &other ) const
  {
    return !( *this == other );
  }
};

//! Converts column name to base type and returns struct with both names
TableColumnType columnType( const std::string &columnType, const std::string &driverName, bool isGeometry = false );

//! Converts table schema from base to destination driver, raises GeoDiffException if that is not supported
void tableSchemaConvert( const std::string &driverDstName, TableSchema &tbl );

#endif // TABLESCHEMA_H
