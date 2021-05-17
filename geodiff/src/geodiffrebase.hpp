/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFREBASE_H
#define GEODIFFREBASE_H

#include <string>
#include <vector>
#include "geodiffutils.hpp"

int rebase(
  const std::string &changeset_BASE_THEIRS, //in
  const std::string &changeset_THEIRS_MODIFIED, // out
  const std::string &changeset_BASE_MODIFIED, //in
  std::vector<ConflictFeature> &conflicts// out
);


#endif // GEODIFFREBASE_H
