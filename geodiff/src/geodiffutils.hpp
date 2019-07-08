/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFUTILS_H
#define GEODIFFUTILS_H

#include <string>
#include "sqlite3.h"

/*
 * TODO remove
** Dynamic string object
*/
typedef struct Str Str;
struct Str
{
  char *z;        /* Text of the string */
  int nAlloc;     /* Bytes allocated in z[] */
  int nUsed;      /* Bytes actually used in z[] */
};

/*
** Initialize a Str object
* TODO remove
*/
void strInit( Str *p );

/*
** Free all memory held by a Str object
* TODO remove
*/
void strFree( Str *p );

/*
** Add formatted text to the end of a Str object
**
** TODO remove
*/
void strPrintf( Str *p, const char *zFormat, ... );

std::string pOpToStr( int pOp );
std::string conflict2Str( int c );
int changesetIter2Str( sqlite3_changeset_iter *pp );
void errorLogCallback( void *pArg, int iErrCode, const char *zMsg );

/*
** Prepare a new SQL statement.  Print an error and abort if anything
** goes wrong.
*/
sqlite3_stmt *db_prepare( sqlite3 *db, const char *zFormat, ... );

/*
** Return the text of an SQL statement that itself returns the list of
** tables to process within the database.
*/
const char *all_tables_sql();

// copy file from to location. override if exists
void cp( const std::string &to, const std::string &from );

std::string sqlite_value_2str( sqlite3_value *ppValue );


/**
 * Reads a file content to the string
 * https://stackoverflow.com/questions/3747086/reading-the-whole-text-file-into-a-char-array-in-c
 * TODO write in C++
 */
long slurp( char const *path, char **buf );


// WRITE CHANGESET API

/*
** Write an SQLite value onto out.
*/
void putValue( FILE *out, sqlite3_value *ppValue );
void putValue( FILE *out, int ppValue );

/*
** Write a 64-bit signed integer as a varint onto out
** Copied from sqldiff.c
*/
void putsVarint( FILE *out, sqlite3_uint64 v );


#endif // GEODIFFUTILS_H
