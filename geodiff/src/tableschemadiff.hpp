/*
 GEODIFF - MIT License
 Copyright (C) 2026 David Koňařík
*/

#ifndef TABLESCHEMADIFF_H
#define TABLESCHEMADIFF_H

#include "changeset.h"
#include "tableschema.h"

std::vector<ChangesetEntry> diffTableSchema( const TableSchema &base, const TableSchema &modified );
std::vector<ChangesetEntry> diffDatabaseSchema( const DatabaseSchema &base, const DatabaseSchema &modified );

#endif // TABLESCHEMADIFF_H
