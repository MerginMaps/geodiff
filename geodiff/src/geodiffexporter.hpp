/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFEXPORTER_H
#define GEODIFFEXPORTER_H

#include <string>
#include <memory>
#include <exception>
#include <vector>

#include "sqlite3.h"
#include "geodiffutils.hpp"

namespace GeoDiffExporter
{
  std::string toString( sqlite3_changeset_iter *pp );
  std::string toJSON( std::shared_ptr<Sqlite3Db> db, Buffer &buf );
  std::string toJSON( std::shared_ptr<Sqlite3Db> db, Sqlite3ChangesetIter &pp );

  std::string toJSONSummary( Buffer &buf );
}

#endif // GEODIFFEXPORTER_H
