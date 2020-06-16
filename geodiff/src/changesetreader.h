/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETREADER_H
#define CHANGESETREADER_H


#include "geodiff.h"

#include "changeset.h"


class Buffer;

/**
 * Class for reading of binary changeset files.
 * First use open() to initialize it, followed by a series of nextEntry() calls.
 *
 * See changeset-format.md for the documentation of the format.
 */
class ChangesetReader
{
  public:
    ChangesetReader();
    ~ChangesetReader();

    //! Starts reading of changeset from a file
    bool open( const std::string &filename );

    //! Reads next changeset entry to the passed object
    bool nextEntry( ChangesetEntry &entry );

    //! Returns whether the changeset being read is completely empty
    bool isEmpty() const;

    //! Resets the reader position back to the start of the changeset
    void rewind();

  private:

    char readByte();
    int readVarint();
    std::string readNullTerminatedString();
    void readRowValues( std::vector<Value> &values );
    void readTableRecord();

    void throwReaderError( const std::string &message );

    int mOffset = 0;  // where are we in the buffer

    std::unique_ptr<Buffer> mBuffer;

    ChangesetTable mCurrentTable;  // currently processed table
};


#endif // CHANGESETREADER_H
