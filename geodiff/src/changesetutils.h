/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETUTILS_H
#define CHANGESETUTILS_H

#include "geodiff.h"
#include <string>

class ChangesetReader;
class ChangesetWriter;
struct ChangesetEntry;
struct ChangesetTable;
struct TableSchema;
struct Value;

ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl );

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );

std::string changesetEntryToJSON( const ChangesetEntry &entry );

std::string changesetToJSON( ChangesetReader &reader );

std::string changesetToJSONSummary( ChangesetReader &reader );

std::string valueToJSON( const Value &value );

std::string hex2bin( const std::string &str );
std::string bin2hex( const std::string &str );

#endif // CHANGESETUTILS_H
