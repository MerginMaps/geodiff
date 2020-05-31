/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetutils.h"

#include "geodiffutils.hpp"
#include "changesetreader.h"
#include "changesetwriter.h"


void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer )
{
  std::string currentTableName;
  std::vector<bool> currentPkeys;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
  {
    if ( entry.table->name != currentTableName )
    {
      writer.beginTable( *entry.table );
      currentTableName = entry.table->name;
      currentPkeys = entry.table->primaryKeys;
    }

    if ( entry.op == ChangesetEntry::OpInsert )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpDelete;
      out.oldValues = entry.newValues;
      writer.writeEntry( out );
    }
    else if ( entry.op == ChangesetEntry::OpDelete )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpInsert;
      out.newValues = entry.oldValues;
      writer.writeEntry( out );
    }
    else if ( entry.op == ChangesetEntry::OpUpdate )
    {
      ChangesetEntry out;
      out.op = ChangesetEntry::OpUpdate;
      out.newValues = entry.oldValues;
      out.oldValues = entry.newValues;
      // if a column is a part of pkey and has not been changed,
      // the original entry has "old" value the pkey value and "new"
      // value is undefined - let's reverse "old" and "new" in that case.
      for ( size_t i = 0; i < currentPkeys.size(); ++i )
      {
        if ( currentPkeys[i] && out.oldValues[i].type() == Value::TypeUndefined )
        {
          out.oldValues[i] = out.newValues[i];
          out.newValues[i].setUndefined();
        }
      }
      writer.writeEntry( out );
    }
    else
    {
      throw GeoDiffException("Unknown entry operation!");
    }
  }
}
