/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef TABLESCHEMA_H
#define TABLESCHEMA_H

#include <string>
#include <vector>


/** Information about a single column of a database table */
struct TableColumnInfo
{
  //! Unique name of the column
  std::string name;
  //! Type of the column as reported by the database
  std::string type;
  //! Whether this column is a part of the table's primary key
  bool isPrimaryKey;

  bool operator==( const TableColumnInfo &other ) const
  {
    return name == other.name && type == other.type && isPrimaryKey == other.isPrimaryKey;
  }
  bool operator!=( const TableColumnInfo &other ) const
  {
    return !( *this == other );
  }
};

/** Information about table schema of a database table */
struct TableSchema
{
  std::vector<TableColumnInfo> columns;

  bool operator==( const TableSchema &other ) const
  {
    return columns == other.columns;
  }
  bool operator!=( const TableSchema &other ) const
  {
    return !( *this == other );
  }
};

#endif // TABLESCHEMA_H
