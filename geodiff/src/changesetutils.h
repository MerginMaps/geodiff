/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETUTILS_H
#define CHANGESETUTILS_H

class ChangesetReader;
class ChangesetWriter;

#include "geodiff.h"

GEODIFF_EXPORT void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );


#endif // CHANGESETUTILS_H
