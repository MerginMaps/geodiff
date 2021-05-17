/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETUTILS_H
#define CHANGESETUTILS_H

#include "geodiff.h"
#include <string>
#include <vector>

class ConflictFeature;
class ChangesetReader;
class ChangesetWriter;
struct ChangesetEntry;
struct ChangesetTable;
struct TableSchema;
struct Value;

ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl );

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );

void concatChangesets( const std::vector<std::string> &filenames, const std::string &outputChangeset );

std::string changesetEntryToJSON( const ChangesetEntry &entry );

std::string changesetToJSON( ChangesetReader &reader );

std::string changesetToJSONSummary( ChangesetReader &reader );

std::string conflictsToJSON( const std::vector<ConflictFeature> &conflicts );

std::string valueToJSON( const Value &value );

std::string hex2bin( const std::string &str );
std::string bin2hex( const std::string &str );

#endif // CHANGESETUTILS_H
