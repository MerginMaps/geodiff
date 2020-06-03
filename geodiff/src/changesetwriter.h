/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETWRITER_H
#define CHANGESETWRITER_H

#include "geodiff.h"

#include "changeset.h"

#include <fstream>

/**
 * Class for writing binary changeset files.
 * First use open() to create a new changeset file and then for each modified table:
 * - call beginTable() once
 * - then call writeEntry() for each change within that table
 *
 * See changeset-format.md for the documentation of the format.
 */
class ChangesetWriter
{
  public:

    //! opens a file for writing changeset (will overwrite if it exists already)
    bool open( const std::string &filename );

    //! writes table information, all subsequent writes will be related to this table until next call to beginTable()
    void beginTable( const ChangesetTable &table );

    //! writes table change entry
    void writeEntry( const ChangesetEntry &entry );

  private:

    void writeByte( char c );
    void writeVarint( int n );
    void writeNullTerminatedString( const std::string &str );

    void writeRowValues( const std::vector<Value> &values );

    std::ofstream mFile;

    ChangesetTable mCurrentTable;  // currently processed table
};

#endif // CHANGESETWRITER_H
