/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFSQLITEREBASE_H
#define GEODIFFSQLITEREBASE_H

#include <string>

int rebase(
  const std::string &changeset_BASE_THEIRS, //in
  const std::string &changeset_THEIRS_MODIFIED, // out
  const std::string &changeset_BASE_MODIFIED //in
);

#endif // GEODIFFSQLITEREBASE_H
