/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "tableschema.h"


bool TableSchema::hasPrimaryKey() const
{
  for ( const TableColumnInfo &c : columns )
  {
    if ( c.isPrimaryKey )
      return true;
  }
  return false;
}
