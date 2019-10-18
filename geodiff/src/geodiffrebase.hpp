/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFREBASE_H
#define GEODIFFREBASE_H

#include <string>

int rebase(
  const std::string &changeset_BASE_THEIRS, //in
  const std::string &changeset_THEIRS_MODIFIED, // out
  const std::string &changeset_BASE_MODIFIED //in
);

// true on error
bool concatChangesets( const std::string &A, const std::string &B, const std::string &C, const std::string &out );


#endif // GEODIFFREBASE_H
